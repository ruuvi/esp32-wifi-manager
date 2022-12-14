/**
 * @file http_server_auth.c
 * @author TheSomeMan
 * @date 2021-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_auth.h"
#include "http_server_auth_type.h"
#include <string.h>
#include <stdio.h>

static http_server_auth_t      g_auth;
static http_server_auth_info_t g_auth_info;

void
http_server_auth_clear_authorized_sessions(void)
{
    memset(&g_auth, 0, sizeof(g_auth));
}

http_server_auth_digest_req_t*
http_server_auth_digest_get_info(void)
{
    return &g_auth.digest;
}

http_server_auth_ruuvi_t*
http_server_auth_ruuvi_get_info(void)
{
    return &g_auth.ruuvi;
}

const char*
http_server_strnstr(const char* const p_haystack, const char* const p_needle, const size_t len)
{
    const size_t needle_len = strnlen(p_needle, len);

    if (0 == needle_len)
    {
        return p_haystack;
    }

    const size_t max_offset = len - needle_len;

    for (size_t i = 0; i <= max_offset; ++i)
    {
        if (0 == strncmp(&p_haystack[i], p_needle, needle_len))
        {
            return &p_haystack[i];
        }
    }
    return NULL;
}

bool
http_server_set_auth(
    const http_server_auth_type_e           auth_type,
    const http_server_auth_user_t* const    p_auth_user,
    const http_server_auth_pass_t* const    p_auth_pass,
    const http_server_auth_api_key_t* const p_auth_api_key)
{
    http_server_auth_info_t* const p_auth_info                    = &g_auth_info;
    const char* const              p_auth_user_safe               = (NULL != p_auth_user) ? p_auth_user->buf : "";
    const char* const              p_auth_pass_safe               = (NULL != p_auth_pass) ? p_auth_pass->buf : "";
    bool                           flag_clear_authorized_sessions = false;
    if (auth_type != p_auth_info->auth_type)
    {
        p_auth_info->auth_type         = auth_type;
        flag_clear_authorized_sessions = true;
    }
    if (0 != strcmp(p_auth_info->auth_user.buf, p_auth_user_safe))
    {
        snprintf(p_auth_info->auth_user.buf, sizeof(p_auth_info->auth_user.buf), "%s", p_auth_user_safe);
        flag_clear_authorized_sessions = true;
    }
    if (0 != strcmp(p_auth_info->auth_pass.buf, p_auth_pass_safe))
    {
        snprintf(p_auth_info->auth_pass.buf, sizeof(p_auth_info->auth_pass.buf), "%s", p_auth_pass_safe);
        flag_clear_authorized_sessions = true;
    }

    if (flag_clear_authorized_sessions)
    {
        http_server_auth_clear_authorized_sessions();
    }

    snprintf(
        p_auth_info->auth_api_key.buf,
        sizeof(p_auth_info->auth_api_key.buf),
        "%s",
        (NULL != p_auth_api_key) ? p_auth_api_key->buf : "");
    return true;
}

http_server_auth_info_t*
http_server_get_auth(void)
{
    return &g_auth_info;
}
