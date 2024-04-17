/**
 * @file http_server_auth.h
 * @author TheSomeMan
 * @date 2021-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef ESP32_WIFI_MANAGER_HTTP_SERVER_AUTH_H
#define ESP32_WIFI_MANAGER_HTTP_SERVER_AUTH_H

#include "http_server_auth_common.h"
#include "http_server_auth_digest.h"
#include "http_server_auth_ruuvi.h"
#include "http_server_auth_type.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct http_server_auth_info_t
{
    http_server_auth_type_e    auth_type;
    http_server_auth_user_t    auth_user;
    http_server_auth_pass_t    auth_pass;
    http_server_auth_api_key_t auth_api_key;
    http_server_auth_api_key_t auth_api_key_rw;
} http_server_auth_info_t;

typedef union http_server_auth_t
{
    http_server_auth_digest_req_t digest;
    http_server_auth_ruuvi_t      ruuvi;
} http_server_auth_t;

void
http_server_auth_clear_authorized_sessions(void);

http_server_auth_digest_req_t*
http_server_auth_digest_get_info(void);

http_server_auth_ruuvi_t*
http_server_auth_ruuvi_get_info(void);

const char*
http_server_strnstr(const char* const p_haystack, const char* const p_needle, const size_t len);

http_server_auth_info_t*
http_server_get_auth(void);

void
http_server_auth_ruuvi_add_authorized_session(
    http_server_auth_ruuvi_t* const                  p_auth_ruuvi,
    const http_server_auth_ruuvi_session_id_t* const p_session_id,
    const sta_ip_string_t* const                     p_remote_ip);

#ifdef __cplusplus
}
#endif

#endif // ESP32_WIFI_MANAGER_HTTP_SERVER_AUTH_H
