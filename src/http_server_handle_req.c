/**
 * @file http_server_handle_req.c
 * @author TheSomeMan
 * @date 2021-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_handle_req.h"
#include <assert.h>
#include <string.h>
#include "str_buf.h"
#include "os_malloc.h"
#include "cJSON.h"
#include "wifi_manager.h"
#include "wifi_manager_internal.h"
#include "wifiman_msg.h"
#include "wifiman_config.h"
#include "json_access_points.h"
#include "json_network_info.h"
#include "http_server.h"
#include "http_server_auth.h"
#include "http_server_handle_req_get_auth.h"
#include "http_server_handle_req_post_auth.h"
#include "http_server_handle_req_delete_auth.h"
#include "http_server_ecdh.h"
#include "dns_server.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG) && !RUUVI_TESTS
#warning Debug log level prints out the passwords as a "plaintext".
#endif

static const char TAG[] = "http_server";

typedef struct http_server_gen_resp_status_json_param_t
{
    http_server_resp_t* const p_http_resp;
    bool                      flag_access_from_lan;
} http_server_gen_resp_status_json_param_t;

typedef struct wifi_ssid_password_t
{
    bool                    is_ssid_null;
    bool                    is_password_null;
    wifiman_wifi_ssid_t     ssid;
    wifiman_wifi_password_t password;
} wifi_ssid_password_t;

static http_server_resp_status_json_t g_resp_status_json;

static void
http_server_gen_resp_status_json(const json_network_info_t* const p_info, void* const p_param)
{
    http_server_gen_resp_status_json_param_t* p_params = p_param;
    if (NULL == p_info)
    {
        LOG_ERR("http_server_netconn_serve: GET /status.json failed to obtain mutex");
        LOG_DBG("status.json: 503");
        *p_params->p_http_resp = http_server_resp_503();
    }
    else
    {
        json_network_info_do_generate_internal(p_info, &g_resp_status_json);
        LOG_DBG("status.json: %s", g_resp_status_json.buf);
        *p_params->p_http_resp = http_server_resp_200_json(g_resp_status_json.buf);
    }
}

static http_server_resp_t
http_server_handle_req_get(
    const char* const                           p_file_name_unchecked,
    const http_server_handle_req_param_t* const p_param,
    http_header_extra_fields_t* const           p_extra_header_fields)
{
    const char* const       p_uri_params = p_param->p_req_info->http_uri_params.ptr;
    const http_req_header_t http_header  = p_param->p_req_info->http_header;

    LOG_DBG("http_server_handle_req_get /%s", p_file_name_unchecked);

    const char* const p_file_name = (0 == strcmp(p_file_name_unchecked, "")) ? "index.html" : p_file_name_unchecked;

    const wifiman_hostinfo_t host_info = wifiman_config_sta_get_hostinfo();

    if (0 == strcmp(p_file_name, "auth"))
    {
        const http_server_handle_req_auth_param_t param = {
            .flag_access_from_lan                   = p_param->flag_access_from_lan,
            .flag_check_rw_access_with_bearer_token = false,
            .http_header                            = http_header,
            .p_remote_ip                            = p_param->p_remote_ip,
            .p_auth_info                            = p_param->p_auth_info,
            .p_hostinfo                             = &host_info,
        };

        return http_server_handle_req_get_auth(&param, p_extra_header_fields);
    }

    bool flag_check_rw_access_with_bearer_token = false;
    if (0 == strcmp(p_file_name, "ap.json"))
    {
        flag_check_rw_access_with_bearer_token = true;
    }

    const char* const p_file_ext = strrchr(p_file_name, '.');
    if ((NULL == p_file_ext) || ((NULL != p_file_ext) && (0 == strcmp(p_file_ext, ".json"))))
    {
        bool flag_access_by_bearer_token = false;

        const http_server_handle_req_auth_param_t param = {
            .flag_access_from_lan                   = p_param->flag_access_from_lan,
            .flag_check_rw_access_with_bearer_token = flag_check_rw_access_with_bearer_token,
            .http_header                            = http_header,
            .p_remote_ip                            = p_param->p_remote_ip,
            .p_auth_info                            = p_param->p_auth_info,
            .p_hostinfo                             = &host_info,
        };

        const http_server_resp_t resp_auth_check = http_server_handle_req_check_auth(
            &param,
            p_extra_header_fields,
            &flag_access_by_bearer_token);
        if ((!flag_access_by_bearer_token) && (HTTP_RESP_CODE_401 == resp_auth_check.http_resp_code)
            && ((HTTP_SERVER_AUTH_TYPE_RUUVI == p_param->p_auth_info->auth_type)
                || (HTTP_SERVER_AUTH_TYPE_DEFAULT == p_param->p_auth_info->auth_type)))
        {
            if ((0 != strcmp(p_file_name, "ap.json")) && (0 != strcmp(p_file_name, "status.json")))
            {
                (void)snprintf(
                    p_extra_header_fields->buf,
                    sizeof(p_extra_header_fields->buf),
                    "Set-Cookie: %s=/%s",
                    HTTP_SERVER_AUTH_RUUVI_COOKIE_PREV_URL,
                    p_file_name);
            }
            return http_server_resp_302();
        }
        if (HTTP_RESP_CODE_200 != resp_auth_check.http_resp_code)
        {
            return resp_auth_check;
        }
    }

    if (0 == strcmp(p_file_name, "ap.json"))
    {
        const char* const p_buff = wifi_manager_scan_sync();
        if (NULL == p_buff)
        {
            LOG_ERR("GET /ap.json: failed to get json, return HTTP error 503");
            return http_server_resp_503();
        }
        LOG_INFO("ap.json: %s", p_buff);
        return http_server_resp_200_json_in_heap(p_buff);
    }

    if (0 == strcmp(p_file_name, "status.json"))
    {
        http_server_resp_t                       http_resp = { 0 };
        http_server_gen_resp_status_json_param_t params    = {
               .p_http_resp          = &http_resp,
               .flag_access_from_lan = p_param->flag_access_from_lan,
        };
        const os_delta_ticks_t ticks_to_wait = 10U;
        json_network_info_do_const_action_with_timeout(&http_server_gen_resp_status_json, &params, ticks_to_wait);
        wifi_manager_cb_on_request_status_json();
        return http_resp;
    }

    return wifi_manager_cb_on_http_get(p_file_name, p_uri_params, p_param->flag_access_from_lan, NULL);
}

static http_server_resp_t
http_server_handle_req_delete(
    const char* const                           p_file_name,
    const http_server_handle_req_param_t* const p_param,
    http_header_extra_fields_t* const           p_extra_header_fields)
{
    const char* const       p_uri_params = p_param->p_req_info->http_uri_params.ptr;
    const http_req_header_t http_header  = p_param->p_req_info->http_header;

    LOG_INFO("DELETE /%s, params=%s", p_file_name, (NULL != p_uri_params) ? p_uri_params : "");
    const wifiman_hostinfo_t host_info = wifiman_config_sta_get_hostinfo();

    bool flag_access_by_bearer_token = false;

    const http_server_handle_req_auth_param_t param = {
        .flag_access_from_lan                   = p_param->flag_access_from_lan,
        .flag_check_rw_access_with_bearer_token = true,
        .http_header                            = http_header,
        .p_remote_ip                            = p_param->p_remote_ip,
        .p_auth_info                            = p_param->p_auth_info,
        .p_hostinfo                             = &host_info,
    };

    const http_server_resp_t resp_auth_check = http_server_handle_req_check_auth(
        &param,
        p_extra_header_fields,
        &flag_access_by_bearer_token);

    if (HTTP_RESP_CODE_200 != resp_auth_check.http_resp_code)
    {
        return resp_auth_check;
    }

    if (0 == strcmp(p_file_name, "auth"))
    {
        return http_server_handle_req_delete_auth(http_header, p_param->p_remote_ip, p_param->p_auth_info, &host_info);
    }
    if (0 == strcmp(p_file_name, "connect.json"))
    {
        LOG_INFO("http_server_netconn_serve: DELETE /connect.json");
        dns_server_stop();
        if (wifi_manager_is_connected_to_ethernet())
        {
            wifi_manager_disconnect_eth();
        }
        else
        {
            /* request a disconnection from Wi-Fi */
            wifi_manager_disconnect_wifi();
        }
        return http_server_resp_200_json("{}");
    }
    return wifi_manager_cb_on_http_delete(p_file_name, p_uri_params, p_param->flag_access_from_lan, NULL);
}

static const char*
http_server_json_get_string_val_ptr(const cJSON* const p_json_root, const char* const p_attr_name)
{
    cJSON* const p_json_attr = cJSON_GetObjectItem(p_json_root, p_attr_name);
    if (NULL == p_json_attr)
    {
        return NULL;
    }
    return cJSON_GetStringValue(p_json_attr);
}

static bool
http_server_handle_ruuvi_ecdh_pub_key(
    const char* const                     p_ruuvi_ecdh_pub_key,
    const uint32_t                        len_ruuvi_ecdh_pub_key,
    http_server_ecdh_pub_key_b64_t* const p_pub_key_b64_srv)
{
    LOG_INFO("Found Ruuvi-Ecdh-Pub-Key: %.*s", (printf_int_t)len_ruuvi_ecdh_pub_key, p_ruuvi_ecdh_pub_key);

    http_server_ecdh_pub_key_b64_t pub_key_b64_cli = { 0 };
    if (len_ruuvi_ecdh_pub_key >= sizeof(pub_key_b64_cli.buf))
    {
        LOG_ERR(
            "Length of ecdh_pub_key_b64 (%u) is longer than expected (%u)",
            len_ruuvi_ecdh_pub_key,
            sizeof(pub_key_b64_cli.buf));
        return false;
    }

    (void)snprintf(
        pub_key_b64_cli.buf,
        sizeof(pub_key_b64_cli.buf),
        "%.*s",
        len_ruuvi_ecdh_pub_key,
        p_ruuvi_ecdh_pub_key);
    if (!http_server_ecdh_handshake(&pub_key_b64_cli, p_pub_key_b64_srv))
    {
        LOG_ERR("%s failed", "http_server_ecdh_handshake");
        return false;
    }
    return true;
}

static bool
http_server_parse_encrypted_req(const cJSON* const p_json_root, http_server_ecdh_encrypted_req_t* const p_enc_req)
{
    p_enc_req->p_encrypted = http_server_json_get_string_val_ptr(p_json_root, "encrypted");
    if (NULL == p_enc_req->p_encrypted)
    {
        return false;
    }
    p_enc_req->p_iv = http_server_json_get_string_val_ptr(p_json_root, "iv");
    if (NULL == p_enc_req->p_iv)
    {
        return false;
    }
    p_enc_req->p_hash = http_server_json_get_string_val_ptr(p_json_root, "hash");
    if (NULL == p_enc_req->p_hash)
    {
        return false;
    }
    return true;
}

bool
http_server_decrypt(const char* const p_http_body, str_buf_t* const p_str_buf)
{
    cJSON* p_json_root = cJSON_Parse(p_http_body);
    if (NULL == p_json_root)
    {
        LOG_ERR("Failed to parse json or no memory");
        return false;
    }
    http_server_ecdh_encrypted_req_t enc_req = { 0 };
    if (!http_server_parse_encrypted_req(p_json_root, &enc_req))
    {
        LOG_ERR("Failed to parse encrypted request");
        cJSON_Delete(p_json_root);
        return false;
    }
    if (!http_server_ecdh_decrypt(&enc_req, p_str_buf))
    {
        LOG_ERR("Failed to decrypt request");
        cJSON_Delete(p_json_root);
        return false;
    }
    cJSON_Delete(p_json_root);
    LOG_DBG("Decrypted: %s", p_str_buf->buf);
    return true;
}

bool
http_server_decrypt_by_params(
    const char* const p_encrypted_val,
    const char* const p_iv,
    const char* const p_hash,
    str_buf_t* const  p_str_buf)
{
    http_server_ecdh_encrypted_req_t enc_req = {
        .p_encrypted = p_encrypted_val,
        .p_iv        = p_iv,
        .p_hash      = p_hash,
    };
    if (!http_server_ecdh_decrypt(&enc_req, p_str_buf))
    {
        LOG_ERR("Failed to decrypt request");
        return false;
    }
    return true;
}

static bool
http_server_parse_cjson_wifi_ssid_password(const cJSON* const p_json_root, wifi_ssid_password_t* const p_info)
{
    cJSON* const p_json_attr_ssid = cJSON_GetObjectItem(p_json_root, "ssid");
    if (NULL == p_json_attr_ssid)
    {
        LOG_ERR("connect.json: Can't find attribute 'ssid'");
        return false;
    }
    const char*  p_ssid               = cJSON_GetStringValue(p_json_attr_ssid);
    cJSON* const p_json_attr_password = cJSON_GetObjectItem(p_json_root, "password");
    if (NULL == p_json_attr_password)
    {
        LOG_ERR("connect.json: Can't find attribute 'password'");
        return false;
    }
    const char* p_password = cJSON_GetStringValue(p_json_attr_password);

    if (NULL == p_ssid)
    {
        p_info->is_ssid_null     = true;
        p_info->ssid.ssid_buf[0] = '\0';
    }
    else
    {
        p_info->is_ssid_null = false;
        if (strlen(p_ssid) >= sizeof(p_info->ssid))
        {
            LOG_ERR("connect.json: SSID is too long");
            return false;
        }
        (void)snprintf(p_info->ssid.ssid_buf, sizeof(p_info->ssid.ssid_buf), "%s", p_ssid);
    }

    if (NULL == p_password)
    {
        p_info->is_password_null         = true;
        p_info->password.password_buf[0] = '\0';
    }
    else
    {
        p_info->is_password_null = false;
        if (strlen(p_password) >= sizeof(p_info->password))
        {
            LOG_ERR("connect.json: password is too long");
            return false;
        }
        (void)snprintf(p_info->password.password_buf, sizeof(p_info->password.password_buf), "%s", p_password);
    }
    return true;
}

static bool
http_server_parse_json_wifi_ssid_password(const char* const p_json, wifi_ssid_password_t* const p_info)
{
    cJSON* p_json_root = cJSON_Parse(p_json);
    if (NULL == p_json_root)
    {
        LOG_ERR("connect.json: Failed to parse decrypted content or no memory");
        return false;
    }
    const bool res = http_server_parse_cjson_wifi_ssid_password(p_json_root, p_info);
    cJSON_Delete(p_json_root);
    return res;
}

static http_server_resp_t
http_server_handle_req_post_connect_json(const http_req_body_t http_body)
{
    LOG_INFO("http_server_netconn_serve: POST /connect.json");
    LOG_DBG("http_server_netconn_serve: %s", http_body.ptr);
    wifi_ssid_password_t login_info = { 0 };
    if (!http_server_parse_json_wifi_ssid_password(http_body.ptr, &login_info))
    {
        return http_server_resp_400();
    }

    if (login_info.is_ssid_null && login_info.is_password_null)
    {
        LOG_INFO("POST /connect.json: SSID:NULL, PWD: NULL - connect to Ethernet");
        wifiman_msg_send_cmd_connect_eth();
        return http_server_resp_200_json("{}");
    }
    if (!login_info.is_ssid_null)
    {
        if (login_info.is_password_null)
        {
            const wifiman_wifi_ssid_t saved_ssid = wifiman_config_sta_get_ssid();
            if (0 == strcmp(saved_ssid.ssid_buf, login_info.ssid.ssid_buf))
            {
                LOG_INFO("POST /connect.json: SSID:%s, PWD: NULL - reconnect to saved WiFi", login_info.ssid.ssid_buf);
            }
            else
            {
                LOG_WARN(
                    "POST /connect.json: SSID:%s, PWD: NULL - try to connect to WiFi without authentication",
                    login_info.ssid.ssid_buf);
                wifiman_config_sta_set_ssid_and_password(&login_info.ssid, NULL);
            }
            LOG_DBG("http_server_netconn_serve: wifi_manager_connect_async() call");
            wifi_manager_start_timer_reconnect_sta_after_timeout();
            return http_server_resp_200_json("{}");
        }
        LOG_DBG(
            "POST /connect.json: SSID:%s, PWD: %s - connect to WiFi",
            login_info.ssid.ssid_buf,
            login_info.password.password_buf);
        LOG_INFO("POST /connect.json: SSID:%s, PWD: ******** - connect to WiFi", login_info.ssid.ssid_buf);
        wifiman_config_sta_set_ssid_and_password(&login_info.ssid, &login_info.password);

        LOG_DBG("http_server_netconn_serve: wifi_manager_connect_async() call");
        wifi_manager_connect_async();
        return http_server_resp_200_json("{}");
    }
    /* bad request the authentication header is not complete/not the correct format */
    return http_server_resp_400();
}

static http_server_resp_t
http_server_handle_req_post(
    const char*                                 p_file_name,
    const http_server_handle_req_param_t* const p_param,
    const http_req_body_t                       http_body,
    http_header_extra_fields_t* const           p_extra_header_fields)
{
    const char*             p_uri_params = p_param->p_req_info->http_uri_params.ptr;
    const http_req_header_t http_header  = p_param->p_req_info->http_header;

    LOG_INFO("POST /%s, params=%s", p_file_name, (NULL != p_uri_params) ? p_uri_params : "");

    const wifiman_hostinfo_t hostinfo = wifiman_config_sta_get_hostinfo();

    if (0 == strcmp(p_file_name, "auth"))
    {
        return http_server_handle_req_post_auth(
            p_param->flag_access_from_lan,
            http_header,
            p_param->p_remote_ip,
            http_body,
            p_param->p_auth_info,
            &hostinfo,
            p_extra_header_fields);
    }

    bool flag_access_by_bearer_token = false;

    const http_server_handle_req_auth_param_t param = {
        .flag_access_from_lan                   = p_param->flag_access_from_lan,
        .flag_check_rw_access_with_bearer_token = true,
        .http_header                            = http_header,
        .p_remote_ip                            = p_param->p_remote_ip,
        .p_auth_info                            = p_param->p_auth_info,
        .p_hostinfo                             = &hostinfo,
    };

    const http_server_resp_t resp_auth_check = http_server_handle_req_check_auth(
        &param,
        p_extra_header_fields,
        &flag_access_by_bearer_token);

    if (HTTP_RESP_CODE_200 != resp_auth_check.http_resp_code)
    {
        return resp_auth_check;
    }

    if (0 == strcmp(p_file_name, "connect.json"))
    {
        return http_server_handle_req_post_connect_json(http_body);
    }
    return wifi_manager_cb_on_http_post(p_file_name, p_uri_params, http_body, p_param->flag_access_from_lan);
}

static http_server_resp_t
http_server_handle_req_get_with_ecdh_key(
    const char* const                           p_path,
    const http_server_handle_req_param_t* const p_param,
    http_header_extra_fields_t* const           p_extra_header_fields)
{
    http_server_resp_t resp = http_server_handle_req_get(p_path, p_param, p_extra_header_fields);

    uint32_t          len_ruuvi_ecdh_pub_key = 0;
    const char* const p_ruuvi_ecdh_pub_key   = http_req_header_get_field(
        p_param->p_req_info->http_header,
        "Ruuvi-Ecdh-Pub-Key:",
        &len_ruuvi_ecdh_pub_key);

    if (NULL != p_ruuvi_ecdh_pub_key)
    {
        http_server_ecdh_pub_key_b64_t pub_key_b64_srv = { 0 };
        if (!http_server_handle_ruuvi_ecdh_pub_key(p_ruuvi_ecdh_pub_key, len_ruuvi_ecdh_pub_key, &pub_key_b64_srv))
        {
            LOG_ERR("http_server_handle_ruuvi_ecdh_pub_key failed");
            if ((HTTP_CONTENT_LOCATION_HEAP == resp.content_location) && (NULL != resp.select_location.memory.p_buf))
            {
                os_free(resp.select_location.memory.p_buf);
            }
            return http_server_resp_500();
        }
        const size_t offset = strlen(p_extra_header_fields->buf);
        (void)snprintf(
            &p_extra_header_fields->buf[offset],
            sizeof(p_extra_header_fields->buf) - offset,
            "Ruuvi-Ecdh-Pub-Key: %s\r\n",
            pub_key_b64_srv.buf);
    }
    return resp;
}

static http_server_resp_t
http_server_handle_req_post_with_ecdh_key(
    const char* const                           p_path,
    const http_server_handle_req_param_t* const p_param,
    http_header_extra_fields_t* const           p_extra_header_fields)
{
    bool              flag_encrypted           = false;
    uint32_t          len_ruuvi_ecdh_encrypted = 0;
    const char* const p_ruuvi_ecdh_encrypted   = http_req_header_get_field(
        p_param->p_req_info->http_header,
        "Ruuvi-Ecdh-Encrypted:",
        &len_ruuvi_ecdh_encrypted);
    if ((NULL != p_ruuvi_ecdh_encrypted) && (0 == strncmp(p_ruuvi_ecdh_encrypted, "true", len_ruuvi_ecdh_encrypted)))
    {
        flag_encrypted = true;
    }

    str_buf_t       decrypted_str_buf   = STR_BUF_INIT_NULL();
    http_req_body_t http_body_decrypted = {
        .ptr = NULL,
    };
    if (flag_encrypted)
    {
        if (!http_server_decrypt(p_param->p_req_info->http_body.ptr, &decrypted_str_buf))
        {
            return http_server_resp_400();
        }
        http_body_decrypted.ptr = decrypted_str_buf.buf;
    }

    const http_server_resp_t resp = http_server_handle_req_post(
        p_path,
        p_param,
        flag_encrypted ? http_body_decrypted : p_param->p_req_info->http_body,
        p_extra_header_fields);
    if (flag_encrypted)
    {
        str_buf_free_buf(&decrypted_str_buf);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    return resp;
}

http_server_resp_t
http_server_handle_req(
    const http_server_handle_req_param_t* const p_param,
    http_header_extra_fields_t* const           p_extra_header_fields)
{
    assert(NULL != p_extra_header_fields);
    p_extra_header_fields->buf[0] = '\0';

    const char* p_path = p_param->p_req_info->http_uri.ptr;
    if ('/' == p_path[0])
    {
        p_path += 1;
    }

    if (0 == strcmp("GET", p_param->p_req_info->http_cmd.ptr))
    {
        return http_server_handle_req_get_with_ecdh_key(p_path, p_param, p_extra_header_fields);
    }
    if (0 == strcmp("DELETE", p_param->p_req_info->http_cmd.ptr))
    {
        return http_server_handle_req_delete(p_path, p_param, p_extra_header_fields);
    }
    if (0 == strcmp("POST", p_param->p_req_info->http_cmd.ptr))
    {
        return http_server_handle_req_post_with_ecdh_key(p_path, p_param, p_extra_header_fields);
    }
    return http_server_resp_400();
}
