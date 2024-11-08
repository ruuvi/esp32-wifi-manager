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
http_server_handle_req_get_auth_allow(const wifiman_hostinfo_t* const p_hostinfo, const bool flag_access_from_lan)
{
    const http_server_resp_auth_json_t* const p_auth_json = http_server_fill_auth_json(
        p_hostinfo,
        HTTP_SERVER_AUTH_TYPE_ALLOW,
        flag_access_from_lan,
        NULL);
    return http_server_resp_200_json(p_auth_json->buf);
}

static http_server_resp_t
http_server_resp_401_auth_basic(
    const wifiman_hostinfo_t* const   p_hostinfo,
    http_header_extra_fields_t* const p_extra_header_fields)
{
    const bool flag_access_from_lan = true;

    const http_server_resp_auth_json_t* const p_auth_json = http_server_fill_auth_json(
        p_hostinfo,
        HTTP_SERVER_AUTH_TYPE_BASIC,
        flag_access_from_lan,
        NULL);
    (void)snprintf(
        p_extra_header_fields->buf,
        sizeof(p_extra_header_fields->buf),
        "WWW-Authenticate: Basic realm=\"%s\", charset=\"UTF-8\"\r\n",
        p_hostinfo->hostname.buf);
    return http_server_resp_401_json(p_auth_json);
}

static http_server_auth_api_key_e
http_server_handle_req_check_auth_bearer(
    const http_req_header_t              http_header,
    const bool                           flag_check_rw_access,
    const http_server_auth_info_t* const p_auth_info)
{
    uint32_t          len_authorization = 0;
    const char* const p_authorization   = http_req_header_get_field(http_header, "Authorization:", &len_authorization);
    if (NULL == p_authorization)
    {
        return HTTP_SERVER_AUTH_API_KEY_NOT_USED;
    }
    const char* const p_auth_prefix   = "Bearer ";
    const size_t      auth_prefix_len = strlen(p_auth_prefix);
    if (0 != strncmp(p_authorization, p_auth_prefix, auth_prefix_len))
    {
        return HTTP_SERVER_AUTH_API_KEY_NOT_USED;
    }
    const char* const p_auth_token   = &p_authorization[auth_prefix_len];
    const size_t      auth_token_len = len_authorization - auth_prefix_len;

    if (('\0' != p_auth_info->auth_api_key_rw.buf[0]) && (auth_token_len == strlen(p_auth_info->auth_api_key_rw.buf))
        && (0 == strncmp(p_auth_token, p_auth_info->auth_api_key_rw.buf, auth_token_len)))
    {
        return HTTP_SERVER_AUTH_API_KEY_ALLOWED;
    }
    if ((!flag_check_rw_access) && ('\0' != p_auth_info->auth_api_key.buf[0])
        && (auth_token_len == strlen(p_auth_info->auth_api_key.buf))
        && (0 == strncmp(p_auth_token, p_auth_info->auth_api_key.buf, auth_token_len)))
    {
        return HTTP_SERVER_AUTH_API_KEY_ALLOWED;
    }
    return HTTP_SERVER_AUTH_API_KEY_PROHIBITED;
}

static http_server_resp_t
http_server_handle_req_get_auth_basic(
    const http_server_handle_req_auth_param_t* const p_param,
    http_header_extra_fields_t* const                p_extra_header_fields)
{
    uint32_t          len_authorization = 0;
    const char* const p_authorization   = http_req_header_get_field(
        p_param->http_header,
        "Authorization:",
        &len_authorization);
    if (NULL == p_authorization)
    {
        return http_server_resp_401_auth_basic(p_param->p_hostinfo, p_extra_header_fields);
    }
    const char* const p_auth_prefix   = "Basic ";
    const size_t      auth_prefix_len = strlen(p_auth_prefix);
    if (0 != strncmp(p_authorization, p_auth_prefix, auth_prefix_len))
    {
        return http_server_resp_401_auth_basic(p_param->p_hostinfo, p_extra_header_fields);
    }
    const char* const p_auth_token   = &p_authorization[auth_prefix_len];
    const size_t      auth_token_len = len_authorization - auth_prefix_len;

    if (auth_token_len != strlen(p_param->p_auth_info->auth_pass.buf))
    {
        return http_server_resp_401_auth_basic(p_param->p_hostinfo, p_extra_header_fields);
    }
    if (0 != strncmp(p_auth_token, p_param->p_auth_info->auth_pass.buf, auth_token_len))
    {
        return http_server_resp_401_auth_basic(p_param->p_hostinfo, p_extra_header_fields);
    }

    const http_server_resp_auth_json_t* p_auth_json = http_server_fill_auth_json(
        p_param->p_hostinfo,
        HTTP_SERVER_AUTH_TYPE_BASIC,
        p_param->flag_access_from_lan,
        NULL);
    return http_server_resp_200_json(p_auth_json->buf);
}

static http_server_resp_t
http_server_handle_req_get_auth_digest(
    const http_server_handle_req_auth_param_t* const p_param,
    http_header_extra_fields_t* const                p_extra_header_fields)
{
    uint32_t          len_authorization = 0;
    const char* const p_authorization   = http_req_header_get_field(
        p_param->http_header,
        "Authorization:",
        &len_authorization);
    if (NULL == p_authorization)
    {
        return http_server_resp_401_auth_digest(p_param->p_hostinfo, p_extra_header_fields);
    }
    http_server_auth_digest_req_t* const p_auth_req = http_server_auth_digest_get_info();
    if (!http_server_parse_digest_authorization_str(p_authorization, len_authorization, p_auth_req))
    {
        return http_server_resp_401_auth_digest(p_param->p_hostinfo, p_extra_header_fields);
    }
    if (0 != strcmp(p_auth_req->username, p_param->p_auth_info->auth_user.buf))
    {
        return http_server_resp_401_auth_digest(p_param->p_hostinfo, p_extra_header_fields);
    }

    str_buf_t str_buf = str_buf_printf_with_alloc("GET:%s", p_auth_req->uri);
    if (NULL == str_buf.buf)
    {
        return http_server_resp_503();
    }
    const wifiman_md5_digest_hex_str_t ha2_md5 = wifiman_md5_calc_hex_str(str_buf.buf, strlen(str_buf.buf));
    str_buf_free_buf(&str_buf);

    str_buf = str_buf_printf_with_alloc(
        "%s:%s:%s:%s:%s:%s",
        p_param->p_auth_info->auth_pass.buf,
        p_auth_req->nonce,
        p_auth_req->nc,
        p_auth_req->cnonce,
        p_auth_req->qop,
        ha2_md5.buf);
    if (NULL == str_buf.buf)
    {
        return http_server_resp_503();
    }
    const wifiman_md5_digest_hex_str_t response_md5 = wifiman_md5_calc_hex_str(str_buf.buf, str_buf_get_len(&str_buf));
    str_buf_free_buf(&str_buf);

    if (0 != strcmp(p_auth_req->response, response_md5.buf))
    {
        return http_server_resp_401_auth_digest(p_param->p_hostinfo, p_extra_header_fields);
    }

    const http_server_resp_auth_json_t* p_auth_json = http_server_fill_auth_json(
        p_param->p_hostinfo,
        HTTP_SERVER_AUTH_TYPE_DIGEST,
        p_param->flag_access_from_lan,
        NULL);
    return http_server_resp_200_json(p_auth_json->buf);
}

static http_server_resp_t
http_server_handle_req_get_auth_ruuvi(
    const http_server_handle_req_auth_param_t* const p_param,
    const bool                                       flag_check,
    const http_server_auth_type_e                    auth_type,
    http_header_extra_fields_t* const                p_extra_header_fields)
{
    if (flag_check && (HTTP_SERVER_AUTH_TYPE_ALLOW == auth_type))
    {
        return http_server_handle_req_get_auth_allow(p_param->p_hostinfo, p_param->flag_access_from_lan);
    }
    http_server_auth_ruuvi_session_id_t session_id = { 0 };
    if (!http_server_auth_ruuvi_get_session_id_from_cookies(p_param->http_header, &session_id))
    {
        if (HTTP_SERVER_AUTH_TYPE_ALLOW == auth_type)
        {
            return http_server_resp_200_auth_allow_with_new_session_id(
                p_param->p_remote_ip,
                p_param->p_hostinfo,
                p_extra_header_fields);
        }
        if (flag_check)
        {
            return http_server_resp_401_auth_ruuvi(p_param->p_hostinfo, auth_type);
        }
        return http_server_resp_401_auth_ruuvi_with_new_session_id(
            p_param->p_remote_ip,
            p_param->p_hostinfo,
            p_extra_header_fields,
            auth_type,
            NULL);
    }
    const http_server_auth_ruuvi_authorized_session_t* const p_authorized_session
        = http_server_auth_ruuvi_find_authorized_session(&session_id, p_param->p_remote_ip);

    if (NULL == p_authorized_session)
    {
        if (flag_check)
        {
            return http_server_resp_401_auth_ruuvi(p_param->p_hostinfo, auth_type);
        }
        if (HTTP_SERVER_AUTH_TYPE_ALLOW == auth_type)
        {
            return http_server_resp_200_auth_allow_with_new_session_id(
                p_param->p_remote_ip,
                p_param->p_hostinfo,
                p_extra_header_fields);
        }
        return http_server_resp_401_auth_ruuvi_with_new_session_id(
            p_param->p_remote_ip,
            p_param->p_hostinfo,
            p_extra_header_fields,
            auth_type,
            NULL);
    }

    const http_server_resp_auth_json_t* p_auth_json = http_server_fill_auth_json(
        p_param->p_hostinfo,
        auth_type,
        p_param->flag_access_from_lan,
        NULL);
    return http_server_resp_200_json(p_auth_json->buf);
}

static http_server_resp_t
http_server_handle_req_get_auth_deny(const wifiman_hostinfo_t* const p_hostinfo)
{
    return http_server_resp_403_auth_deny(p_hostinfo);
}

static http_server_resp_t
http_server_handle_req_get_or_check_auth(
    const http_server_handle_req_auth_param_t* const p_param,
    const bool                                       flag_check,
    http_header_extra_fields_t* const                p_extra_header_fields,
    bool* const                                      p_flag_access_by_bearer_token)
{
    if (NULL != p_flag_access_by_bearer_token)
    {
        http_server_auth_api_key_e access_by_bearer_token = http_server_handle_req_check_auth_bearer(
            p_param->http_header,
            p_param->flag_check_rw_access_with_bearer_token,
            p_param->p_auth_info);
        switch (access_by_bearer_token)
        {
            case HTTP_SERVER_AUTH_API_KEY_NOT_USED:
                *p_flag_access_by_bearer_token = false;
                break;
            case HTTP_SERVER_AUTH_API_KEY_ALLOWED:
                *p_flag_access_by_bearer_token = true;
                return http_server_resp_200_json(http_server_fill_auth_json(
                                                     p_param->p_hostinfo,
                                                     HTTP_SERVER_AUTH_TYPE_BEARER,
                                                     p_param->flag_access_from_lan,
                                                     NULL)
                                                     ->buf);
            case HTTP_SERVER_AUTH_API_KEY_PROHIBITED:
                *p_flag_access_by_bearer_token = true;
                return http_server_resp_401_json(http_server_fill_auth_json(
                    p_param->p_hostinfo,
                    HTTP_SERVER_AUTH_TYPE_BEARER,
                    p_param->flag_access_from_lan,
                    NULL));
        }
    }
    switch (p_param->p_auth_info->auth_type)
    {
        case HTTP_SERVER_AUTH_TYPE_ALLOW:
            return http_server_handle_req_get_auth_ruuvi(
                p_param,
                flag_check,
                p_param->p_auth_info->auth_type,
                p_extra_header_fields);
        case HTTP_SERVER_AUTH_TYPE_BASIC:
            return http_server_handle_req_get_auth_basic(p_param, p_extra_header_fields);
        case HTTP_SERVER_AUTH_TYPE_DIGEST:
            return http_server_handle_req_get_auth_digest(p_param, p_extra_header_fields);
        case HTTP_SERVER_AUTH_TYPE_RUUVI:
            return http_server_handle_req_get_auth_ruuvi(
                p_param,
                flag_check,
                p_param->p_auth_info->auth_type,
                p_extra_header_fields);
        case HTTP_SERVER_AUTH_TYPE_DENY:
            return http_server_handle_req_get_auth_deny(p_param->p_hostinfo);
        case HTTP_SERVER_AUTH_TYPE_DEFAULT:
            return http_server_handle_req_get_auth_ruuvi(
                p_param,
                flag_check,
                p_param->p_auth_info->auth_type,
                p_extra_header_fields);
        case HTTP_SERVER_AUTH_TYPE_BEARER:
            return http_server_resp_500();
    }
    return http_server_resp_503();
}

http_server_resp_t
http_server_handle_req_check_auth(
    const http_server_handle_req_auth_param_t* const p_param,
    http_header_extra_fields_t* const                p_extra_header_fields,
    bool* const                                      p_flag_access_by_bearer_token)
{
    if (!p_param->flag_access_from_lan)
    {
        return http_server_handle_req_get_auth_allow(p_param->p_hostinfo, p_param->flag_access_from_lan);
    }
    return http_server_handle_req_get_or_check_auth(
        p_param,
        true,
        p_extra_header_fields,
        p_flag_access_by_bearer_token);
}

http_server_resp_t
http_server_handle_req_get_auth(
    const http_server_handle_req_auth_param_t* const p_param,
    http_header_extra_fields_t* const                p_extra_header_fields)
{
    if (!p_param->flag_access_from_lan)
    {
        return http_server_handle_req_get_auth_allow(p_param->p_hostinfo, p_param->flag_access_from_lan);
    }
    const bool flag_check = false;
    return http_server_handle_req_get_or_check_auth(p_param, flag_check, p_extra_header_fields, NULL);
}
