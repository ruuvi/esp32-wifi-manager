/**
 * @file http_server_resp.h
 * @author TheSomeMan
 * @date 2020-10-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_resp.h"
#include <string.h>
#include <esp_system.h>
#include "http_server_auth.h"

static http_server_resp_auth_json_t g_auth_json;

http_server_resp_t
http_server_resp_200_json(const char* p_json_content)
{
    const bool flag_no_cache        = true;
    const bool flag_add_header_date = true;
    return http_server_resp_data_in_static_mem(
        HTTP_CONENT_TYPE_APPLICATION_JSON,
        NULL,
        strlen(p_json_content),
        HTTP_CONENT_ENCODING_NONE,
        (const uint8_t*)p_json_content,
        flag_no_cache,
        flag_add_header_date);
}

http_server_resp_t
http_server_resp_json_in_heap(const http_resp_code_e http_resp_code, const char* const p_json_content)
{
    const bool flag_no_cache        = true;
    const bool flag_add_header_date = true;
    return http_server_resp_data_in_heap(
        http_resp_code,
        HTTP_CONENT_TYPE_APPLICATION_JSON,
        NULL,
        strlen(p_json_content),
        HTTP_CONENT_ENCODING_NONE,
        (const uint8_t*)p_json_content,
        flag_no_cache,
        flag_add_header_date);
}

http_server_resp_t
http_server_resp_200_json_in_heap(const char* const p_json_content)
{
    return http_server_resp_json_in_heap(HTTP_RESP_CODE_200, p_json_content);
}

http_server_resp_t
http_server_resp_err(const http_resp_code_e http_resp_code)
{
    const http_server_resp_t resp = {
        .http_resp_code       = http_resp_code,
        .content_location     = HTTP_CONTENT_LOCATION_NO_CONTENT,
        .flag_no_cache        = true,
        .flag_add_header_date = true,
        .content_type         = HTTP_CONENT_TYPE_TEXT_HTML,
        .p_content_type_param = NULL,
        .content_len          = 0,
        .content_encoding     = HTTP_CONENT_ENCODING_NONE,
        .select_location = {
            .memory = {
                .p_buf = NULL,
            },
        },
    };
    return resp;
}

static http_server_resp_t
http_server_resp_err_json_in_static_mem(const http_resp_code_e http_resp_code, const char* const p_json_content)
{
    if (NULL == p_json_content)
    {
        return http_server_resp_err(http_resp_code);
    }
    const http_server_resp_t resp = {
        .http_resp_code       = http_resp_code,
        .content_location     = HTTP_CONTENT_LOCATION_STATIC_MEM,
        .flag_no_cache        = true,
        .flag_add_header_date = true,
        .content_type         = HTTP_CONENT_TYPE_APPLICATION_JSON,
        .p_content_type_param = NULL,
        .content_len          = strlen(p_json_content),
        .content_encoding     = HTTP_CONENT_ENCODING_NONE,
        .select_location      = {
            .memory = {
                .p_buf = (const uint8_t*)p_json_content,
            },
        },
    };
    return resp;
}

static http_server_resp_t
http_server_resp_err_json_in_heap(const http_resp_code_e http_resp_code, const char* const p_json_content)
{
    if (NULL == p_json_content)
    {
        return http_server_resp_err(http_resp_code);
    }
    const http_server_resp_t resp = {
        .http_resp_code       = http_resp_code,
        .content_location     = HTTP_CONTENT_LOCATION_HEAP,
        .flag_no_cache        = true,
        .flag_add_header_date = true,
        .content_type         = HTTP_CONENT_TYPE_APPLICATION_JSON,
        .p_content_type_param = NULL,
        .content_len          = strlen(p_json_content),
        .content_encoding     = HTTP_CONENT_ENCODING_NONE,
        .select_location      = {
            .memory = {
                .p_buf = (const uint8_t*)p_json_content,
            },
        },
    };
    return resp;
}

http_server_resp_t
http_server_resp_302(void)
{
    return http_server_resp_err(HTTP_RESP_CODE_302);
}

http_server_resp_t
http_server_resp_400(void)
{
    return http_server_resp_err(HTTP_RESP_CODE_400);
}

http_server_resp_t
http_server_resp_401_json(const http_server_resp_auth_json_t* const p_auth_json)
{
    return http_server_resp_err_json_in_static_mem(HTTP_RESP_CODE_401, p_auth_json->buf);
}

http_server_resp_t
http_server_resp_403_json(const http_server_resp_auth_json_t* const p_auth_json)
{
    return http_server_resp_err_json_in_static_mem(HTTP_RESP_CODE_403, p_auth_json->buf);
}

http_server_resp_t
http_server_resp_404(void)
{
    return http_server_resp_err(HTTP_RESP_CODE_404);
}

http_server_resp_t
http_server_resp_500(void)
{
    return http_server_resp_err(HTTP_RESP_CODE_500);
}

http_server_resp_t
http_server_resp_502(void)
{
    return http_server_resp_err(HTTP_RESP_CODE_502);
}

http_server_resp_t
http_server_resp_502_json_in_heap(const char* const p_json)
{
    return http_server_resp_err_json_in_heap(HTTP_RESP_CODE_502, p_json);
}

http_server_resp_t
http_server_resp_503(void)
{
    return http_server_resp_err(HTTP_RESP_CODE_503);
}

http_server_resp_t
http_server_resp_504(void)
{
    return http_server_resp_err(HTTP_RESP_CODE_504);
}

http_server_resp_t
http_server_resp_data_in_flash(
    const http_content_type_e     content_type,
    const char*                   p_content_type_param,
    const size_t                  content_len,
    const http_content_encoding_e content_encoding,
    const uint8_t*                p_buf,
    const bool                    flag_no_cache)
{
    const http_server_resp_t resp = {
        .http_resp_code       = HTTP_RESP_CODE_200,
        .content_location     = HTTP_CONTENT_LOCATION_FLASH_MEM,
        .flag_no_cache        = flag_no_cache,
        .flag_add_header_date = false,
        .content_type         = content_type,
        .p_content_type_param = p_content_type_param,
        .content_len          = content_len,
        .content_encoding     = content_encoding,
        .select_location      = {
            .memory = {
                .p_buf = p_buf,
            },
        },
    };
    return resp;
}

http_server_resp_t
http_server_resp_data_in_static_mem(
    const http_content_type_e     content_type,
    const char*                   p_content_type_param,
    const size_t                  content_len,
    const http_content_encoding_e content_encoding,
    const uint8_t*                p_buf,
    const bool                    flag_no_cache,
    const bool                    flag_add_header_date)
{
    const http_server_resp_t resp = {
        .http_resp_code       = HTTP_RESP_CODE_200,
        .content_location     = HTTP_CONTENT_LOCATION_STATIC_MEM,
        .flag_no_cache        = flag_no_cache,
        .flag_add_header_date = flag_add_header_date,
        .content_type         = content_type,
        .p_content_type_param = p_content_type_param,
        .content_len          = content_len,
        .content_encoding     = content_encoding,
        .select_location      = {
            .memory = {
                .p_buf = p_buf,
            },
        },
    };
    return resp;
}

http_server_resp_t
http_server_resp_data_in_heap(
    const http_resp_code_e        resp_code,
    const http_content_type_e     content_type,
    const char*                   p_content_type_param,
    const size_t                  content_len,
    const http_content_encoding_e content_encoding,
    const uint8_t*                p_buf,
    const bool                    flag_no_cache,
    const bool                    flag_add_header_date)
{
    const http_server_resp_t resp = {
        .http_resp_code       = resp_code,
        .content_location     = HTTP_CONTENT_LOCATION_HEAP,
        .flag_no_cache        = flag_no_cache,
        .flag_add_header_date = flag_add_header_date,
        .content_type         = content_type,
        .p_content_type_param = p_content_type_param,
        .content_len          = content_len,
        .content_encoding     = content_encoding,
        .select_location      = {
            .memory = {
                .p_buf = p_buf,
            },
        },
    };
    return resp;
}

http_server_resp_t
http_server_resp_200_data_in_heap(
    const http_content_type_e     content_type,
    const char*                   p_content_type_param,
    const size_t                  content_len,
    const http_content_encoding_e content_encoding,
    const uint8_t*                p_buf,
    const bool                    flag_no_cache,
    const bool                    flag_add_header_date)
{
    return http_server_resp_data_in_heap(
        HTTP_RESP_CODE_200,
        content_type,
        p_content_type_param,
        content_len,
        content_encoding,
        p_buf,
        flag_no_cache,
        flag_add_header_date);
}

http_server_resp_t
http_server_resp_data_from_file(
    http_resp_code_e              http_resp_code,
    const http_content_type_e     content_type,
    const char*                   p_content_type_param,
    const size_t                  content_len,
    const http_content_encoding_e content_encoding,
    const socket_t                fd,
    const bool                    flag_no_cache)
{
    const http_server_resp_t resp = {
        .http_resp_code       = http_resp_code,
        .content_location     = HTTP_CONTENT_LOCATION_FATFS,
        .flag_no_cache        = flag_no_cache,
        .flag_add_header_date = false,
        .content_type         = content_type,
        .p_content_type_param = p_content_type_param,
        .content_len          = content_len,
        .content_encoding     = content_encoding,
        .select_location      = {
            .fatfs = {
                .fd = fd,
            },
        },
    };
    return resp;
}

static void
http_server_fill_buf_with_random_u8(uint8_t* const p_buf, const size_t buf_size)
{
    for (uint32_t i = 0; i < buf_size; ++i)
    {
        p_buf[i] = (uint8_t)esp_random();
    }
}

static void
http_server_login_session_init(
    http_server_auth_ruuvi_login_session_t* p_login_session,
    const sta_ip_string_t* const            p_remote_ip)
{
    static const char g_ascii_upper[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    uint8_t           challenge_random[SHA256_DIGEST_SIZE];
    http_server_fill_buf_with_random_u8(challenge_random, sizeof(challenge_random) / sizeof(challenge_random[0]));
    wifiman_sha256_digest_t challenge_sha256;
    wifiman_sha256_calc(challenge_random, sizeof(challenge_random), &challenge_sha256);
    p_login_session->challenge = wifiman_sha256_hex_str(&challenge_sha256);
    p_login_session->remote_ip = *p_remote_ip;

    for (uint32_t i = 0; i < (HTTP_SERVER_AUTH_RUUVI_SESSION_ID_SIZE - 1); ++i)
    {
        p_login_session->session_id.buf[i] = g_ascii_upper[esp_random() % (sizeof(g_ascii_upper) - 1)];
    }
    p_login_session->session_id.buf[HTTP_SERVER_AUTH_RUUVI_SESSION_ID_SIZE - 1] = '\0';
}

const http_server_resp_auth_json_t*
http_server_fill_auth_json(
    const wifiman_hostinfo_t* const p_hostinfo,
    const http_server_auth_type_e   lan_auth_type,
    const char* const               p_err_message)
{
    if (NULL == p_err_message)
    {
        (void)snprintf(
            g_auth_json.buf,
            sizeof(g_auth_json.buf),
            "{\"gateway_name\": \"%s\", \"fw_ver\": \"%s\", \"nrf52_fw_ver\": \"%s\", \"lan_auth_type\": \"%s\"}",
            p_hostinfo->hostname.buf,
            p_hostinfo->fw_ver.buf,
            p_hostinfo->nrf52_fw_ver.buf,
            http_server_auth_type_to_str(lan_auth_type));
    }
    else
    {
        (void)snprintf(
            g_auth_json.buf,
            sizeof(g_auth_json.buf),
            "{\"gateway_name\": \"%s\", \"fw_ver\": \"%s\", \"nrf52_fw_ver\": \"%s\", \"lan_auth_type\": \"%s\", "
            "\"message\": \"%s\"}",
            p_hostinfo->hostname.buf,
            p_hostinfo->fw_ver.buf,
            p_hostinfo->nrf52_fw_ver.buf,
            http_server_auth_type_to_str(lan_auth_type),
            p_err_message);
    }
    return &g_auth_json;
}

const http_server_resp_auth_json_t*
http_server_fill_auth_json_bearer(const wifiman_hostinfo_t* const p_hostinfo)
{
    (void)snprintf(
        g_auth_json.buf,
        sizeof(g_auth_json.buf),
        "{\"gateway_name\": \"%s\", \"fw_ver\": \"%s\", \"nrf52_fw_ver\": \"%s\"}",
        p_hostinfo->hostname.buf,
        p_hostinfo->fw_ver.buf,
        p_hostinfo->nrf52_fw_ver.buf);
    return &g_auth_json;
}

http_server_resp_t
http_server_resp_401_auth_digest(
    const wifiman_hostinfo_t* const   p_hostinfo,
    http_header_extra_fields_t* const p_extra_header_fields)
{
    uint8_t nonce_random[HTTP_SERVER_AUTH_DIGEST_RANDOM_SIZE];
    http_server_fill_buf_with_random_u8(nonce_random, sizeof(nonce_random) / sizeof(nonce_random[0]));
    const wifiman_sha256_digest_hex_str_t nonce_random_sha256_str = wifiman_sha256_calc_hex_str(
        nonce_random,
        sizeof(nonce_random));
    if (wifiman_sha256_is_empty_digest_hex_str(&nonce_random_sha256_str))
    {
        return http_server_resp_503();
    }

    const wifiman_sha256_digest_hex_str_t opaque_sha256_str = wifiman_sha256_calc_hex_str(
        p_hostinfo->hostname.buf,
        strlen(p_hostinfo->hostname.buf));

    const http_server_resp_auth_json_t* const p_auth_json = http_server_fill_auth_json(
        p_hostinfo,
        HTTP_SERVER_AUTH_TYPE_DIGEST,
        NULL);
    (void)snprintf(
        p_extra_header_fields->buf,
        sizeof(p_extra_header_fields->buf),
        "WWW-Authenticate: Digest realm=\"%s\" qop=\"auth\" nonce=\"%s\" opaque=\"%s\"\r\n",
        p_hostinfo->hostname.buf,
        nonce_random_sha256_str.buf,
        opaque_sha256_str.buf);
    return http_server_resp_401_json(p_auth_json);
}

static void
http_server_resp_auth_ruuvi_prep_www_authenticate_header(
    const wifiman_hostinfo_t* const                     p_hostinfo,
    const http_server_auth_ruuvi_login_session_t* const p_login_session,
    http_header_extra_fields_t* const                   p_extra_header_fields)
{
    (void)snprintf(
        p_extra_header_fields->buf,
        sizeof(p_extra_header_fields->buf),
        "WWW-Authenticate: x-ruuvi-interactive realm=\"%s\" challenge=\"%s\" session_cookie=\"%s\" "
        "session_id=\"%s\"\r\n"
        "Set-Cookie: %s=%s\r\n",
        p_hostinfo->hostname.buf,
        p_login_session->challenge.buf,
        HTTP_SERVER_AUTH_RUUVI_COOKIE_SESSION,
        p_login_session->session_id.buf,
        HTTP_SERVER_AUTH_RUUVI_COOKIE_SESSION,
        p_login_session->session_id.buf);
}

http_server_resp_t
http_server_resp_401_auth_ruuvi_with_new_session_id(
    const sta_ip_string_t* const      p_remote_ip,
    const wifiman_hostinfo_t* const   p_hostinfo,
    http_header_extra_fields_t* const p_extra_header_fields,
    const bool                        flag_auth_default,
    const char* const                 p_err_message)
{
    http_server_auth_ruuvi_t* const               p_auth_info     = http_server_auth_ruuvi_get_info();
    http_server_auth_ruuvi_login_session_t* const p_login_session = &p_auth_info->login_session;

    http_server_login_session_init(p_login_session, p_remote_ip);

    if (wifiman_sha256_is_empty_digest_hex_str(&p_login_session->challenge))
    {
        return http_server_resp_503();
    }

    http_server_resp_auth_ruuvi_prep_www_authenticate_header(p_hostinfo, p_login_session, p_extra_header_fields);

    const http_server_resp_auth_json_t* const p_auth_json = http_server_fill_auth_json(
        p_hostinfo,
        flag_auth_default ? HTTP_SERVER_AUTH_TYPE_DEFAULT : HTTP_SERVER_AUTH_TYPE_RUUVI,
        p_err_message);
    return http_server_resp_401_json(p_auth_json);
}

http_server_resp_t
http_server_resp_401_auth_ruuvi(const wifiman_hostinfo_t* const p_hostinfo, const bool flag_auth_default)
{
    const http_server_resp_auth_json_t* const p_auth_json = http_server_fill_auth_json(
        p_hostinfo,
        flag_auth_default ? HTTP_SERVER_AUTH_TYPE_DEFAULT : HTTP_SERVER_AUTH_TYPE_RUUVI,
        NULL);
    return http_server_resp_401_json(p_auth_json);
}

http_server_resp_t
http_server_resp_403_auth_deny(const wifiman_hostinfo_t* const p_hostinfo)
{
    const http_server_resp_auth_json_t* const p_auth_json = http_server_fill_auth_json(
        p_hostinfo,
        HTTP_SERVER_AUTH_TYPE_DENY,
        NULL);
    return http_server_resp_403_json(p_auth_json);
}

http_server_resp_t
http_server_resp_403_forbidden(void)
{
    return http_server_resp_err(HTTP_RESP_CODE_403);
}
