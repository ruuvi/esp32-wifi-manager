/**
 * @file http_server_netconn_serve_handle_req.c
 * @author TheSomeMan
 * @date 2026-05-07
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_netconn_serve_handle_req.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "http_server_auth.h"
#include "http_server_handle_req.h"
#include "http_server_netconn_resp.h"
#include "http_server_internal.h"
#include "wifiman_config.h"
#include "wifi_manager.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"
static const char TAG[] = "http_server";

#define HTTP_SERVER_MAX_CONTENT_LEN_TO_PRINT_LOG_FOR_JSON_RESP (256U)

#define HTTP_SERVER_MAX_REQ_BUF_LEN_TO_LOG (128U)

static str_buf_t
http_server_netconn_log_req_and_extract_hostname(
    const http_req_info_t* const        p_req_info,
    const sta_ip_string_t* const        p_local_ip_str,
    const sta_ip_string_t* const        p_remote_ip_str,
    const wifiman_ip4_addr_str_t* const p_ap_ip_str,
    bool* const                         p_is_request_to_ap_ip)
{
    uint32_t          host_len = 0;
    const char* const p_host   = http_req_header_get_field(p_req_info->http_header, "Host:", &host_len);

    LOG_INFO(
        "Request from %s to %s (Host: %.*s): %s %s%s%s",
        p_remote_ip_str->buf,
        p_local_ip_str->buf,
        (printf_int_t)((NULL != p_host) ? host_len : 0),
        (NULL != p_host) ? p_host : "",
        (NULL != p_req_info->http_cmd.ptr) ? p_req_info->http_cmd.ptr : "NULL",
        (NULL != p_req_info->http_uri.ptr) ? p_req_info->http_uri.ptr : "NULL",
        (NULL != p_req_info->http_uri_params.ptr) ? "?" : "",
        (NULL != p_req_info->http_uri_params.ptr) ? p_req_info->http_uri_params.ptr : "");

    *p_is_request_to_ap_ip
        = ((host_len > 0) && (NULL != memmem(p_host, host_len, p_ap_ip_str->buf, strlen(p_ap_ip_str->buf))));

    const str_buf_t hostname = ((NULL != p_host) && (0 != host_len))
                                   ? str_buf_printf_with_alloc("%.*s", (printf_int_t)host_len, p_host)
                                   : str_buf_printf_with_alloc("%s", p_local_ip_str->buf);
    return hostname;
}

static void
http_server_netconn_log_req_info(const http_req_info_t* const p_req_info)
{
    (void)p_req_info;
    LOG_DBG("p_http_cmd: %s", p_req_info->http_cmd.ptr ? p_req_info->http_cmd.ptr : "NULL");
    LOG_DBG("p_http_uri: %s", p_req_info->http_uri.ptr ? p_req_info->http_uri.ptr : "NULL");
    LOG_DBG("p_http_uri_params: %s", p_req_info->http_uri_params.ptr ? p_req_info->http_uri_params.ptr : "NULL");
    LOG_DBG("p_http_ver: %s", p_req_info->http_ver.ptr ? p_req_info->http_ver.ptr : "NULL");
    LOG_DBG("p_http_header: %s", p_req_info->http_header.ptr ? p_req_info->http_header.ptr : "NULL");
    LOG_DBG("p_http_body: %s", p_req_info->http_body.ptr ? p_req_info->http_body.ptr : "NULL");
}

static void
http_server_netconn_log_json_resp(const http_server_resp_t* const p_resp)
{
    if (HTTP_CONTENT_TYPE_APPLICATION_JSON == p_resp->content_type)
    {
        const char* p_content_buf = NULL;
        switch (p_resp->content_location)
        {
            case HTTP_CONTENT_LOCATION_NO_CONTENT:
                LOG_WARN(
                    "Json resp: code=%u, content (len %zu): NO_CONTENT",
                    p_resp->http_resp_code,
                    p_resp->content_len);
                break;
            case HTTP_CONTENT_LOCATION_FLASH_MEM:
                p_content_buf = (const char*)p_resp->select_location.flash.p_buf;
                break;
            case HTTP_CONTENT_LOCATION_STATIC_MEM:
                p_content_buf = (const char*)p_resp->select_location.static_mem.p_buf;
                break;
            case HTTP_CONTENT_LOCATION_HEAP:
                p_content_buf = (const char*)p_resp->select_location.heap.p_buf;
                break;
            case HTTP_CONTENT_LOCATION_FATFS:
                LOG_INFO("Json resp: code=%u, content (len %zu): FATFS", p_resp->http_resp_code, p_resp->content_len);
                break;
            case HTTP_CONTENT_LOCATION_JSON_GENERATOR:
                LOG_INFO(
                    "Json resp: code=%u, content (len %zu): Json generator",
                    p_resp->http_resp_code,
                    p_resp->content_len);
                break;
        }
        if (NULL != p_content_buf)
        {
            if (p_resp->content_len <= HTTP_SERVER_MAX_CONTENT_LEN_TO_PRINT_LOG_FOR_JSON_RESP)
            {
                LOG_INFO(
                    "Json resp: code=%u, content:\n%.*s",
                    p_resp->http_resp_code,
                    (printf_int_t)p_resp->content_len,
                    p_content_buf);
            }
            else
            {
                LOG_INFO("Json resp: code=%u, content_len=%zu", p_resp->http_resp_code, p_resp->content_len);
            }
        }
    }
}

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
        http_server_netconn_resp_400(p_conn);
        return;
    }

    const wifiman_ip4_addr_str_t ap_ip_str = wifiman_config_ap_get_ip_str();

    const bool flag_access_from_lan = (0 != strcmp(p_local_ip_str->buf, ap_ip_str.buf)) ? true : false;

    bool      is_request_to_ap_ip = false;
    str_buf_t hostname            = http_server_netconn_log_req_and_extract_hostname(
        &req_info,
        p_local_ip_str,
        p_remote_ip_str,
        &ap_ip_str,
        &is_request_to_ap_ip);
    if (NULL == hostname.buf)
    {
        LOG_ERR("Failed to allocate memory for hostname string");
        http_server_netconn_resp_500(p_conn);
        return;
    }

    http_server_netconn_log_req_info(&req_info);

    if (flag_access_from_lan)
    {
        if (wifi_manager_is_req_from_lan_blocked_while_ap_is_active())
        {
            LOG_WARN("Request from LAN while WiFi hotspot is active - return HTTP error 503");
            http_server_netconn_resp_503(p_conn);
            str_buf_free_buf(&hostname);
            return;
        }
    }
    else
    {
        /* captive portal functionality: redirect to access point IP for HOST that are not the access point IP */
        if (!is_request_to_ap_ip)
        {
            http_server_netconn_resp_302(p_conn);
            str_buf_free_buf(&hostname);
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

    http_server_netconn_log_json_resp(&resp);
    if ((HTTP_RESP_CODE_401 == resp.http_resp_code) || (HTTP_RESP_CODE_403 == resp.http_resp_code))
    {
        const uint32_t wdog_period_ms = http_server_get_task_wdog_feed_period_ms();
        uint32_t       remaining_ms   = HTTP_SERVER_BRUTE_FORCE_PROTECTION_TIMEOUT_MS;
        http_server_task_wdt_reset();
        while (remaining_ms > 0U)
        {
            const uint32_t step_ms = (remaining_ms > wdog_period_ms) ? wdog_period_ms : remaining_ms;
            vTaskDelay(pdMS_TO_TICKS(step_ms));
            http_server_task_wdt_reset();
            remaining_ms -= step_ms;
        }
    }

    http_server_netconn_resp(p_conn, &resp, hostname.buf);
    str_buf_free_buf(&hostname);
}
