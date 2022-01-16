/**
 * @file http_server_handle_req_get_auth.c
 * @author TheSomeMan
 * @date 2021-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_handle_req_get_auth.h"
#include <string.h>
#include <stdio.h>
#include "http_server_auth.h"
#include "wifiman_sha256.h"
#include "wifiman_md5.h"
#include "str_buf.h"

static http_server_resp_t
http_server_handle_req_get_auth_allow(const wifi_ssid_t *const p_ap_ssid)
{
    const bool                                is_successful = true;
    const http_server_resp_auth_json_t *const p_auth_json   = http_server_fill_auth_json(
        is_successful,
        p_ap_ssid,
        HTTP_SERVER_AUTH_TYPE_STR_ALLOW);
    return http_server_resp_200_json(p_auth_json->buf);
}

static http_server_resp_t
http_server_resp_401_auth_basic(
    const wifi_ssid_t *const          p_ap_ssid,
    http_header_extra_fields_t *const p_extra_header_fields)
{
    const http_server_resp_auth_json_t *const p_auth_json = http_server_fill_auth_json(
        false,
        p_ap_ssid,
        HTTP_SERVER_AUTH_TYPE_STR_BASIC);
    snprintf(
        p_extra_header_fields->buf,
        sizeof(p_extra_header_fields->buf),
        "WWW-Authenticate: Basic realm=\"%s\", charset=\"UTF-8\"\r\n",
        p_ap_ssid->ssid_buf);
    return http_server_resp_401_json(p_auth_json);
}

static http_server_auth_api_key_e
http_server_handle_req_check_auth_bearer(
    const http_req_header_t              http_header,
    const http_server_auth_info_t *const p_auth_info)
{
    if ('\0' == p_auth_info->auth_api_key[0])
    {
        return HTTP_SERVER_AUTH_API_KEY_PROHIBITED;
    }
    uint32_t          len_authorization = 0;
    const char *const p_authorization   = http_req_header_get_field(http_header, "Authorization:", &len_authorization);
    if (NULL == p_authorization)
    {
        return HTTP_SERVER_AUTH_API_KEY_NOT_USED;
    }
    const char *const p_auth_prefix   = "Bearer ";
    const size_t      auth_prefix_len = strlen(p_auth_prefix);
    if (0 != strncmp(p_authorization, p_auth_prefix, auth_prefix_len))
    {
        return HTTP_SERVER_AUTH_API_KEY_NOT_USED;
    }
    const char *const p_auth_token   = &p_authorization[auth_prefix_len];
    const size_t      auth_token_len = len_authorization - auth_prefix_len;

    if (auth_token_len != strlen(p_auth_info->auth_api_key))
    {
        return HTTP_SERVER_AUTH_API_KEY_PROHIBITED;
    }
    if (0 != strncmp(p_auth_token, p_auth_info->auth_api_key, auth_token_len))
    {
        return HTTP_SERVER_AUTH_API_KEY_PROHIBITED;
    }
    return HTTP_SERVER_AUTH_API_KEY_ALLOWED;
}

static http_server_resp_t
http_server_handle_req_get_auth_basic(
    const http_req_header_t              http_header,
    const http_server_auth_info_t *const p_auth_info,
    const wifi_ssid_t *const             p_ap_ssid,
    http_header_extra_fields_t *const    p_extra_header_fields)
{
    uint32_t          len_authorization = 0;
    const char *const p_authorization   = http_req_header_get_field(http_header, "Authorization:", &len_authorization);
    if (NULL == p_authorization)
    {
        return http_server_resp_401_auth_basic(p_ap_ssid, p_extra_header_fields);
    }
    const char *const p_auth_prefix   = "Basic ";
    const size_t      auth_prefix_len = strlen(p_auth_prefix);
    if (0 != strncmp(p_authorization, p_auth_prefix, auth_prefix_len))
    {
        return http_server_resp_401_auth_basic(p_ap_ssid, p_extra_header_fields);
    }
    const char *const p_auth_token   = &p_authorization[auth_prefix_len];
    const size_t      auth_token_len = len_authorization - auth_prefix_len;

    if (auth_token_len != strlen(p_auth_info->auth_pass))
    {
        return http_server_resp_401_auth_basic(p_ap_ssid, p_extra_header_fields);
    }
    if (0 != strncmp(p_auth_token, p_auth_info->auth_pass, auth_token_len))
    {
        return http_server_resp_401_auth_basic(p_ap_ssid, p_extra_header_fields);
    }

    const http_server_resp_auth_json_t *p_auth_json = http_server_fill_auth_json(
        true,
        p_ap_ssid,
        HTTP_SERVER_AUTH_TYPE_STR_BASIC);
    return http_server_resp_200_json(p_auth_json->buf);
}

static http_server_resp_t
http_server_handle_req_get_auth_digest(
    const http_req_header_t              http_header,
    const http_server_auth_info_t *const p_auth_info,
    const wifi_ssid_t *const             p_ap_ssid,
    http_header_extra_fields_t *const    p_extra_header_fields)
{
    uint32_t          len_authorization = 0;
    const char *const p_authorization   = http_req_header_get_field(http_header, "Authorization:", &len_authorization);
    if (NULL == p_authorization)
    {
        return http_server_resp_401_auth_digest(p_ap_ssid, p_extra_header_fields);
    }
    http_server_auth_digest_req_t *const p_auth_req = http_server_auth_digest_get_info();
    if (!http_server_parse_digest_authorization_str(p_authorization, len_authorization, p_auth_req))
    {
        return http_server_resp_401_auth_digest(p_ap_ssid, p_extra_header_fields);
    }
    if (0 != strcmp(p_auth_req->username, p_auth_info->auth_user))
    {
        return http_server_resp_401_auth_digest(p_ap_ssid, p_extra_header_fields);
    }

    const char ha2_prefix[] = "GET:";
    char       ha2_str[sizeof(ha2_prefix) + HTTP_SERVER_AUTH_DIGEST_URI_SIZE];
    snprintf(ha2_str, sizeof(ha2_str), "%s%s", ha2_prefix, p_auth_req->uri);
    const wifiman_md5_digest_hex_str_t ha2_md5 = wifiman_md5_calc_hex_str(ha2_str, strlen(ha2_str));

    str_buf_t str_buf_response = str_buf_printf_with_alloc(
        "%s:%s:%s:%s:%s:%s",
        p_auth_info->auth_pass,
        p_auth_req->nonce,
        p_auth_req->nc,
        p_auth_req->cnonce,
        p_auth_req->qop,
        ha2_md5.buf);
    if (NULL == str_buf_response.buf)
    {
        return http_server_resp_503();
    }
    const wifiman_md5_digest_hex_str_t response_md5 = wifiman_md5_calc_hex_str(
        str_buf_response.buf,
        str_buf_get_len(&str_buf_response));
    str_buf_free_buf(&str_buf_response);

    if (0 != strcmp(p_auth_req->response, response_md5.buf))
    {
        return http_server_resp_401_auth_digest(p_ap_ssid, p_extra_header_fields);
    }

    const http_server_resp_auth_json_t *p_auth_json = http_server_fill_auth_json(
        true,
        p_ap_ssid,
        HTTP_SERVER_AUTH_TYPE_STR_DIGEST);
    return http_server_resp_200_json(p_auth_json->buf);
}

static http_server_resp_t
http_server_handle_req_get_auth_ruuvi(
    const http_req_header_t           http_header,
    const sta_ip_string_t *const      p_remote_ip,
    const wifi_ssid_t *const          p_ap_ssid,
    const bool                        flag_check,
    http_header_extra_fields_t *const p_extra_header_fields)
{
    http_server_auth_ruuvi_session_id_t session_id = { 0 };
    if (!http_server_auth_ruuvi_get_session_id_from_cookies(http_header, &session_id))
    {
        if (flag_check)
        {
            return http_server_resp_401_auth_ruuvi(p_ap_ssid);
        }
        else
        {
            return http_server_resp_401_auth_ruuvi_with_new_session_id(p_remote_ip, p_ap_ssid, p_extra_header_fields);
        }
    }
    const http_server_auth_ruuvi_authorized_session_t *const p_authorized_session
        = http_server_auth_ruuvi_find_authorized_session(&session_id, p_remote_ip);

    if (NULL == p_authorized_session)
    {
        if (flag_check)
        {
            return http_server_resp_401_auth_ruuvi(p_ap_ssid);
        }
        else
        {
            return http_server_resp_401_auth_ruuvi_with_new_session_id(p_remote_ip, p_ap_ssid, p_extra_header_fields);
        }
    }

    const http_server_resp_auth_json_t *p_auth_json = http_server_fill_auth_json(
        true,
        p_ap_ssid,
        HTTP_SERVER_AUTH_TYPE_STR_RUUVI);
    return http_server_resp_200_json(p_auth_json->buf);
}

static http_server_resp_t
http_server_handle_req_get_auth_deny(const wifi_ssid_t *const p_ap_ssid)
{
    return http_server_resp_403_auth_deny(p_ap_ssid);
}

static http_server_resp_t
http_server_handle_req_get_or_check_auth(
    const http_req_header_t              http_header,
    const sta_ip_string_t *const         p_remote_ip,
    const http_server_auth_info_t *const p_auth_info,
    const wifi_ssid_t *const             p_ap_ssid,
    const bool                           flag_check,
    http_header_extra_fields_t *const    p_extra_header_fields,
    http_server_auth_api_key_e *const    p_allow_access_by_api_key)
{
    if (NULL != p_allow_access_by_api_key)
    {
        *p_allow_access_by_api_key = http_server_handle_req_check_auth_bearer(http_header, p_auth_info);
    }
    switch (p_auth_info->auth_type)
    {
        case HTTP_SERVER_AUTH_TYPE_ALLOW:
            return http_server_handle_req_get_auth_allow(p_ap_ssid);
        case HTTP_SERVER_AUTH_TYPE_BASIC:
            return http_server_handle_req_get_auth_basic(http_header, p_auth_info, p_ap_ssid, p_extra_header_fields);
        case HTTP_SERVER_AUTH_TYPE_DIGEST:
            return http_server_handle_req_get_auth_digest(http_header, p_auth_info, p_ap_ssid, p_extra_header_fields);
        case HTTP_SERVER_AUTH_TYPE_RUUVI:
            return http_server_handle_req_get_auth_ruuvi(
                http_header,
                p_remote_ip,
                p_ap_ssid,
                flag_check,
                p_extra_header_fields);
        case HTTP_SERVER_AUTH_TYPE_DENY:
            return http_server_handle_req_get_auth_deny(p_ap_ssid);
    }
    return http_server_resp_503();
}

http_server_resp_t
http_server_handle_req_check_auth(
    const bool                           flag_access_from_lan,
    const http_req_header_t              http_header,
    const sta_ip_string_t *const         p_remote_ip,
    const http_server_auth_info_t *const p_auth_info,
    const wifi_ssid_t *const             p_ap_ssid,
    http_header_extra_fields_t *const    p_extra_header_fields,
    http_server_auth_api_key_e *const    p_allow_access_by_api_key)
{
    if (!flag_access_from_lan)
    {
        return http_server_handle_req_get_auth_allow(p_ap_ssid);
    }
    return http_server_handle_req_get_or_check_auth(
        http_header,
        p_remote_ip,
        p_auth_info,
        p_ap_ssid,
        true,
        p_extra_header_fields,
        p_allow_access_by_api_key);
}

http_server_resp_t
http_server_handle_req_get_auth(
    const bool                           flag_access_from_lan,
    const http_req_header_t              http_header,
    const sta_ip_string_t *const         p_remote_ip,
    const http_server_auth_info_t *const p_auth_info,
    const wifi_ssid_t *const             p_ap_ssid,
    http_header_extra_fields_t *const    p_extra_header_fields)
{
    if (!flag_access_from_lan)
    {
        return http_server_handle_req_get_auth_allow(p_ap_ssid);
    }
    return http_server_handle_req_get_or_check_auth(
        http_header,
        p_remote_ip,
        p_auth_info,
        p_ap_ssid,
        false,
        p_extra_header_fields,
        NULL);
}
