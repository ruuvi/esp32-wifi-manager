/**
 * @file http_server_auth_type.c
 * @author TheSomeMan
 * @date 2022-04-08
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_auth_type.h"
#include <string.h>
#include <assert.h>

#define HTTP_SERVER_AUTH_TYPE_STR_ALLOW   "lan_auth_allow"
#define HTTP_SERVER_AUTH_TYPE_STR_BASIC   "lan_auth_basic"
#define HTTP_SERVER_AUTH_TYPE_STR_DIGEST  "lan_auth_digest"
#define HTTP_SERVER_AUTH_TYPE_STR_RUUVI   "lan_auth_ruuvi"
#define HTTP_SERVER_AUTH_TYPE_STR_DENY    "lan_auth_deny"
#define HTTP_SERVER_AUTH_TYPE_STR_DEFAULT "lan_auth_default"

http_server_auth_type_e
http_server_auth_type_from_str(const char *const p_auth_type, bool *const p_flag_default)
{
    *p_flag_default                   = false;
    http_server_auth_type_e auth_type = HTTP_SERVER_AUTH_TYPE_DENY;
    if (0 == strcmp(HTTP_SERVER_AUTH_TYPE_STR_ALLOW, p_auth_type))
    {
        auth_type = HTTP_SERVER_AUTH_TYPE_ALLOW;
    }
    else if (0 == strcmp(HTTP_SERVER_AUTH_TYPE_STR_BASIC, p_auth_type))
    {
        auth_type = HTTP_SERVER_AUTH_TYPE_BASIC;
    }
    else if (0 == strcmp(HTTP_SERVER_AUTH_TYPE_STR_DIGEST, p_auth_type))
    {
        auth_type = HTTP_SERVER_AUTH_TYPE_DIGEST;
    }
    else if (0 == strcmp(HTTP_SERVER_AUTH_TYPE_STR_RUUVI, p_auth_type))
    {
        auth_type = HTTP_SERVER_AUTH_TYPE_RUUVI;
    }
    else if (0 == strcmp(HTTP_SERVER_AUTH_TYPE_STR_DENY, p_auth_type))
    {
        auth_type = HTTP_SERVER_AUTH_TYPE_DENY;
    }
    else if (0 == strcmp(HTTP_SERVER_AUTH_TYPE_STR_DEFAULT, p_auth_type))
    {
        auth_type       = HTTP_SERVER_AUTH_TYPE_RUUVI;
        *p_flag_default = true;
    }
    else
    {
        // MISRA C:2012, 15.7 - All if...else if constructs shall be terminated with an else statement
        auth_type = HTTP_SERVER_AUTH_TYPE_DENY;
    }
    return auth_type;
}

const char *
http_server_auth_type_to_str(const http_server_auth_type_e auth_type, const bool flag_use_default_auth)
{
    if (flag_use_default_auth)
    {
        assert(HTTP_SERVER_AUTH_TYPE_RUUVI == auth_type);
        return HTTP_SERVER_AUTH_TYPE_STR_DEFAULT;
    }
    switch (auth_type)
    {
        case HTTP_SERVER_AUTH_TYPE_ALLOW:
            return HTTP_SERVER_AUTH_TYPE_STR_ALLOW;
        case HTTP_SERVER_AUTH_TYPE_BASIC:
            return HTTP_SERVER_AUTH_TYPE_STR_BASIC;
        case HTTP_SERVER_AUTH_TYPE_DIGEST:
            return HTTP_SERVER_AUTH_TYPE_STR_DIGEST;
        case HTTP_SERVER_AUTH_TYPE_RUUVI:
            return HTTP_SERVER_AUTH_TYPE_STR_RUUVI;
        case HTTP_SERVER_AUTH_TYPE_DENY:
            return HTTP_SERVER_AUTH_TYPE_STR_DENY;
    }
    assert(0);
    return HTTP_SERVER_AUTH_TYPE_STR_DEFAULT;
}
