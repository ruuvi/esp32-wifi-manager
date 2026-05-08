/**
 * @file http_server_netconn_serve_handle_req.c
 * @author TheSomeMan
 * @date 2026-05-07
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_netconn_serve_handle_req.h"
#include <string.h>
#include "http_server_auth.h"
#include "http_server_handle_req.h"
#include "http_server_netconn_resp.h"
#include "wifiman_config.h"
#include "wifi_manager.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"
static const char TAG[] = "http_server";

#define HTTP_SERVER_MAX_CONTENT_LEN_TO_PRINT_LOG_FOR_JSON_RESP (256U)

#define HTTP_SERVER_MAX_REQ_BUF_LEN_TO_LOG (128U)

void
http_server_netconn_serve_handle_req(
    struct netconn* const        p_conn,
    char* const                  p_req_buf,
    const sta_ip_string_t* const p_local_ip_str,
    const sta_ip_string_t* const p_remote_ip_str)
{
    const http_req_info_t req_info = http_req_parse(p_req_buf);
    if (!req_info.is_success)
    {
        LOG_ERR(
            "Request from %s to %s: failed to parse request: %.*s",
            p_remote_ip_str->buf,
            p_local_ip_str->buf,
            (printf_int_t)HTTP_SERVER_MAX_REQ_BUF_LEN_TO_LOG,
            p_req_buf);
        http_server_netconn_resp_400(p_conn, NULL);
        return;
    }
    uint32_t          host_len = 0;
    const char* const p_host   = http_req_header_get_field(req_info.http_header, "Host:", &host_len);

    LOG_INFO(
        "Request from %s to %s (Host: %.*s): %s %s%s%s",
        p_remote_ip_str->buf,
        p_local_ip_str->buf,
        (printf_int_t)((NULL != p_host) ? host_len : 0),
        (NULL != p_host) ? p_host : "",
        (NULL != req_info.http_cmd.ptr) ? req_info.http_cmd.ptr : "NULL",
        (NULL != req_info.http_uri.ptr) ? req_info.http_uri.ptr : "NULL",
        (NULL != req_info.http_uri_params.ptr) ? "?" : "",
        (NULL != req_info.http_uri_params.ptr) ? req_info.http_uri_params.ptr : "");

    LOG_DBG("p_http_cmd: %s", req_info.http_cmd.ptr ? req_info.http_cmd.ptr : "NULL");
    LOG_DBG("p_http_uri: %s", req_info.http_uri.ptr ? req_info.http_uri.ptr : "NULL");
    LOG_DBG("p_http_uri_params: %s", req_info.http_uri_params.ptr ? req_info.http_uri_params.ptr : "NULL");
    LOG_DBG("p_http_ver: %s", req_info.http_ver.ptr ? req_info.http_ver.ptr : "NULL");
    LOG_DBG("p_http_header: %s", req_info.http_header.ptr ? req_info.http_header.ptr : "NULL");
    LOG_DBG("p_http_body: %s", req_info.http_body.ptr ? req_info.http_body.ptr : "NULL");

    const wifiman_ip4_addr_str_t ap_ip_str = wifiman_config_ap_get_ip_str();

    const bool flag_access_from_lan = (0 != strcmp(p_local_ip_str->buf, ap_ip_str.buf)) ? true : false;
    if (flag_access_from_lan)
    {
        if (wifi_manager_is_req_from_lan_blocked_while_ap_is_active())
        {
            LOG_WARN("Request from LAN while WiFi hotspot is active - return HTTP error 503");
            http_server_netconn_resp_503(p_conn, NULL);
            return;
        }
    }
    else
    {
        /* captive portal functionality: redirect to access point IP for HOST that are not the access point IP */
        const bool is_request_to_ap_ip
            = ((host_len > 0) && (NULL != memmem(p_host, host_len, ap_ip_str.buf, strlen(ap_ip_str.buf))));
        if (!is_request_to_ap_ip)
        {
            http_server_netconn_resp_302(p_conn);
            return;
        }
    }

    g_http_server_extra_header_fields.buf[0] = '\0';

    const http_server_handle_req_param_t param = {
        .p_req_info           = &req_info,
        .p_remote_ip          = p_remote_ip_str,
        .p_auth_info          = http_server_get_auth(),
        .flag_access_from_lan = flag_access_from_lan,
    };

    http_server_resp_t resp = http_server_handle_req(&param, &g_http_server_extra_header_fields);
    if ('\0' != g_http_server_extra_header_fields.buf[0])
    {
        LOG_INFO("Extra HTTP-header resp: %s", g_http_server_extra_header_fields.buf);
    }
    if ((HTTP_CONTENT_TYPE_APPLICATION_JSON == resp.content_type)
        && ((HTTP_CONTENT_LOCATION_STATIC_MEM == resp.content_location)
            || (HTTP_CONTENT_LOCATION_HEAP == resp.content_location)))
    {
        const size_t content_len = strlen((const char*)resp.select_location.memory.p_buf);
        if (content_len <= HTTP_SERVER_MAX_CONTENT_LEN_TO_PRINT_LOG_FOR_JSON_RESP)
        {
            LOG_INFO(
                "Json resp: code=%u, content:\n%s",
                resp.http_resp_code,
                (const char*)resp.select_location.memory.p_buf);
        }
        else
        {
            LOG_INFO("Json resp: code=%u, content_len=%lu", resp.http_resp_code, (printf_ulong_t)content_len);
        }
    }

    str_buf_t hostname = ((NULL != p_host) && (0 != host_len))
                             ? str_buf_printf_with_alloc("%.*s", (printf_int_t)host_len, p_host)
                             : str_buf_printf_with_alloc("%s", p_local_ip_str->buf);
    if (NULL == hostname.buf)
    {
        LOG_ERR("Failed to allocate memory for hostname string");
        http_server_netconn_resp_500(p_conn, NULL);
        return;
    }
    http_server_netconn_resp(p_conn, &resp, hostname.buf);
    str_buf_free_buf(&hostname);
}
