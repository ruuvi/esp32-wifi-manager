/**
 * @file http_server_netconn_resp.c
 * @author TheSomeMan
 * @date 2026-05-07
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_netconn_resp.h"
#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <esp_task_wdt.h>
#include "lwip/api.h"
#include "os_malloc.h"
#include "str_buf.h"
#include "esp_type_wrapper.h"
#include "wifi_manager_defs.h"
#include "wifiman_config.h"
#include "http_server_resp.h"
#include "http_server_internal.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

static const char TAG[] = "http_server";

#if defined(RUUVI_TESTS_HTTP_SERVER_NETCONN_RESP) && RUUVI_TESTS_HTTP_SERVER_NETCONN_RESP
#define STATIC_RELEASE
#else
#define STATIC_RELEASE static
#endif

#define HTTP_SERVER_TX_CHUNK_SIZE (1536U)

#define HTTP_SERVER_MAX_CONTENT_LEN_TO_PRINT_LOG_FROM_JSON_GENERATOR (4U * 1024U)

#define HTTP_SERVER_DELAY_BETWEEN_NETCONN_WRITE_MS (5)

http_header_extra_fields_t g_http_server_extra_header_fields;

const char*
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

STATIC_RELEASE bool
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
        if (ERR_OK != err)
        {
            if (ERR_WOULDBLOCK != err)
            {
                LOG_ERR_ESP(
                    err,
                    "netconn_write_partly failed (%s), offset=%zu, size=%zu",
                    conv_lwip_err_to_str(err),
                    offset,
                    (size_t)(buf_len - offset));
                return false;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        LOG_DBG("netconn_write_partly: offset=%zu, bytes_written=%zu", offset, bytes_written);
        offset += bytes_written;
        if ((0 != send_timeout_ticks) && ((xTaskGetTickCount() - tick_start) > send_timeout_ticks))
        {
            LOG_ERR("netconn_write_partly failed: send timeout (%" PRId32 " ms)", (int32_t)p_conn->send_timeout);
            return false;
        }
        http_server_task_wdt_reset();
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
    LOG_DBG("netconn_write: %zu bytes", str_buf_get_len(&str_buf));

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

static bool
http_content_type_is_textual(const http_content_type_e content_type)
{
    switch (content_type)
    {
        case HTTP_CONTENT_TYPE_TEXT_HTML:
        case HTTP_CONTENT_TYPE_TEXT_PLAIN:
        case HTTP_CONTENT_TYPE_TEXT_CSS:
        case HTTP_CONTENT_TYPE_TEXT_JAVASCRIPT:
        case HTTP_CONTENT_TYPE_APPLICATION_JSON:
            return true;
        case HTTP_CONTENT_TYPE_IMAGE_PNG:
        case HTTP_CONTENT_TYPE_IMAGE_SVG_XML:
        case HTTP_CONTENT_TYPE_APPLICATION_OCTET_STREAM:
            return false;
    }
    return false;
}

static const char*
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
write_content_from_flash(struct netconn* const p_conn, const http_server_resp_t* const p_resp)
{
    LOG_DBG("netconn_write: %zu bytes", p_resp->content_len);
    const bool res = http_server_netconn_write(
        p_conn,
        p_resp->select_location.flash.p_buf,
        p_resp->content_len,
        NETCONN_NOCOPY);
    if (!res)
    {
        LOG_ERR("%s failed", "http_server_netconn_write");
    }
}

static void
write_content_from_static_mem(struct netconn* const p_conn, const http_server_resp_t* const p_resp)
{
    LOG_DBG("netconn_write: %zu bytes", p_resp->content_len);
    const bool res = http_server_netconn_write(
        p_conn,
        p_resp->select_location.static_mem.p_buf,
        p_resp->content_len,
        NETCONN_NOCOPY);
    if (!res)
    {
        LOG_ERR("%s failed", "http_server_netconn_write");
    }
}

static void
write_content_from_heap(struct netconn* const p_conn, const http_server_resp_t* const p_resp)
{
    LOG_DBG("netconn_write: %zu bytes", p_resp->content_len);
    const bool res = http_server_netconn_write(
        p_conn,
        p_resp->select_location.heap.p_buf,
        p_resp->content_len,
        NETCONN_COPY);
    if (!res)
    {
        LOG_ERR("%s failed", "http_server_netconn_write");
    }
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
    size_t rem_len = p_resp->content_len;
    while (rem_len > 0)
    {
        const size_t num_bytes       = (rem_len <= tmp_buf_size) ? rem_len : tmp_buf_size;
        const bool   flag_last_block = (num_bytes == rem_len) ? true : false;

        const file_read_result_t read_result = read(p_resp->select_location.fatfs.fd, p_tmp_buf, num_bytes);
        if (read_result < 0)
        {
            LOG_ERR("Failed to read %zu bytes", num_bytes);
            break;
        }
        if (read_result != num_bytes)
        {
            LOG_ERR("Read %d bytes, while requested %zu bytes", (printf_int_t)read_result, num_bytes);
            break;
        }
        rem_len -= read_result;
        uint8_t netconn_flags = (uint8_t)NETCONN_COPY;
        if (!flag_last_block)
        {
            netconn_flags |= (uint8_t)NETCONN_MORE;
        }
        LOG_DBG("netconn_write: %zu bytes", num_bytes);
        const bool res = http_server_netconn_write(p_conn, p_tmp_buf, num_bytes, netconn_flags);
        if (!res)
        {
            LOG_ERR("%s failed", "http_server_netconn_write");
            break;
        }
    }
    os_free(p_tmp_buf);
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
            LOG_INFO("json_stream_gen: send %zu bytes:\n%s", num_bytes, p_chunk);
        }
        else
        {
            LOG_DBG("json_stream_gen: send %zu bytes:\n%s", num_bytes, p_chunk);
        }
        const bool res = http_server_netconn_write(p_conn, p_chunk, num_bytes, netconn_flags);
        if (!res)
        {
            LOG_ERR("%s failed", "http_server_netconn_write");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(HTTP_SERVER_DELAY_BETWEEN_NETCONN_WRITE_MS)); // A delay to avoid triggering watchdog
    }
}

static void
http_server_write_content(struct netconn* const p_conn, const http_server_resp_t* const p_resp)
{
    switch (p_resp->content_location)
    {
        case HTTP_CONTENT_LOCATION_NO_CONTENT:
            break;
        case HTTP_CONTENT_LOCATION_FLASH_MEM:
            write_content_from_flash(p_conn, p_resp);
            break;
        case HTTP_CONTENT_LOCATION_STATIC_MEM:
            write_content_from_static_mem(p_conn, p_resp);
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
http_server_netconn_resp_with_content(
    struct netconn* const                   p_conn,
    const http_server_resp_t* const         p_resp,
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
    const http_header_date_str_t date_str            = http_server_gen_header_date_str(true);
    char                         content_len_buf[64] = { '\0' };
    if (SIZE_MAX != p_resp->content_len)
    {
        snprintf(content_len_buf, sizeof(content_len_buf), "Content-Length: %zu\r\n", p_resp->content_len);
    }

    const char* const p_content_type_str = http_get_content_type_str(p_resp->content_type);
    const char* const p_charset_str      = http_content_type_is_textual(p_resp->content_type) ? "; charset=utf-8" : "";
    const char* const p_extra_content_type_separator = use_extra_content_type_param ? "; " : "";
    const char* const p_extra_content_type_str       = use_extra_content_type_param ? p_resp->p_content_type_param : "";
    const char* const p_extra_header_fields_str = (NULL != p_extra_header_fields) ? p_extra_header_fields->buf : "";
    if (!http_server_netconn_printf(
            p_conn,
            true,
            "HTTP/1.0 %u %s\r\n"
            "Server: Ruuvi Gateway\r\n"
            "%s"
            "Content-type: %s%s%s%s\r\n"
            "%s"
            "%s"
            "%s"
            "%s"
            "\r\n",
            (printf_uint_t)resp_code,
            p_status_msg,
            date_str.buf,
            p_content_type_str,
            p_charset_str,
            p_extra_content_type_separator,
            p_extra_content_type_str,
            content_len_buf,
            p_extra_header_fields_str,
            http_get_content_encoding_str(p_resp),
            http_get_cache_control_str(p_resp)))
    {
        LOG_ERR("%s failed", "http_server_netconn_printf");
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
    const http_server_resp_t* const         p_resp,
    const http_header_extra_fields_t* const p_extra_header_fields)
{
    http_server_netconn_resp_with_content(p_conn, p_resp, p_extra_header_fields, HTTP_RESP_CODE_200, "OK");
}

void
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
    struct netconn* const           p_conn,
    const http_server_resp_t* const p_resp,
    const http_resp_code_e          resp_code,
    const char* const               p_status_msg)
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
http_server_netconn_resp_400_with_param(struct netconn* const p_conn, http_server_resp_t* const p_resp)
{
    http_server_netconn_resp_with_code(p_conn, p_resp, HTTP_RESP_CODE_400, "Bad Request");
}

void
http_server_netconn_resp_400(struct netconn* const p_conn)
{
    http_server_netconn_resp_400_with_param(p_conn, NULL);
}

static void
http_server_netconn_resp_401(
    struct netconn* const                   p_conn,
    const http_server_resp_t* const         p_resp,
    const http_header_extra_fields_t* const p_extra_header_fields)
{
    http_server_netconn_resp_with_content(p_conn, p_resp, p_extra_header_fields, HTTP_RESP_CODE_401, "Unauthorized");
}

static void
http_server_netconn_resp_403(
    struct netconn* const                   p_conn,
    const http_server_resp_t* const         p_resp,
    const http_header_extra_fields_t* const p_extra_header_fields)
{
    http_server_netconn_resp_with_content(p_conn, p_resp, p_extra_header_fields, HTTP_RESP_CODE_403, "Forbidden");
}

static void
http_server_netconn_resp_404(struct netconn* const p_conn, const http_server_resp_t* const p_resp)
{
    http_server_netconn_resp_with_code(p_conn, p_resp, HTTP_RESP_CODE_404, "Not Found");
}

static void
http_server_netconn_resp_409(struct netconn* const p_conn, const http_server_resp_t* const p_resp)
{
    http_server_netconn_resp_with_code(p_conn, p_resp, HTTP_RESP_CODE_409, "Conflict");
}

static void
http_server_netconn_resp_429(struct netconn* const p_conn, const http_server_resp_t* const p_resp)
{
    http_server_netconn_resp_with_code(p_conn, p_resp, HTTP_RESP_CODE_429, "Too Many Requests");
}

static void
http_server_netconn_resp_500_with_param(struct netconn* const p_conn, const http_server_resp_t* const p_resp)
{
    http_server_netconn_resp_with_code(p_conn, p_resp, HTTP_RESP_CODE_500, "Internal Server Error");
}

void
http_server_netconn_resp_500(struct netconn* const p_conn)
{
    http_server_netconn_resp_500_with_param(p_conn, NULL);
}

static void
http_server_netconn_resp_502_with_param(struct netconn* const p_conn, const http_server_resp_t* const p_resp)
{
    http_server_netconn_resp_with_code(p_conn, p_resp, HTTP_RESP_CODE_502, "Bad Gateway");
}

void
http_server_netconn_resp_502(struct netconn* const p_conn)
{
    http_server_netconn_resp_502_with_param(p_conn, NULL);
}

static void
http_server_netconn_resp_503_with_param(struct netconn* const p_conn, const http_server_resp_t* const p_resp)
{
    http_server_netconn_resp_with_code(p_conn, p_resp, HTTP_RESP_CODE_503, "Service Unavailable");
}

void
http_server_netconn_resp_503(struct netconn* const p_conn)
{
    http_server_netconn_resp_503_with_param(p_conn, NULL);
}

static void
http_server_netconn_resp_504_with_param(struct netconn* const p_conn, const http_server_resp_t* const p_resp)
{
    http_server_netconn_resp_with_code(p_conn, p_resp, HTTP_RESP_CODE_504, "Gateway timeout");
}

void
http_server_netconn_resp_504(struct netconn* const p_conn)
{
    http_server_netconn_resp_504_with_param(p_conn, NULL);
}

static void
http_server_netconn_resp_without_free_resp(
    struct netconn* const     p_conn,
    http_server_resp_t* const p_resp,
    const char* const         p_hostname)
{
    // Check that all enum values are handled at compile time
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
            http_server_netconn_resp_400_with_param(p_conn, p_resp);
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
            http_server_netconn_resp_500_with_param(p_conn, p_resp);
            return;
        case HTTP_RESP_CODE_502:
            http_server_netconn_resp_502_with_param(p_conn, p_resp);
            return;
        case HTTP_RESP_CODE_503:
            http_server_netconn_resp_503_with_param(p_conn, p_resp);
            return;
        case HTTP_RESP_CODE_504:
            http_server_netconn_resp_504_with_param(p_conn, p_resp);
            return;
    }
    LOG_ERR("Unsupported HTTP response code: %u", (printf_uint_t)p_resp->http_resp_code);
    // Return HTTP status 503 in release build mode
    assert(0);
    http_server_netconn_resp_503_with_param(p_conn, p_resp);
}

void
http_server_netconn_resp(struct netconn* const p_conn, http_server_resp_t* const p_resp, const char* const p_hostname)
{
    http_server_netconn_resp_without_free_resp(p_conn, p_resp, p_hostname);
    http_server_resp_free(p_resp);
}
