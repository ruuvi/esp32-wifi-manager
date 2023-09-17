/**
 * @file http_req.h
 * @author TheSomeMan
 * @date 2020-11-20
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_req.h"
#include <stddef.h>
#include <string.h>

http_req_info_t
http_req_parse(char* const p_req_buf)
{
    static const char g_two_crlf[] = "\r\n\r\n";
    static const char g_two_lf[]   = "\n\n";
    static const char g_one_crlf[] = "\r\n";
    static const char g_one_lf[]   = "\n";

    http_req_info_t req_info = {
        .is_success    = false,
        .http_cmd      = {
            .ptr = NULL,
        },
        .http_uri    = {
            .ptr = NULL,
        },
        .http_uri_params = {
            .ptr = NULL,
        },
        .http_ver    = {
            .ptr = NULL,
        },
        .http_header = {
            .ptr = NULL,
        },
        .http_body   = {
            .ptr = NULL,
        },
    };

    // find body
    char* p_ptr2 = strstr(p_req_buf, g_two_crlf);
    if (NULL != p_ptr2)
    {
        p_ptr2[strlen(g_one_crlf)] = '\0';
        req_info.http_body.ptr     = p_ptr2 + strlen(g_two_crlf);
    }
    else
    {
        p_ptr2 = strstr(p_req_buf, g_two_lf);
        if (NULL != p_ptr2)
        {
            p_ptr2[strlen(g_one_lf)] = '\0';
            req_info.http_body.ptr   = p_ptr2 + strlen(g_two_lf);
        }
        else
        {
            return req_info;
        }
    }

    // find header
    p_ptr2 = strstr(p_req_buf, g_one_crlf);
    if (NULL != p_ptr2)
    {
        *p_ptr2                  = '\0';
        req_info.http_header.ptr = p_ptr2 + strlen(g_one_crlf);
    }
    else
    {
        p_ptr2 = strstr(p_req_buf, g_one_lf);
        if (NULL != p_ptr2)
        {
            *p_ptr2                  = '\0';
            req_info.http_header.ptr = p_ptr2 + strlen(g_one_lf);
        }
        else
        {
            req_info.http_header.ptr = p_req_buf + strlen(p_req_buf);
        }
    }

    char* p_ptr1          = p_req_buf;
    req_info.http_cmd.ptr = p_ptr1;
    p_ptr2                = strchr(p_ptr1, ' ');
    if (NULL == p_ptr2)
    {
        return req_info;
    }
    *p_ptr2 = '\0';
    p_ptr1  = p_ptr2 + 1;

    req_info.http_uri.ptr = p_ptr1;
    p_ptr2                = strchr(p_ptr1, ' ');
    if (NULL == p_ptr2)
    {
        return req_info;
    }
    *p_ptr2 = '\0';

    req_info.http_ver.ptr = p_ptr2 + 1;

    p_ptr2 = strchr(req_info.http_uri.ptr, '?');
    if (NULL != p_ptr2)
    {
        *p_ptr2                      = '\0';
        req_info.http_uri_params.ptr = p_ptr2 + 1;
    }

    req_info.is_success = true;

    return req_info;
}

const char*
http_req_header_get_field(const http_req_header_t req_header, const char* const p_field_name, uint32_t* const p_len)
{
    *p_len = 0;

    const char* p_start = strcasestr(req_header.ptr, p_field_name);
    if (NULL == p_start)
    {
        return NULL;
    }
    const char* p_val = p_start + strlen(p_field_name);
    while (' ' == *p_val)
    {
        p_val += 1;
    }
    if ('"' == *p_val)
    {
        p_val += 1;
        const char* p_end = strpbrk(p_val, "\r\n");
        if (NULL == p_end)
        {
            return NULL;
        }
        if (p_end <= p_val)
        {
            return NULL;
        }
        p_end -= 1;
        if ('"' != *p_end)
        {
            return NULL;
        }
        *p_len = (uint32_t)(p_end - p_val);
    }
    else
    {
        const char* p_end = strpbrk(p_val, "\r\n");
        if (NULL == p_end)
        {
            return NULL;
        }
        *p_len = (uint32_t)(p_end - p_val);
    }

    return p_val;
}
