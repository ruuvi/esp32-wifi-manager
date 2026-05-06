/**
 * @file http_server_accept_and_handle_conn.c
 * @author TheSomeMan
 * @date 2021-11-20
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_accept_and_handle_conn.h"
#include <esp_task_wdt.h>
#include "lwip/api.h"
#include "lwip/priv/tcp_priv.h"
#include "os_sema.h"
#include "os_malloc.h"
#include "str_buf.h"
#include "wifiman_config.h"
#include "sta_ip.h"
#include "http_req.h"
#include "http_server_auth.h"
#include "http_server_handle_req.h"
#include "wifi_manager.h"
#include "http_server_mutex.h"
#include "wifi_manager_internal.h"
#include "http_server_netconn_resp.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#define HTTP_SERVER_MAX_REQUEST_SIZE (4U * 1024U)
#define HTTP_SERVER_MAX_CONTENT_SIZE (8U * 1024U)

#define HTTP_SERVER_REQUEST_TIMEOUT_MS (5 * 1000U)
#define HTTP_SERVER_CONTENT_TIMEOUT_MS (30 * 1000U)

#define HTTP_SERVER_LOG_DUMP_PRINT_MAX_LEN (2U * 1024U)

#define HTTP_SERVER_MAX_CONTENT_LEN_TO_PRINT_LOG_FOR_JSON_RESP (256U)

#define BASE_10 (10U)

typedef struct http_server_recv_ctx_t
{
    char*  p_req_buf;
    size_t req_buf_size;
    size_t accum_len;
    size_t header_len;
    size_t content_len;
    bool   is_ready;
    bool   is_header_completed;
} http_server_recv_ctx_t;

static const char TAG[] = "http_server";

static const char*
get_http_body(const char* const p_msg)
{
    static const char g_newlines[] = "\r\n\r\n";

    const char* p_body = strstr(p_msg, g_newlines);
    if (NULL == p_body)
    {
        LOG_DBG("http body not found: %s", p_msg);
        return NULL;
    }
    p_body += strlen(g_newlines);
    return p_body;
}

static bool
http_server_handle_received_buf_alloc_mem_for_first_frame(
    const char* const             p_buf,
    const u16_t                   buflen,
    http_server_recv_ctx_t* const p_ctx)
{
    p_ctx->is_header_completed = false;
    if (buflen > HTTP_SERVER_MAX_REQUEST_SIZE)
    {
        LOG_ERR(
            "Received request size %u exceeds maximum allowed %u",
            (printf_uint_t)buflen,
            (printf_uint_t)HTTP_SERVER_MAX_REQUEST_SIZE);
        return false;
    }
    LOG_DBG("Allocating request buffer for the first time, size: %u", (printf_uint_t)buflen);
    p_ctx->p_req_buf = os_malloc(buflen + 1);
    if (NULL == p_ctx->p_req_buf)
    {
        LOG_ERR("Failed to allocate %u bytes for request buffer", (printf_uint_t)buflen);
        return false;
    }
    p_ctx->req_buf_size = buflen;
    return true;
}

static bool
http_server_handle_received_buf_alloc_mem_for_non_first_frame(
    const char* const             p_buf,
    const u16_t                   buflen,
    http_server_recv_ctx_t* const p_ctx)
{
    if (p_ctx->req_buf_size == p_ctx->accum_len)
    {
        const size_t new_size = p_ctx->req_buf_size + buflen;
        if (new_size > HTTP_SERVER_MAX_REQUEST_SIZE)
        {
            LOG_ERR(
                "Can't fit new data to request buffer, max request size exceeded, accum_len: %zu, buf_len: %u, "
                "req_buf_size: %zu, max_request_size: %u",
                p_ctx->accum_len,
                (printf_uint_t)buflen,
                p_ctx->req_buf_size,
                (printf_uint_t)HTTP_SERVER_MAX_REQUEST_SIZE);
            os_free(p_ctx->p_req_buf);
            p_ctx->p_req_buf = NULL;
            return false;
        }
        LOG_DBG("Reallocating request buffer, old size: %zu, new size: %zu", p_ctx->req_buf_size, new_size);
        if (!os_realloc_safe_and_clean((void**)&p_ctx->p_req_buf, new_size + 1))
        {
            LOG_ERR(
                "Failed to reallocate request buffer to %u bytes (accum_len: %u, buf_len: %u)",
                (printf_uint_t)(p_ctx->accum_len + buflen),
                (printf_uint_t)p_ctx->accum_len,
                (printf_uint_t)buflen);
            p_ctx->p_req_buf = NULL;
            return false;
        }
        p_ctx->req_buf_size += buflen;
    }
    return true;
}

static bool
http_server_handle_request_content_len(http_server_recv_ctx_t* const p_ctx, const char* const p_content_len_str)
{
    p_ctx->content_len = (size_t)strtoul(p_content_len_str, NULL, BASE_10);
    LOG_DBG("Header Content-Length: %zu", p_ctx->content_len);
    if (p_ctx->content_len > HTTP_SERVER_MAX_CONTENT_SIZE)
    {
        LOG_ERR(
            "Content-Length %zu exceeds maximum allowed %u",
            p_ctx->content_len,
            (printf_uint_t)HTTP_SERVER_MAX_CONTENT_SIZE);
        os_free(p_ctx->p_req_buf);
        p_ctx->p_req_buf = NULL;
        return false;
    }
    const size_t size_of_header_and_body = p_ctx->header_len + p_ctx->content_len;
    if (p_ctx->req_buf_size < size_of_header_and_body)
    {
        LOG_DBG(
            "Reallocating request buffer to fit header and body, new size: %zu (header_len: %zu, content_len: "
            "%zu)",
            size_of_header_and_body,
            p_ctx->header_len,
            p_ctx->content_len);
        if (!os_realloc_safe_and_clean((void**)&p_ctx->p_req_buf, size_of_header_and_body + 1))
        {
            LOG_ERR(
                "Failed to reallocate request buffer to %zu bytes (header_len: %zu, content_len: %zu)",
                size_of_header_and_body,
                p_ctx->header_len,
                p_ctx->content_len);
            p_ctx->p_req_buf = NULL;
            return false;
        }
        p_ctx->req_buf_size = size_of_header_and_body;
    }
    return true;
}

static bool
http_server_handle_received_buf(const char* const p_buf, const u16_t buflen, http_server_recv_ctx_t* const p_ctx)
{
    if (NULL == p_ctx->p_req_buf)
    {
        if (!http_server_handle_received_buf_alloc_mem_for_first_frame(p_buf, buflen, p_ctx))
        {
            return false;
        }
    }
    else
    {
        if (!http_server_handle_received_buf_alloc_mem_for_non_first_frame(p_buf, buflen, p_ctx))
        {
            return false;
        }
    }
    if ((p_ctx->accum_len + buflen) > p_ctx->req_buf_size)
    {
        LOG_ERR(
            "Request buffer is full, can't fit new data, accum_len: %zu, buf_len: %u, req_buf_size: %zu",
            p_ctx->accum_len,
            (printf_uint_t)buflen,
            p_ctx->req_buf_size);
        os_free(p_ctx->p_req_buf);
        p_ctx->p_req_buf = NULL;
        return false;
    }
    memcpy(&p_ctx->p_req_buf[p_ctx->accum_len], p_buf, buflen);
    p_ctx->accum_len += buflen;
    p_ctx->p_req_buf[p_ctx->accum_len] = '\0'; // zero terminated string

    if (!p_ctx->is_header_completed)
    {
        const char* const p_body = get_http_body(p_ctx->p_req_buf);
        if (NULL != p_body)
        {
            p_ctx->is_header_completed = true;
            p_ctx->header_len          = (size_t)(p_body - p_ctx->p_req_buf);
        }
    }
    if (!p_ctx->is_header_completed)
    {
        LOG_DBG("Header not completed yet, waiting for more data, accum_len: %zu", p_ctx->accum_len);
        return true;
    }
    if (0 == p_ctx->content_len)
    {
        uint32_t   field_len                = 0;
        const char prev_char                = p_ctx->p_req_buf[p_ctx->header_len];
        p_ctx->p_req_buf[p_ctx->header_len] = '\0'; // zero terminate header for easier parsing
        const http_req_header_t req_header  = {
             .ptr = p_ctx->p_req_buf,
        };
        const char* const p_content_len_str = http_req_header_get_field(req_header, "Content-Length:", &field_len);
        p_ctx->p_req_buf[p_ctx->header_len] = prev_char; // restore original char after parsing header
        if (NULL != p_content_len_str)
        {
            if (!http_server_handle_request_content_len(p_ctx, p_content_len_str))
            {
                return false;
            }
        }
        else
        {
            LOG_DBG("Header Content-Length not found, assuming no body, request is full");
            p_ctx->is_ready = true;
            return true;
        }
    }

    if (p_ctx->accum_len < (p_ctx->header_len + p_ctx->content_len))
    {
        LOG_DBG(
            "Request not full yet, waiting for more data, accum_len: %zu, header_len: %zu, content_len: %zu",
            p_ctx->accum_len,
            p_ctx->header_len,
            p_ctx->content_len);
        return true;
    }
    if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG)
    {
        http_server_task_wdt_reset();
        size_t offset = 0;
        while (offset < p_ctx->req_buf_size)
        {
            const size_t rem_len   = p_ctx->req_buf_size - offset;
            const size_t print_len = (rem_len < HTTP_SERVER_LOG_DUMP_PRINT_MAX_LEN)
                                         ? rem_len
                                         : HTTP_SERVER_LOG_DUMP_PRINT_MAX_LEN;
            LOG_DUMP_DBG(
                (const uint8_t*)p_ctx->p_req_buf + offset,
                print_len,
                "Full request buffer (size: %zu), print from offset 0x%04zx",
                p_ctx->req_buf_size,
                offset);
            offset += print_len;
            http_server_task_wdt_reset();
        }
    }
    p_ctx->is_ready = true;
    return true;
}

static bool
http_server_recv_and_handle(struct netconn* const p_conn, http_server_recv_ctx_t* const p_ctx)
{
    struct netbuf* p_netbuf_in = NULL;

    const os_delta_ticks_t t0 = xTaskGetTickCount();

    const err_t err = netconn_recv(p_conn, &p_netbuf_in);

    const os_delta_ticks_t time_for_netconn_recv = xTaskGetTickCount() - t0;
    if (ERR_OK != err)
    {
        LOG_ERR("netconn recv: %d (time: %lu ticks)", (printf_int_t)err, (printf_ulong_t)time_for_netconn_recv);
        if (NULL != p_ctx->p_req_buf)
        {
            os_free(p_ctx->p_req_buf);
            p_ctx->p_req_buf = NULL;
        }
        return false;
    }

    char* p_buf  = NULL;
    u16_t buflen = 0;
    netbuf_data(p_netbuf_in, (void**)&p_buf, &buflen);
    LOG_DUMP_DBG((const uint8_t*)p_buf, buflen, "Received data (len: %u)", (printf_uint_t)buflen);

    const bool res = http_server_handle_received_buf(p_buf, buflen, p_ctx);

    netbuf_delete(p_netbuf_in);

    return res;
}

static void
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
            "Request from %s to %s: failed to parse request: %s",
            p_remote_ip_str->buf,
            p_local_ip_str->buf,
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
        host_len,
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
        const bool is_request_to_ap_ip = ((host_len > 0) && (NULL != strstr(p_host, ap_ip_str.buf)));
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

    str_buf_t hostname = ((NULL != p_host) && (0 != host_len)) ? str_buf_printf_with_alloc("%.*s", host_len, p_host)
                                                               : str_buf_printf_with_alloc("%s", p_local_ip_str->buf);
    http_server_netconn_resp(p_conn, &resp, hostname.buf);
    str_buf_free_buf(&hostname);
}

/**
 * @brief Helper function that processes one HTTP request at a time.
 * @param p_conn - ptr to a connection object
 */
static void
http_server_netconn_serve(struct netconn* const p_conn)
{
    sta_ip_string_t local_ip_str  = { '\0' };
    sta_ip_string_t remote_ip_str = { '\0' };

    const struct tcp_pcb* const p_tcp = p_conn->pcb.tcp;
    if (NULL == p_tcp)
    {
        LOG_ERR("p_conn->pcb.tcp is NULL due to race condition(1)");
        return;
    }

    const ip_addr_t local_ip  = p_tcp->local_ip;
    const ip_addr_t remote_ip = p_tcp->remote_ip;
    if (NULL == p_conn->pcb.tcp)
    {
        LOG_ERR("p_conn->pcb.tcp is NULL due to race condition(2)");
        return;
    }
    ipaddr_ntoa_r(&local_ip, local_ip_str.buf, sizeof(local_ip_str.buf));
    ipaddr_ntoa_r(&remote_ip, remote_ip_str.buf, sizeof(remote_ip_str.buf));

    http_server_recv_ctx_t ctx = {
        .p_req_buf           = NULL,
        .req_buf_size        = 0,
        .accum_len           = 0,
        .header_len          = 0,
        .content_len         = 0,
        .is_ready            = false,
        .is_header_completed = false,
    };

    LOG_DBG("New connection from %s to %s", remote_ip_str.buf, local_ip_str.buf);
    const os_delta_ticks_t t0 = xTaskGetTickCount();
    while (!ctx.is_ready)
    {
        const os_delta_ticks_t timeout_ticks = ctx.is_header_completed ? pdMS_TO_TICKS(HTTP_SERVER_CONTENT_TIMEOUT_MS)
                                                                       : pdMS_TO_TICKS(HTTP_SERVER_REQUEST_TIMEOUT_MS);
        if ((xTaskGetTickCount() - t0) > timeout_ticks)
        {
            LOG_ERR(
                "Connection from %s to %s: Timeout waiting for HTTP %s",
                remote_ip_str.buf,
                local_ip_str.buf,
                ctx.is_header_completed ? "content" : "request");
            break;
        }
        LOG_DBG("Wait for data");
        http_server_task_wdt_reset();
        if (!http_server_recv_and_handle(p_conn, &ctx))
        {
            LOG_DBG("http_server_recv_and_handle returned false");
            break;
        }
    }
    if (!ctx.is_ready)
    {
        LOG_WARN("Connection from %s to %s: The connection was closed", remote_ip_str.buf, local_ip_str.buf);
        if (NULL != ctx.p_req_buf)
        {
            os_free(ctx.p_req_buf);
        }
        return;
    }
    LOG_DBG(
        "Connection from %s to %s: Received request (%zu bytes): %.*s",
        remote_ip_str.buf,
        local_ip_str.buf,
        ctx.req_buf_size,
        (printf_int_t)ctx.req_buf_size,
        ctx.p_req_buf);

    http_server_task_wdt_reset();
    http_server_netconn_serve_handle_req(p_conn, ctx.p_req_buf, &local_ip_str, &remote_ip_str);
    os_free(ctx.p_req_buf);
}

void
http_server_accept_and_handle_conn(struct netconn* const p_conn)
{
    struct netconn* p_new_conn = NULL;

    os_mutex_t p_mutex = http_server_get_mutex();
    if ((NULL != p_mutex) && (!os_mutex_try_lock(p_mutex)))
    {
        LOG_DBG("Can't lock mutex, sleep for %u ms", HTTP_SERVER_ACCEPT_DELAY_MS);
        vTaskDelay(pdMS_TO_TICKS(HTTP_SERVER_ACCEPT_DELAY_MS));
        return;
    }

    const err_t err = netconn_accept(p_conn, &p_new_conn);

    if (ERR_OK != err)
    {
        if (NULL != p_mutex)
        {
            os_mutex_unlock(p_mutex);
        }
        if (ERR_TIMEOUT == err)
        {
            vTaskDelay(pdMS_TO_TICKS(HTTP_SERVER_ACCEPT_DELAY_MS));
        }
        else if (ERR_ABRT == err)
        {
            LOG_ERR("netconn_accept ERR_ABRT");
        }
        else
        {
            LOG_ERR("netconn_accept: %d", err);
        }
        return;
    }

    if (NULL == p_new_conn)
    {
        LOG_ERR("netconn_accept returned OK, but p_new_conn is NULL");
    }
    else if (NULL == p_conn->pcb.tcp)
    {
        // It seems that's a bug in netconn_accept, err is ERR_OK, p_new_conn is not NULL,
        // but p_conn->pcb.tcp is NULL.
        // Perhaps, err_tcp() was called. So the socked has already been closed.
        // As a workaround try to free resources and ignore this error.
        LOG_ERR("netconn_accept returned OK, but p_conn->pcb.tcp is NULL");
        netconn_delete(p_new_conn);
    }
    else
    {
#if LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG
        const os_delta_ticks_t t0 = xTaskGetTickCount();
#endif
        const int_fast32_t recv_timeout_ms = 3000;
        netconn_set_recvtimeout(p_new_conn, recv_timeout_ms);
        const int_fast32_t send_timeout_ms = 15000;
        netconn_set_sendtimeout(p_new_conn, send_timeout_ms);
        LOG_DBG("call http_server_netconn_serve");
        http_server_netconn_serve(p_new_conn);
        LOG_DBG("call netconn_close");
        const err_t err_close = netconn_close(p_new_conn);
        if (ESP_OK != err_close)
        {
            LOG_ERR_ESP(err_close, "%s failed (%s)", "netconn_close", conv_lwip_err_to_str(err_close));
        }
        LOG_DBG("call netconn_delete");
        const err_t err_delete = netconn_delete(p_new_conn);
        if (ESP_OK != err_delete)
        {
            LOG_ERR_ESP(err_delete, "%s failed", "netconn_delete");
        }
#if LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG
        const os_delta_ticks_t time_for_processing_request = xTaskGetTickCount() - t0;
        LOG_DBG("req processed for %u ticks", (printf_uint_t)time_for_processing_request);
#endif
    }

    if (NULL != p_mutex)
    {
        os_mutex_unlock(p_mutex);
    }
}
