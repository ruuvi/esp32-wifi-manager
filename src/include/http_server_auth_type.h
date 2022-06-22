/**
 * @file http_server_auth_type.h
 * @author TheSomeMan
 * @date 2021-05-20
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef WIFI_MANAGER_HTTP_SERVER_AUTH_TYPE_H
#define WIFI_MANAGER_HTTP_SERVER_AUTH_TYPE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP_SERVER_AUTH_TYPE_LEN    20
#define HTTP_SERVER_AUTH_USER_LEN    51
#define HTTP_SERVER_AUTH_PASS_LEN    (64 + 1)
#define HTTP_SERVER_AUTH_API_KEY_LEN (256 + 1)

typedef enum http_server_auth_type_e
{
    HTTP_SERVER_AUTH_TYPE_ALLOW   = 0,
    HTTP_SERVER_AUTH_TYPE_BASIC   = 1,
    HTTP_SERVER_AUTH_TYPE_DIGEST  = 2,
    HTTP_SERVER_AUTH_TYPE_RUUVI   = 3,
    HTTP_SERVER_AUTH_TYPE_DENY    = 4,
    HTTP_SERVER_AUTH_TYPE_DEFAULT = 5,
} http_server_auth_type_e;

typedef struct http_server_auth_type_str_t
{
    char buf[HTTP_SERVER_AUTH_TYPE_LEN];
} http_server_auth_type_str_t;

typedef struct http_server_auth_user_t
{
    char buf[HTTP_SERVER_AUTH_USER_LEN];
} http_server_auth_user_t;

typedef struct http_server_auth_pass_t
{
    char buf[HTTP_SERVER_AUTH_PASS_LEN];
} http_server_auth_pass_t;

typedef struct http_server_auth_api_key_t
{
    char buf[HTTP_SERVER_AUTH_API_KEY_LEN];
} http_server_auth_api_key_t;

http_server_auth_type_e
http_server_auth_type_from_str(const char *const p_auth_type);

const char *
http_server_auth_type_to_str(const http_server_auth_type_e auth_type);

bool
http_server_set_auth(
    const http_server_auth_type_e           auth_type,
    const http_server_auth_user_t *const    p_auth_user,
    const http_server_auth_pass_t *const    p_auth_pass,
    const http_server_auth_api_key_t *const p_auth_api_key);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_HTTP_SERVER_AUTH_TYPE_H
