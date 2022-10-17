/**
 * @file http_server_handle_req_post_auth.c
 * @author TheSomeMan
 * @date 2021-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_handle_req_post_auth.h"
#include <string.h>
#include <stdio.h>
#include "mbedtls/sha256.h"
#include "http_server_auth.h"
#include "cJSON.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

static const char* TAG = "req_post_auth";

#define MBEDTLS_SHA256_USE_224 1
#define MBEDTLS_SHA256_USE_256 0

static bool
http_server_json_get_string_val(
    const cJSON* const p_json_root,
    const char* const  p_attr_name,
    char* const        buf,
    const size_t       buf_len)
{
    if ((NULL == buf) || (0 == buf_len))
    {
        return false;
    }
    buf[0]                   = '\0';
    cJSON* const p_json_attr = cJSON_GetObjectItem(p_json_root, p_attr_name);
    if (NULL == p_json_attr)
    {
        return false;
    }
    const char* const p_str = cJSON_GetStringValue(p_json_attr);
    if (NULL == p_str)
    {
        return false;
    }
    snprintf(buf, buf_len, "%s", p_str);
    return true;
}

static bool
json_ruuvi_auth_parse(const cJSON* const p_json_root, http_server_auth_ruuvi_req_t* const p_auth_req)
{
    if (!http_server_json_get_string_val(p_json_root, "login", p_auth_req->username.buf, sizeof(p_auth_req->username)))
    {
        return false;
    }
    if (!http_server_json_get_string_val(
            p_json_root,
            "password",
            p_auth_req->password.buf,
            sizeof(p_auth_req->password)))
    {
        return false;
    }
    return true;
}

static bool
json_ruuvi_auth_parse_http_body(const char* const p_body, http_server_auth_ruuvi_req_t* const p_auth_req)
{
    cJSON* p_json_root = cJSON_Parse(p_body);
    if (NULL == p_json_root)
    {
        return false;
    }
    const bool ret = json_ruuvi_auth_parse(p_json_root, p_auth_req);
    cJSON_Delete(p_json_root);
    return ret;
}

static bool
http_server_auth_ruuvi_gen_hashed_password(
    const char* const                      p_challenge,
    const char* const                      p_password,
    wifiman_sha256_digest_hex_str_t* const p_enc_pass)
{
    mbedtls_sha256_context ctx = { 0 };
    mbedtls_sha256_init(&ctx);
    if (0 != mbedtls_sha256_starts_ret(&ctx, MBEDTLS_SHA256_USE_256))
    {
        mbedtls_sha256_free(&ctx);
        return false;
    }
    if (0 != mbedtls_sha256_update_ret(&ctx, (const unsigned char*)p_challenge, strlen(p_challenge)))
    {
        mbedtls_sha256_free(&ctx);
        return false;
    }
    if (0 != mbedtls_sha256_update_ret(&ctx, (const unsigned char*)":", 1U))
    {
        mbedtls_sha256_free(&ctx);
        return false;
    }
    if (0 != mbedtls_sha256_update_ret(&ctx, (const unsigned char*)p_password, strlen(p_password)))
    {
        mbedtls_sha256_free(&ctx);
        return false;
    }
    wifiman_sha256_digest_t digest = { 0 };
    if (0 != mbedtls_sha256_finish_ret(&ctx, digest.buf))
    {
        mbedtls_sha256_free(&ctx);
        return false;
    }
    mbedtls_sha256_free(&ctx);
    *p_enc_pass = wifiman_sha256_hex_str(&digest);
    return true;
}

static void
http_server_auth_ruuvi_add_authorized_session(
    http_server_auth_ruuvi_t* const                  p_auth_ruuvi,
    const http_server_auth_ruuvi_session_id_t* const p_session_id,
    const sta_ip_string_t* const                     p_remote_ip)
{
    for (int32_t i = HTTP_SERVER_AUTH_RUUVI_MAX_NUM_SESSIONS - 1; i > 0; --i)
    {
        p_auth_ruuvi->authorized_sessions[i] = p_auth_ruuvi->authorized_sessions[i - 1];
    }
    p_auth_ruuvi->authorized_sessions[0].session_id = *p_session_id;
    p_auth_ruuvi->authorized_sessions[0].remote_ip  = *p_remote_ip;
}

static bool
http_server_handle_req_post_auth_check_login_session(
    const http_server_auth_ruuvi_login_session_t* const p_login_session,
    const http_server_auth_ruuvi_session_id_t*          p_session_id,
    const sta_ip_string_t* const                        p_remote_ip,
    const wifiman_wifi_ssid_t* const                    p_ap_ssid,
    const bool                                          flag_auth_default,
    http_header_extra_fields_t* const                   p_extra_header_fields,
    http_server_resp_t* const                           p_resp)
{
    if ('\0' == p_login_session->session_id.buf[0])
    {
        LOG_WARN("session_id is empty");
        *p_resp = http_server_resp_401_auth_ruuvi_with_new_session_id(
            p_remote_ip,
            p_ap_ssid,
            p_extra_header_fields,
            flag_auth_default);
        return false;
    }
    if (0 != strcmp(p_login_session->session_id.buf, p_session_id->buf))
    {
        LOG_WARN(
            "session_id (%s) does not match with the last saved one (%s)",
            p_session_id->buf,
            p_login_session->session_id.buf);
        *p_resp = http_server_resp_401_auth_ruuvi_with_new_session_id(
            p_remote_ip,
            p_ap_ssid,
            p_extra_header_fields,
            flag_auth_default);
        return false;
    }
    if (!sta_ip_cmp(&p_login_session->remote_ip, p_remote_ip))
    {
        LOG_WARN("RemoteIP does not match with the session_id");
        *p_resp = http_server_resp_401_auth_ruuvi_with_new_session_id(
            p_remote_ip,
            p_ap_ssid,
            p_extra_header_fields,
            flag_auth_default);
        return false;
    }
    return true;
}

http_server_resp_t
http_server_handle_req_post_auth(
    const bool                           flag_access_from_lan,
    const http_req_header_t              http_header,
    const sta_ip_string_t* const         p_remote_ip,
    const http_req_body_t                http_body,
    const http_server_auth_info_t* const p_auth_info,
    const wifiman_wifi_ssid_t* const     p_ap_ssid,
    http_header_extra_fields_t* const    p_extra_header_fields)
{
    if (!flag_access_from_lan)
    {
        return http_server_resp_200_json("{}");
    }
    if ((HTTP_SERVER_AUTH_TYPE_RUUVI != p_auth_info->auth_type)
        && (HTTP_SERVER_AUTH_TYPE_DEFAULT != p_auth_info->auth_type))
    {
        LOG_ERR("Auth type is not RUUVI, auth_type=%d", (printf_int_t)p_auth_info->auth_type);
        return http_server_resp_503();
    }

    const bool flag_auth_default = (HTTP_SERVER_AUTH_TYPE_DEFAULT == p_auth_info->auth_type) ? true : false;

    http_server_auth_ruuvi_session_id_t session_id = { 0 };
    if (!http_server_auth_ruuvi_get_session_id_from_cookies(http_header, &session_id))
    {
        LOG_WARN("There is no session_id in cookies");
        return http_server_resp_401_auth_ruuvi_with_new_session_id(
            p_remote_ip,
            p_ap_ssid,
            p_extra_header_fields,
            flag_auth_default);
    }
    const http_server_auth_ruuvi_prev_url_t prev_url = http_server_auth_ruuvi_get_prev_url_from_cookies(http_header);

    http_server_auth_ruuvi_t* const p_auth_ruuvi = http_server_auth_ruuvi_get_info();

    http_server_resp_t resp = { 0 };
    if (!http_server_handle_req_post_auth_check_login_session(
            &p_auth_ruuvi->login_session,
            &session_id,
            p_remote_ip,
            p_ap_ssid,
            flag_auth_default,
            p_extra_header_fields,
            &resp))
    {
        return resp;
    }

    http_server_auth_ruuvi_req_t auth_req = { 0 };
    if (!json_ruuvi_auth_parse_http_body(http_body.ptr, &auth_req))
    {
        LOG_WARN("Failed to parse auth request body");
        return http_server_resp_401_auth_ruuvi_with_new_session_id(
            p_remote_ip,
            p_ap_ssid,
            p_extra_header_fields,
            flag_auth_default);
    }
    if ('\0' == auth_req.username.buf[0])
    {
        LOG_WARN("User name is empty");
        return http_server_resp_401_auth_ruuvi_with_new_session_id(
            p_remote_ip,
            p_ap_ssid,
            p_extra_header_fields,
            flag_auth_default);
    }
    if (0 != strcmp(p_auth_info->auth_user.buf, auth_req.username.buf))
    {
        LOG_WARN("User name in auth_info does not match the username from auth_req");
        return http_server_resp_401_auth_ruuvi_with_new_session_id(
            p_remote_ip,
            p_ap_ssid,
            p_extra_header_fields,
            flag_auth_default);
    }
    wifiman_sha256_digest_hex_str_t password_hash = { 0 };
    if (!http_server_auth_ruuvi_gen_hashed_password(
            p_auth_ruuvi->login_session.challenge.buf,
            p_auth_info->auth_pass.buf,
            &password_hash))
    {
        LOG_WARN("Failed to generate hashed password");
        return http_server_resp_401_auth_ruuvi_with_new_session_id(
            p_remote_ip,
            p_ap_ssid,
            p_extra_header_fields,
            flag_auth_default);
    }
    if (0 != strcmp(password_hash.buf, auth_req.password.buf))
    {
        LOG_WARN("Password does not match");
        return http_server_resp_401_auth_ruuvi_with_new_session_id(
            p_remote_ip,
            p_ap_ssid,
            p_extra_header_fields,
            flag_auth_default);
    }
    http_server_auth_ruuvi_add_authorized_session(p_auth_ruuvi, &session_id, p_remote_ip);
    http_server_auth_ruuvi_login_session_clear(&p_auth_ruuvi->login_session);
    if ('\0' != prev_url.buf[0])
    {
        (void)snprintf(
            p_extra_header_fields->buf,
            sizeof(p_extra_header_fields->buf),
            "Ruuvi-prev-url: %s\r\n"
            "Set-Cookie: %s=; Max-Age=-1; Expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n",
            prev_url.buf,
            HTTP_SERVER_AUTH_RUUVI_COOKIE_PREV_URL);
    }
    return http_server_resp_200_json("{}");
}
