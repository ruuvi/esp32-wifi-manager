/**
 * @file http_server_accept_and_handle_conn.c
 * @author TheSomeMan
 * @date 2021-11-20
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_accept_and_handle_conn.h"
#include <esp_task_wdt.h>
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

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#define HTTP_SERVER_MAX_REQUEST_SIZE (4U * 1024U)
#define HTTP_SERVER_MAX_CONTENT_SIZE (8U * 1024U)

#define HTTP_SERVER_TX_CHUNK_SIZE (1536U)

#define HTTP_SERVER_REQUEST_TIMEOUT_MS (5 * 1000U)
#define HTTP_SERVER_CONTENT_TIMEOUT_MS (30 * 1000U)

#define HTTP_SERVER_LOG_DUMP_PRINT_MAX_LEN (2U * 1024U)

#define HTTP_SERVER_MAX_CONTENT_LEN_TO_PRINT_LOG_FOR_JSON_RESP       (256U)
#define HTTP_SERVER_MAX_CONTENT_LEN_TO_PRINT_LOG_FROM_JSON_GENERATOR (4U * 1024U)

#define HTTP_SERVER_DELAY_BETWEEN_NETCONN_WRITE_MS (5)

#define HTTP_HEADER_DATE_EXAMPLE "Date: Thu, 01 Jan 2021 00:00:00 GMT\r\n"

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

typedef struct http_header_date_str_t
{
    char buf[sizeof(HTTP_HEADER_DATE_EXAMPLE)];
} http_header_date_str_t;

static const char TAG[] = "http_server";

static http_header_extra_fields_t g_http_server_extra_header_fields;

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
http_server_recv_and_handle(struct netconn* const p_conn, http_server_recv_ctx_t* const p_ctx)
{
    struct netbuf* p_netbuf_in = NULL;

    const os_delta_ticks_t t0                    = xTaskGetTickCount();
    const err_t            err                   = netconn_recv(p_conn, &p_netbuf_in);
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

    if (NULL == p_ctx->p_req_buf)
    {
        p_ctx->is_header_completed = false;
        if (buflen > HTTP_SERVER_MAX_REQUEST_SIZE)
        {
            LOG_ERR(
                "Received request size %u exceeds maximum allowed %u",
                (printf_uint_t)buflen,
                (printf_uint_t)HTTP_SERVER_MAX_REQUEST_SIZE);
            netbuf_delete(p_netbuf_in);
            return false;
        }
        LOG_DBG("Allocating request buffer for the first time, size: %u", (printf_uint_t)buflen);
        p_ctx->p_req_buf = os_malloc(buflen + 1);
        if (NULL == p_ctx->p_req_buf)
        {
            LOG_ERR("Failed to allocate %u bytes for request buffer", (printf_uint_t)buflen);
            netbuf_delete(p_netbuf_in);
            return false;
        }
        p_ctx->req_buf_size = buflen;
    }
    else
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
                netbuf_delete(p_netbuf_in);
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
                netbuf_delete(p_netbuf_in);
                p_ctx->p_req_buf = NULL;
                return false;
            }
            p_ctx->req_buf_size += buflen;
        }
    }
    if ((p_ctx->accum_len + buflen) > p_ctx->req_buf_size)
    {
        LOG_ERR(
            "Request buffer is full, can't fit new data, accum_len: %zu, buf_len: %u, req_buf_size: %zu",
            p_ctx->accum_len,
            (printf_uint_t)buflen,
            p_ctx->req_buf_size);
        netbuf_delete(p_netbuf_in);
        os_free(p_ctx->p_req_buf);
        p_ctx->p_req_buf = NULL;
        return false;
    }
    memcpy(&p_ctx->p_req_buf[p_ctx->accum_len], p_buf, buflen);
    p_ctx->accum_len += buflen;
    p_ctx->p_req_buf[p_ctx->accum_len] = '\0'; // zero terminated string

    netbuf_delete(p_netbuf_in);
    p_netbuf_in = NULL;

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
            p_ctx->content_len = (size_t)strtoul(p_content_len_str, NULL, 10);
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

static const char*
conv_lwip_err_to_str(const err_enum_t err)
{
    switch (err)
    {
        case ERR_OK:
            return "No error";
        case ERR_MEM:
            return "Out of memory error";
        case ERR_BUF:
            return "Buffer error";
        case ERR_TIMEOUT:
            return "Timeout";
        case ERR_RTE:
            return "Routing problem";
        case ERR_INPROGRESS:
            return "Operation in progress";
        case ERR_VAL:
            return "Illegal value";
        case ERR_WOULDBLOCK:
            return "Operation would block";
        case ERR_USE:
            return "Address in use";
        case ERR_ALREADY:
            return "Already connecting";
        case ERR_ISCONN:
            return "Conn already established";
        case ERR_CONN:
            return "Not connected";
        case ERR_IF:
            return "Low-level netif error";
        case ERR_ABRT:
            return "Connection aborted";
        case ERR_RST:
            return "Connection reset";
        case ERR_CLSD:
            return "Connection closed";
        case ERR_ARG:
            return "Illegal argument";
    }
    return "Unknown error";
}

static bool
http_server_netconn_write(
    struct netconn* const p_conn,
    const void* const     p_buf,
    const size_t          buf_len,
    const uint8_t         netconn_flags)
{
    /**
     * It's not enough to just set timeout with netconn_set_sendtimeout because if the WiFi connection is lost,
     * then netconn_write_partly will ignore p_conn->send_timeout and will wait much longer,
     * which will trigger task watchdog for http_server.
     */
    const TickType_t tick_start         = xTaskGetTickCount();
    const TickType_t send_timeout_ticks = (0 != p_conn->send_timeout) ? pdMS_TO_TICKS(p_conn->send_timeout) : 0;
    size_t           offset             = 0;
    do
    {
        size_t bytes_written = 0;

        http_server_sema_send_wait_immediate();
        const err_t err = netconn_write_partly(
            p_conn,
            &((const uint8_t*)p_buf)[offset],
            buf_len - offset,
            netconn_flags | (uint8_t)NETCONN_DONTBLOCK,
            &bytes_written);
        if (ESP_OK != err)
        {
            if (ERR_WOULDBLOCK != err)
            {
                LOG_ERR_ESP(
                    err,
                    "netconn_write_partly failed (%s), offset=%u, size=%u",
                    conv_lwip_err_to_str(err),
                    (printf_uint_t)offset,
                    (printf_uint_t)(buf_len - offset));
                return false;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        LOG_DBG(
            "netconn_write_partly: offset=%u, bytes_written=%u",
            (printf_uint_t)offset,
            (printf_uint_t)bytes_written);
        offset += bytes_written;
        if ((0 != send_timeout_ticks) && ((xTaskGetTickCount() - tick_start) > send_timeout_ticks))
        {
            LOG_ERR("netconn_write_partly failed: send timeout (%d ms)", (printf_int_t)p_conn->send_timeout);
            return false;
        }
        const esp_err_t err_wdt = esp_task_wdt_reset();
        if (ESP_OK != err_wdt)
        {
            LOG_ERR_ESP(err_wdt, "%s failed", "esp_task_wdt_reset");
        }
    } while (offset != buf_len);
    return true;
}

ATTR_PRINTF(3, 4)
static bool
http_server_netconn_printf(struct netconn* const p_conn, const bool flag_more, const char* const p_fmt, ...)
{
    va_list args;
    va_start(args, p_fmt);
    str_buf_t str_buf = str_buf_vprintf_with_alloc(p_fmt, args);
    va_end(args);
    if (NULL == str_buf.buf)
    {
        LOG_ERR("Can't allocate memory for buffer");
        return false;
    }
    LOG_DBG("Response: %s", str_buf.buf);
    uint8_t netconn_flags = (uint8_t)NETCONN_COPY;
    if (flag_more)
    {
        netconn_flags |= (uint8_t)NETCONN_MORE;
    }
    LOG_DBG("netconn_write: %u bytes", (printf_uint_t)str_buf_get_len(&str_buf));

    const bool res = http_server_netconn_write(p_conn, str_buf.buf, str_buf_get_len(&str_buf), netconn_flags);

    str_buf_free_buf(&str_buf);
    if (!res)
    {
        LOG_ERR("%s failed", "http_server_netconn_write");
        return false;
    }
    return true;
}

static const char*
http_get_content_type_str(const http_content_type_e content_type)
{
    const char* p_content_type_str = "application/octet-stream";
    switch (content_type)
    {
        case HTTP_CONTENT_TYPE_TEXT_HTML:
            p_content_type_str = "text/html";
            break;
        case HTTP_CONTENT_TYPE_TEXT_PLAIN:
            p_content_type_str = "text/plain";
            break;
        case HTTP_CONTENT_TYPE_TEXT_CSS:
            p_content_type_str = "text/css";
            break;
        case HTTP_CONTENT_TYPE_TEXT_JAVASCRIPT:
            p_content_type_str = "text/javascript";
            break;
        case HTTP_CONTENT_TYPE_IMAGE_PNG:
            p_content_type_str = "image/png";
            break;
        case HTTP_CONTENT_TYPE_IMAGE_SVG_XML:
            p_content_type_str = "image/svg+xml";
            break;
        case HTTP_CONTENT_TYPE_APPLICATION_JSON:
            p_content_type_str = "application/json";
            break;
        case HTTP_CONTENT_TYPE_APPLICATION_OCTET_STREAM:
            p_content_type_str = "application/octet-stream";
            break;
    }
    return p_content_type_str;
}

const char*
http_get_content_encoding_str(const http_server_resp_t* const p_resp)
{
    const char* p_content_encoding_str = "";
    switch (p_resp->content_encoding)
    {
        case HTTP_CONTENT_ENCODING_NONE:
            p_content_encoding_str = "";
            break;
        case HTTP_CONTENT_ENCODING_GZIP:
            p_content_encoding_str = "Content-Encoding: gzip\r\n";
            break;
    }
    return p_content_encoding_str;
}

static const char*
http_get_cache_control_str(const http_server_resp_t* const p_resp)
{
    const char* p_cache_control_str = "";
    if (p_resp->flag_no_cache)
    {
        p_cache_control_str
            = "Cache-Control: no-store, no-cache, must-revalidate, max-age=0\r\n"
              "Pragma: no-cache\r\n";
    }
    return p_cache_control_str;
}

static void
write_content_from_memory(struct netconn* const p_conn, const http_server_resp_t* const p_resp)
{
    LOG_DBG("netconn_write: %u bytes", p_resp->content_len);
    const bool res = http_server_netconn_write(
        p_conn,
        p_resp->select_location.memory.p_buf,
        p_resp->content_len,
        NETCONN_NOCOPY);
    if (!res)
    {
        LOG_ERR("%s failed", "http_server_netconn_write");
    }
}

static void
write_content_from_heap(struct netconn* const p_conn, http_server_resp_t* const p_resp)
{
    write_content_from_memory(p_conn, p_resp);
    os_free(p_resp->select_location.memory.p_buf);
}

static void
write_content_from_fatfs(struct netconn* const p_conn, const http_server_resp_t* const p_resp)
{
    const size_t tmp_buf_size = HTTP_SERVER_TX_CHUNK_SIZE;
    char*        p_tmp_buf    = os_malloc(tmp_buf_size);
    if (NULL == p_tmp_buf)
    {
        LOG_ERR("Can't allocate memory for temporary buffer");
        return;
    }
    uint32_t rem_len = p_resp->content_len;
    while (rem_len > 0)
    {
        const uint32_t num_bytes       = (rem_len <= tmp_buf_size) ? rem_len : tmp_buf_size;
        const bool     flag_last_block = (num_bytes == rem_len) ? true : false;

        const file_read_result_t read_result = read(p_resp->select_location.fatfs.fd, p_tmp_buf, num_bytes);
        if (read_result < 0)
        {
            LOG_ERR("Failed to read %u bytes", num_bytes);
            break;
        }
        if (read_result != num_bytes)
        {
            LOG_ERR("Read %u bytes, while requested %u bytes", read_result, num_bytes);
            break;
        }
        rem_len -= read_result;
        uint8_t netconn_flags = (uint8_t)NETCONN_COPY;
        if (!flag_last_block)
        {
            netconn_flags |= (uint8_t)NETCONN_MORE;
        }
        LOG_DBG("netconn_write: %u bytes", num_bytes);
        const bool res = http_server_netconn_write(p_conn, p_tmp_buf, num_bytes, netconn_flags);
        if (!res)
        {
            LOG_ERR("%s failed", "http_server_netconn_write");
            break;
        }
    }
    os_free(p_tmp_buf);
    LOG_DBG("Close file fd=%d", p_resp->select_location.fatfs.fd);
    close(p_resp->select_location.fatfs.fd);
}

static void
write_content_from_json_generator(struct netconn* const p_conn, const http_server_resp_t* const p_resp)
{
    json_stream_gen_t* p_json_gen = p_resp->select_location.json_generator.p_json_gen;

    size_t bytes_cnt = 0;
    while (true)
    {
        const char* p_chunk = json_stream_gen_get_next_chunk(p_json_gen);
        if (NULL == p_chunk)
        {
            LOG_ERR("json_stream_gen_get_next_chunk return error");
            break;
        }
        const size_t num_bytes = strlen(p_chunk);
        if (0 == num_bytes)
        {
            break;
        }
        bytes_cnt += num_bytes;

        uint8_t netconn_flags = (uint8_t)NETCONN_COPY;
        if (bytes_cnt < p_resp->content_len)
        {
            netconn_flags |= (uint8_t)NETCONN_MORE;
        }
        if (p_resp->content_len < HTTP_SERVER_MAX_CONTENT_LEN_TO_PRINT_LOG_FROM_JSON_GENERATOR)
        {
            LOG_INFO("json_stream_gen: send %u bytes:\n%s", num_bytes, p_chunk);
        }
        else
        {
            LOG_DBG("json_stream_gen: send %u bytes:\n%s", num_bytes, p_chunk);
        }
        const bool res = http_server_netconn_write(p_conn, p_chunk, num_bytes, netconn_flags);
        if (!res)
        {
            LOG_ERR("%s failed", "http_server_netconn_write");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(HTTP_SERVER_DELAY_BETWEEN_NETCONN_WRITE_MS)); // A delay to avoid triggering watchdog
    }
    json_stream_gen_delete(&p_json_gen);
}

static http_header_date_str_t
http_server_gen_header_date_str(const bool flag_gen_date)
{
    http_header_date_str_t date_str = { 0 };
    if (flag_gen_date)
    {
        const time_t cur_time = time(NULL);
        struct tm    tm_time  = { 0 };
        gmtime_r(&cur_time, &tm_time);
        (void)strftime(date_str.buf, sizeof(date_str.buf), "Date: %a, %d %b %Y %H:%M:%S GMT\r\n", &tm_time);
    }
    return date_str;
}

static void
http_server_write_content(struct netconn* const p_conn, http_server_resp_t* const p_resp)
{
    switch (p_resp->content_location)
    {
        case HTTP_CONTENT_LOCATION_NO_CONTENT:
            break;
        case HTTP_CONTENT_LOCATION_FLASH_MEM:
            ATTR_FALLTHROUGH;
        case HTTP_CONTENT_LOCATION_STATIC_MEM:
            write_content_from_memory(p_conn, p_resp);
            break;
        case HTTP_CONTENT_LOCATION_HEAP:
            write_content_from_heap(p_conn, p_resp);
            break;
        case HTTP_CONTENT_LOCATION_FATFS:
            write_content_from_fatfs(p_conn, p_resp);
            break;
        case HTTP_CONTENT_LOCATION_JSON_GENERATOR:
            write_content_from_json_generator(p_conn, p_resp);
            break;
    }
}

static void
http_server_netconn_resp_content_with_len(
    struct netconn* const                   p_conn,
    const http_server_resp_t* const         p_resp,
    const http_header_extra_fields_t* const p_extra_header_fields,
    const http_resp_code_e                  resp_code,
    const char* const                       p_status_msg,
    const bool                              use_extra_content_type_param,
    const http_header_date_str_t* const     p_date_str)
{
    if (!http_server_netconn_printf(
            p_conn,
            true,
            "HTTP/1.0 %u %s\r\n"
            "Server: Ruuvi Gateway\r\n"
            "%s"
            "Content-type: %s; charset=utf-8%s%s\r\n"
            "Content-Length: %lu\r\n"
            "%s"
            "%s"
            "%s"
            "\r\n",
            (printf_uint_t)resp_code,
            p_status_msg,
            p_date_str->buf,
            http_get_content_type_str(p_resp->content_type),
            use_extra_content_type_param ? "; " : "",
            use_extra_content_type_param ? p_resp->p_content_type_param : "",
            (printf_ulong_t)p_resp->content_len,
            (NULL != p_extra_header_fields) ? p_extra_header_fields->buf : "",
            http_get_content_encoding_str(p_resp),
            http_get_cache_control_str(p_resp)))
    {
        LOG_ERR("%s failed", "http_server_netconn_printf");
        return;
    }
}

static void
http_server_netconn_resp_content_without_len(
    struct netconn* const                   p_conn,
    const http_server_resp_t* const         p_resp,
    const http_header_extra_fields_t* const p_extra_header_fields,
    const http_resp_code_e                  resp_code,
    const char* const                       p_status_msg,
    const bool                              use_extra_content_type_param,
    const http_header_date_str_t* const     p_date_str)
{
    if (!http_server_netconn_printf(
            p_conn,
            true,
            "HTTP/1.0 %u %s\r\n"
            "Server: Ruuvi Gateway\r\n"
            "%s"
            "Content-type: %s; charset=utf-8%s%s\r\n"
            "%s"
            "%s"
            "%s"
            "\r\n",
            (printf_uint_t)resp_code,
            p_status_msg,
            p_date_str->buf,
            http_get_content_type_str(p_resp->content_type),
            use_extra_content_type_param ? "; " : "",
            use_extra_content_type_param ? p_resp->p_content_type_param : "",
            (NULL != p_extra_header_fields) ? p_extra_header_fields->buf : "",
            http_get_content_encoding_str(p_resp),
            http_get_cache_control_str(p_resp)))
    {
        LOG_ERR("%s failed", "http_server_netconn_printf");
        return;
    }
}

static void
http_server_netconn_resp_with_content(
    struct netconn* const                   p_conn,
    http_server_resp_t* const               p_resp,
    const http_header_extra_fields_t* const p_extra_header_fields,
    const http_resp_code_e                  resp_code,
    const char* const                       p_status_msg)
{
    if (HTTP_RESP_CODE_200 == resp_code)
    {
        LOG_INFO("Response: %s", p_status_msg);
    }
    else
    {
        LOG_WARN(
            "Response: status %u (%s), extra header fields:\n%s",
            (printf_uint_t)resp_code,
            p_status_msg,
            (NULL != p_extra_header_fields) ? p_extra_header_fields->buf : "");
    }
    const bool use_extra_content_type_param = (NULL != p_resp->p_content_type_param)
                                              && ('\0' != p_resp->p_content_type_param[0]);
    const http_header_date_str_t date_str = http_server_gen_header_date_str(true);
    if (SIZE_MAX != p_resp->content_len)
    {
        http_server_netconn_resp_content_with_len(
            p_conn,
            p_resp,
            p_extra_header_fields,
            resp_code,
            p_status_msg,
            use_extra_content_type_param,
            &date_str);
    }
    else
    {
        http_server_netconn_resp_content_without_len(
            p_conn,
            p_resp,
            p_extra_header_fields,
            resp_code,
            p_status_msg,
            use_extra_content_type_param,
            &date_str);
    }

    http_server_write_content(p_conn, p_resp);
}

static void
http_server_netconn_resp_without_content(
    struct netconn* const  p_conn,
    const http_resp_code_e resp_code,
    const char* const      p_status_msg)
{
    LOG_WARN("Response: status %u (%s)", (printf_uint_t)resp_code, p_status_msg);
    const char* const p_empty_json = "{}";
    if (!http_server_netconn_printf(
            p_conn,
            false,
            "HTTP/1.0 %u %s\r\n"
            "Server: Ruuvi Gateway\r\n"
            "Content-type: %s; charset=utf-8\r\n"
            "Content-Length: %lu\r\n"
            "\r\n"
            "%s",
            (printf_uint_t)resp_code,
            p_status_msg,
            http_get_content_type_str(HTTP_CONTENT_TYPE_APPLICATION_JSON),
            (printf_ulong_t)strlen(p_empty_json),
            p_empty_json))
    {
        LOG_ERR("%s failed", "http_server_netconn_printf");
    }
}

static void
http_server_netconn_resp_200(
    struct netconn* const                   p_conn,
    http_server_resp_t* const               p_resp,
    const http_header_extra_fields_t* const p_extra_header_fields)
{
    http_server_netconn_resp_with_content(p_conn, p_resp, p_extra_header_fields, HTTP_RESP_CODE_200, "OK");
}

static void
http_server_netconn_resp_302(struct netconn* const p_conn)
{
    const wifiman_ip4_addr_str_t ap_ip_str = wifiman_config_ap_get_ip_str();
    LOG_INFO("Response: status 302 (Found), URL=http://%s/", ap_ip_str.buf);
    if (!http_server_netconn_printf(
            p_conn,
            false,
            "HTTP/1.0 302 Found\r\n"
            "Server: Ruuvi Gateway\r\n"
            "Location: http://%s/\r\n"
            "\r\n",
            ap_ip_str.buf))
    {
        LOG_ERR("%s failed", "http_server_netconn_printf");
        return;
    }
}

static void
http_server_netconn_resp_301_auth_html(
    struct netconn* const                   p_conn,
    const char* const                       p_hostname,
    const http_header_extra_fields_t* const p_extra_header_fields)
{
    LOG_INFO("Response: status 301 (Moved Permanently), URL=http://%s/#auth", p_hostname);
    if (!http_server_netconn_printf(
            p_conn,
            false,
            "HTTP/1.0 301 Moved Permanently\r\n"
            "Server: Ruuvi Gateway\r\n"
            "Location: http://%s/#auth\r\n"
            "%s"
            "\r\n",
            p_hostname,
            (NULL != p_extra_header_fields) ? p_extra_header_fields->buf : ""))
    {
        LOG_ERR("%s failed", "http_server_netconn_printf");
        return;
    }
}

static void
http_server_netconn_resp_302_auth_html(
    struct netconn* const                   p_conn,
    const char* const                       p_hostname,
    const http_header_extra_fields_t* const p_extra_header_fields)
{
    LOG_INFO("Response: status 302 (Found), URL=http://%s/#auth", p_hostname);
    if (!http_server_netconn_printf(
            p_conn,
            false,
            "HTTP/1.0 302 Found\r\n"
            "Server: Ruuvi Gateway\r\n"
            "Location: http://%s/#auth\r\n"
            "%s"
            "\r\n",
            p_hostname,
            (NULL != p_extra_header_fields) ? p_extra_header_fields->buf : ""))
    {
        LOG_ERR("%s failed", "http_server_netconn_printf");
        return;
    }
}

static void
http_server_netconn_resp_with_code(
    struct netconn* const     p_conn,
    http_server_resp_t* const p_resp,
    const http_resp_code_e    resp_code,
    const char* const         p_status_msg)
{
    if ((NULL == p_resp) || (0 == p_resp->content_len))
    {
        http_server_netconn_resp_without_content(p_conn, resp_code, p_status_msg);
    }
    else
    {
        http_server_netconn_resp_with_content(p_conn, p_resp, NULL, resp_code, p_status_msg);
    }
}

static void
http_server_netconn_resp_400(struct netconn* const p_conn, http_server_resp_t* const p_resp)
{
    http_server_netconn_resp_with_code(p_conn, p_resp, HTTP_RESP_CODE_400, "Bad Request");
}

static void
http_server_netconn_resp_401(
    struct netconn* const                   p_conn,
    http_server_resp_t* const               p_resp,
    const http_header_extra_fields_t* const p_extra_header_fields)
{
    http_server_netconn_resp_with_content(p_conn, p_resp, p_extra_header_fields, HTTP_RESP_CODE_401, "Unauthorized");
}

static void
http_server_netconn_resp_403(
    struct netconn* const                   p_conn,
    http_server_resp_t* const               p_resp,
    const http_header_extra_fields_t* const p_extra_header_fields)
{
    http_server_netconn_resp_with_content(p_conn, p_resp, p_extra_header_fields, HTTP_RESP_CODE_403, "Forbidden");
}

static void
http_server_netconn_resp_404(struct netconn* const p_conn, http_server_resp_t* const p_resp)
{
    http_server_netconn_resp_with_code(p_conn, p_resp, HTTP_RESP_CODE_404, "Not Found");
}

static void
http_server_netconn_resp_409(struct netconn* const p_conn, http_server_resp_t* const p_resp)
{
    http_server_netconn_resp_with_code(p_conn, p_resp, HTTP_RESP_CODE_409, "Conflict");
}

static void
http_server_netconn_resp_429(struct netconn* const p_conn, http_server_resp_t* const p_resp)
{
    http_server_netconn_resp_with_code(p_conn, p_resp, HTTP_RESP_CODE_429, "Too Many Requests");
}

static void
http_server_netconn_resp_500(struct netconn* const p_conn, http_server_resp_t* const p_resp)
{
    http_server_netconn_resp_with_code(p_conn, p_resp, HTTP_RESP_CODE_500, "Internal Server Error");
}

static void
http_server_netconn_resp_502(struct netconn* const p_conn, http_server_resp_t* const p_resp)
{
    http_server_netconn_resp_with_code(p_conn, p_resp, HTTP_RESP_CODE_502, "Bad Gateway");
}

static void
http_server_netconn_resp_503(struct netconn* const p_conn, http_server_resp_t* const p_resp)
{
    http_server_netconn_resp_with_code(p_conn, p_resp, HTTP_RESP_CODE_503, "Service Unavailable");
}

static void
http_server_netconn_resp_504(struct netconn* const p_conn, http_server_resp_t* const p_resp)
{
    http_server_netconn_resp_with_code(p_conn, p_resp, HTTP_RESP_CODE_504, "Gateway timeout");
}

static void
http_server_netconn_resp(struct netconn* const p_conn, http_server_resp_t* const p_resp, const char* const p_hostname)
{
    switch (p_resp->http_resp_code)
    {
        case HTTP_RESP_CODE_206: // Server supports only HTTP/1.0, so fall back to HTTP status 200 for partial content
            p_resp->http_resp_code = HTTP_RESP_CODE_200;
            LOG_WARN("Falling back to HTTP/1.0 status code 200 for partial content");
            ATTR_FALLTHROUGH;
        case HTTP_RESP_CODE_200:
            ATTR_FALLTHROUGH;
        case HTTP_RESP_CODE_299:
            http_server_netconn_resp_200(p_conn, p_resp, &g_http_server_extra_header_fields);
            return;
        case HTTP_RESP_CODE_301:
            http_server_netconn_resp_301_auth_html(p_conn, p_hostname, &g_http_server_extra_header_fields);
            return;
        case HTTP_RESP_CODE_302:
            http_server_netconn_resp_302_auth_html(p_conn, p_hostname, &g_http_server_extra_header_fields);
            return;
        case HTTP_RESP_CODE_400:
            http_server_netconn_resp_400(p_conn, p_resp);
            return;
        case HTTP_RESP_CODE_401:
            http_server_netconn_resp_401(p_conn, p_resp, &g_http_server_extra_header_fields);
            return;
        case HTTP_RESP_CODE_403:
            http_server_netconn_resp_403(p_conn, p_resp, &g_http_server_extra_header_fields);
            return;
        case HTTP_RESP_CODE_404:
            http_server_netconn_resp_404(p_conn, p_resp);
            return;
        case HTTP_RESP_CODE_409:
            http_server_netconn_resp_409(p_conn, p_resp);
            return;
        case HTTP_RESP_CODE_429:
            http_server_netconn_resp_429(p_conn, p_resp);
            return;
        case HTTP_RESP_CODE_500:
            http_server_netconn_resp_500(p_conn, p_resp);
            return;
        case HTTP_RESP_CODE_502:
            http_server_netconn_resp_502(p_conn, p_resp);
            return;
        case HTTP_RESP_CODE_503:
            http_server_netconn_resp_503(p_conn, p_resp);
            return;
        case HTTP_RESP_CODE_504:
            http_server_netconn_resp_504(p_conn, p_resp);
            return;
    }
    LOG_ERR("Unsupported HTTP response code: %u", (printf_uint_t)p_resp->http_resp_code);
    assert(0);
    http_server_netconn_resp_503(p_conn, p_resp);
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
