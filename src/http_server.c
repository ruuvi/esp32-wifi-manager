#include <limits.h>
/*
Copyright (c) 2017-2019 Tony Pottier

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

@file http_server.c
@author Tony Pottier
@brief Defines all functions necessary for the HTTP server to run.

Contains the freeRTOS task for the HTTP listener and all necessary support
function to process requests, decode URLs, serve files, etc. etc.

@note http_server task cannot run without the wifi_manager task!
@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#include "http_server.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "lwip/netbuf.h"
#include "mdns.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/opt.h"
#include "lwip/memp.h"
#include "lwip/ip.h"
#include "lwip/raw.h"
#include "lwip/udp.h"
#include "lwip/priv/api_msg.h"
#include "lwip/priv/tcp_priv.h"
#include "lwip/priv/tcpip_priv.h"

#include "wifi_manager_internal.h"
#include "wifi_manager.h"
#include "json_access_points.h"
#include "json_network_info.h"

#include "cJSON.h"
#include "sta_ip_safe.h"
#include "os_task.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

#define RUUVI_GWUI_HTML_ENABLE 1

#define FULLBUF_SIZE 4 * 1024

typedef int file_read_result_t;

/**
 * @brief RTOS task for the HTTP server. Do not start manually.
 * @see void http_server_start()
 */
static _Noreturn void
http_server_task(void *pvParameters);

//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
/* @brief tag used for ESP serial console messages */
static const char TAG[] = "http_server";

/* @brief task handle for the http server */
static os_task_handle_t task_http_server;

static char *
http_server_strdupVprintf(size_t *pLen, const char *fmt, va_list args)
{
    const int len = vsnprintf(NULL, 0, fmt, args);
    char *    buf = malloc(len + 1);
    vsnprintf(buf, len + 1, fmt, args);
    if (NULL != pLen)
    {
        *pLen = len;
    }
    return buf;
}

__attribute__((format(printf, 3, 4))) //
static void
http_server_netconn_printf(struct netconn *conn, bool flag_more, const char *fmt, ...)
{
    size_t  len = 0;
    va_list args;
    va_start(args, fmt);
    char *buf = http_server_strdupVprintf(&len, fmt, args);
    va_end(args);
    if (NULL == buf)
    {
        return;
    }
    ESP_LOGD(TAG, "Respond: %s", buf);
    uint8_t netconn_flags = (uint8_t)NETCONN_COPY;
    if (flag_more)
    {
        netconn_flags |= (uint8_t)NETCONN_MORE;
    }
    netconn_write(conn, buf, len, netconn_flags);
    free(buf);
}

static const char *
http_get_content_type_str(const http_content_type_e content_type)
{
    const char *content_type_str = "application/octet-stream";
    switch (content_type)
    {
        case HTTP_CONENT_TYPE_TEXT_HTML:
            content_type_str = "text/html";
            break;
        case HTTP_CONENT_TYPE_TEXT_PLAIN:
            content_type_str = "text/plain";
            break;
        case HTTP_CONENT_TYPE_TEXT_CSS:
            content_type_str = "text/css";
            break;
        case HTTP_CONENT_TYPE_TEXT_JAVASCRIPT:
            content_type_str = "text/javascript";
            break;
        case HTTP_CONENT_TYPE_IMAGE_PNG:
            content_type_str = "image/png";
            break;
        case HTTP_CONENT_TYPE_IMAGE_SVG_XML:
            content_type_str = "image/svg+xml";
            break;
        case HTTP_CONENT_TYPE_APPLICATION_JSON:
            content_type_str = "application/json";
            break;
        case HTTP_CONENT_TYPE_APPLICATION_OCTET_STREAM:
            content_type_str = "application/octet-stream";
            break;
    }
    return content_type_str;
}

const char *
http_get_content_encoding_str(const http_server_resp_t *p_resp)
{
    const char *content_encoding_str = "";
    switch (p_resp->content_encoding)
    {
        case HTTP_CONENT_ENCODING_NONE:
            content_encoding_str = "";
            break;
        case HTTP_CONENT_ENCODING_GZIP:
            content_encoding_str = "Content-Encoding: gzip\n";
            break;
    }
    return content_encoding_str;
}

const char *
http_get_cache_control_str(const http_server_resp_t *p_resp)
{
    const char *cache_control_str = "";
    if (p_resp->flag_no_cache)
    {
        cache_control_str
            = "Cache-Control: no-store, no-cache, must-revalidate, max-age=0\r\n"
              "Pragma: no-cache\r\n";
    }
    return cache_control_str;
}

static void
write_content_from_flash(struct netconn *conn, const http_server_resp_t *p_resp)
{
    const err_t err = netconn_write(conn, p_resp->select_location.memory.p_buf, p_resp->content_len, NETCONN_NOCOPY);
    if (ERR_OK != err)
    {
        ESP_LOGE(TAG, "netconn_write failed, err=%d", err);
    }
}

static void
write_content_from_static_mem(struct netconn *conn, const http_server_resp_t *p_resp)
{
    const err_t err = netconn_write(conn, p_resp->select_location.memory.p_buf, p_resp->content_len, NETCONN_COPY);
    if (ERR_OK != err)
    {
        ESP_LOGE(TAG, "netconn_write failed, err=%d", err);
    }
}

static void
write_content_from_heap(struct netconn *conn, const http_server_resp_t *p_resp)
{
    const err_t err = netconn_write(conn, p_resp->select_location.memory.p_buf, p_resp->content_len, NETCONN_COPY);
    if (ERR_OK != err)
    {
        ESP_LOGE(TAG, "netconn_write failed, err=%d", err);
    }
    free((void *)p_resp->select_location.memory.p_buf);
}

static void
write_content_from_fatfs(struct netconn *conn, const http_server_resp_t *p_resp)
{
    static char tmp_buf[512U];
    uint32_t    rem_len = p_resp->content_len;
    while (rem_len > 0)
    {
        const uint32_t num_bytes       = (rem_len <= sizeof(tmp_buf)) ? rem_len : sizeof(tmp_buf);
        const bool     flag_last_block = (num_bytes == rem_len) ? true : false;

        const file_read_result_t read_result = read(p_resp->select_location.fatfs.fd, tmp_buf, num_bytes);
        if (read_result < 0)
        {
            ESP_LOGE(TAG, "Failed to read %u bytes", num_bytes);
            break;
        }
        if (read_result != num_bytes)
        {
            ESP_LOGE(TAG, "Read %u bytes, while requested %u bytes", read_result, num_bytes);
            break;
        }
        rem_len -= read_result;
        uint8_t netconn_flags = (uint8_t)NETCONN_COPY;
        if (!flag_last_block)
        {
            netconn_flags |= (uint8_t)NETCONN_MORE;
        }
        ESP_LOGD(TAG, "Send %u bytes", num_bytes);
        const err_t err = netconn_write(conn, tmp_buf, num_bytes, netconn_flags);
        if (ERR_OK != err)
        {
            ESP_LOGE(TAG, "netconn_write failed, err=%d", err);
            break;
        }
    }
    ESP_LOGD(TAG, "Close file fd=%d", p_resp->select_location.fatfs.fd);
    close(p_resp->select_location.fatfs.fd);
}

static void
http_server_netconn_resp_200(struct netconn *conn, const http_server_resp_t *p_resp)
{
    const bool use_extra_content_type_param = (NULL != p_resp->p_content_type_param)
                                              && ('\0' != p_resp->p_content_type_param[0]);

    http_server_netconn_printf(
        conn,
        true,
        "HTTP/1.1 200 OK\n"
        "Content-type: %s; charset=utf-8%s%s\n"
        "Content-Length: %u\n"
        "%s"
        "%s"
        "\n",
        http_get_content_type_str(p_resp->content_type),
        use_extra_content_type_param ? "; " : "",
        use_extra_content_type_param ? p_resp->p_content_type_param : "",
        (uint32_t)p_resp->content_len,
        http_get_content_encoding_str(p_resp),
        http_get_cache_control_str(p_resp));

    switch (p_resp->content_location)
    {
        case HTTP_CONTENT_LOCATION_NO_CONTENT:
            break;
        case HTTP_CONTENT_LOCATION_FLASH_MEM:
            write_content_from_flash(conn, p_resp);
            break;
        case HTTP_CONTENT_LOCATION_STATIC_MEM:
            write_content_from_static_mem(conn, p_resp);
            break;
        case HTTP_CONTENT_LOCATION_HEAP:
            write_content_from_heap(conn, p_resp);
            break;
        case HTTP_CONTENT_LOCATION_FATFS:
            write_content_from_fatfs(conn, p_resp);
            break;
    }
}

static http_server_resp_t
http_server_resp_200_json(const char *json_content)
{
    const bool flag_no_cache = true;
    return http_server_resp_data_in_static_mem(
        HTTP_CONENT_TYPE_APPLICATION_JSON,
        NULL,
        strlen(json_content),
        HTTP_CONENT_ENCODING_NONE,
        (const uint8_t *)json_content,
        flag_no_cache);
}

static void
http_server_netconn_resp_302(struct netconn *conn)
{
    ESP_LOGI(TAG, "Respond: 302 Found");
    http_server_netconn_printf(
        conn,
        false,
        "HTTP/1.1 302 Found\n"
        "Location: http://%s/\n"
        "\n",
        DEFAULT_AP_IP);
}

static void
http_server_netconn_resp_400(struct netconn *conn)
{
    ESP_LOGW(TAG, "Respond: 400 Bad Request");
    http_server_netconn_printf(
        conn,
        false,
        "HTTP/1.1 400 Bad Request\r\n"
        "Content-Length: 0\r\n"
        "\r\n");
}

static void
http_server_netconn_resp_404(struct netconn *conn)
{
    ESP_LOGW(TAG, "Respond: 404 Not Found");
    http_server_netconn_printf(
        conn,
        false,
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Length: 0\r\n"
        "\r\n");
}

static void
http_server_netconn_resp_503(struct netconn *conn)
{
    ESP_LOGW(TAG, "Respond: 503 Service Unavailable");
    http_server_netconn_printf(
        conn,
        false,
        "HTTP/1.1 503 Service Unavailable\r\n"
        "Content-Length: 0\r\n"
        "\r\n");
}

void
http_server_start(void)
{
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    if (task_http_server == NULL)
    {
        ESP_LOGI(TAG, "Run http_server");
        if (!os_task_create(
                &http_server_task,
                "http_server",
                20 * 1024,
                NULL,
                WIFI_MANAGER_TASK_PRIORITY - 1,
                &task_http_server))
        {
            ESP_LOGE(TAG, "xTaskCreate failed: http_server");
        }
    }
    else
    {
        ESP_LOGI(TAG, "Run http_server - the server is already running");
    }
}

void
http_server_stop(void)
{
    if (task_http_server)
    {
        vTaskDelete(task_http_server);
        task_http_server = NULL;
    }
}

static _Noreturn void
http_server_task(void *pvParameters)
{
    struct netconn *conn, *newconn;
    err_t           err;
    conn = netconn_new(NETCONN_TCP);
    netconn_bind(conn, IP_ADDR_ANY, 80);
    netconn_listen(conn);
    ESP_LOGI(TAG, "HTTP Server listening on 80/tcp");
    for (;;)
    {
        err = netconn_accept(conn, &newconn);
        if (err == ERR_OK)
        {
            http_server_netconn_serve(newconn);
            netconn_close(newconn);
            netconn_delete(newconn);
        }
        else if (err == ERR_TIMEOUT)
        {
            ESP_LOGE(TAG, "http_server: netconn_accept ERR_TIMEOUT");
        }
        else if (err == ERR_ABRT)
        {
            ESP_LOGE(TAG, "http_server: netconn_accept ERR_ABRT");
        }
        else
        {
            ESP_LOGE(TAG, "http_server: netconn_accept: %d", err);
        }
        taskYIELD(); /* allows the freeRTOS scheduler to take over if needed. */
    }
    netconn_close(conn);
    netconn_delete(conn);

    vTaskDelete(NULL);
}

char *
http_server_get_header(char *request, char *header_name, int *len)
{
    *len      = 0;
    char *ret = NULL;
    char *ptr = NULL;

    ptr = strstr(request, header_name);
    if (ptr)
    {
        ret = ptr + strlen(header_name);
        ptr = ret;
        while (*ptr != '\0' && *ptr != '\n' && *ptr != '\r')
        {
            (*len)++;
            ptr++;
        }
        return ret;
    }
    return NULL;
}

char *
get_http_body(char *msg, int len, int *blen)
{
    char *newlines = "\r\n\r\n";
    char *p        = strstr(msg, newlines);
    if (p)
    {
        p += strlen(newlines);
    }
    else
    {
        ESP_LOGD(TAG, "http body not found: %s", msg);
        return 0;
    }

    if (blen)
    {
        *blen = len - (p - msg);
    }

    return p;
}

static http_server_resp_t
http_server_handle_req_get(const char *p_file_name)
{
    ESP_LOGI(TAG, "GET /%s", p_file_name);

    char *file_ext = strrchr(p_file_name, '.');
    if ((NULL != file_ext) && (0 == strcmp(file_ext, ".json")))
    {
        if (0 == strcmp(p_file_name, "ap.json"))
        {
            /* if we can get the mutex, write the last version of the AP list */
            if (!wifi_manager_lock_json_buffer((TickType_t)10))
            {
                ESP_LOGD(TAG, "http_server_netconn_serve: GET /ap.json failed to obtain mutex");
                ESP_LOGI(TAG, "ap.json: 503");
                return http_server_resp_503();
            }
            const char *buff = json_access_points_get();
            if (NULL == buff)
            {
                ESP_LOGI(TAG, "status.json: 503");
                return http_server_resp_503();
            }
            ESP_LOGI(TAG, "ap.json: %s", buff);
            const http_server_resp_t resp = http_server_resp_200_json(buff);
            wifi_manager_unlock_json_buffer();
            return resp;
        }
        else if (0 == strcmp(p_file_name, "status.json"))
        {
            if (!wifi_manager_lock_json_buffer((TickType_t)10))
            {
                ESP_LOGD(TAG, "http_server_netconn_serve: GET /status failed to obtain mutex");
                ESP_LOGI(TAG, "status.json: 503");
                return http_server_resp_503();
            }
            const char *buff = json_network_info_get();
            if (NULL == buff)
            {
                ESP_LOGI(TAG, "status.json: 503");
                return http_server_resp_503();
            }
            ESP_LOGI(TAG, "status.json: %s", buff);
            const http_server_resp_t resp = http_server_resp_200_json(buff);
            wifi_manager_unlock_json_buffer();
            return resp;
        }
    }
    return wifi_manager_cb_on_http_get(p_file_name);
}

static http_server_resp_t
http_server_handle_req_delete(const char *p_file_name)
{
    ESP_LOGI(TAG, "DELETE /%s", p_file_name);
    if (0 == strcmp(p_file_name, "connect.json"))
    {
        ESP_LOGD(TAG, "http_server_netconn_serve: DELETE /connect.json");
        /* request a disconnection from wifi and forget about it */
        wifi_manager_disconnect_async();
        return http_server_resp_200_json("{}");
    }
    return wifi_manager_cb_on_http_delete(p_file_name);
}

static http_server_resp_t
http_server_handle_req_post(const char *p_file_name, char *save_ptr)
{
    ESP_LOGI(TAG, "POST /%s", p_file_name);
    const char *body = strstr(save_ptr, "\r\n\r\n");
    if (0 == strcmp(p_file_name, "connect.json"))
    {
        ESP_LOGD(TAG, "http_server_netconn_serve: POST /connect.json");
        int   lenS = 0, lenP = 0;
        char *ssid     = http_server_get_header(save_ptr, "X-Custom-ssid: ", &lenS);
        char *password = http_server_get_header(save_ptr, "X-Custom-pwd: ", &lenP);
        if ((NULL != ssid) && (lenS <= MAX_SSID_SIZE) && (NULL != password) && (lenP <= MAX_PASSWORD_SIZE))
        {
            wifi_config_t *config = wifi_manager_get_wifi_sta_config();
            memset(config, 0x00, sizeof(wifi_config_t));
            memcpy(config->sta.ssid, ssid, lenS);
            memcpy(config->sta.password, password, lenP);
            ESP_LOGD(TAG, "http_server_netconn_serve: wifi_manager_connect_async() call");
            wifi_manager_connect_async();
            return http_server_resp_200_json("{}");
        }
        else
        {
            /* bad request the authentification header is not complete/not the correct format */
            return http_server_resp_400();
        }
    }
    return wifi_manager_cb_on_http_post(p_file_name, body);
}

static http_server_resp_t
http_server_handle_req(char *line, char *save_ptr)
{
    char *p = strchr(line, ' ');
    if (NULL == p)
    {
        return http_server_resp_400();
    }
    const char * http_cmd     = line;
    const size_t http_cmd_len = p - line;
    char *       path         = p + 1;
    if ('/' == path[0])
    {
        path += 1;
    }
    p = strchr(path, ' ');
    if (NULL == p)
    {
        return http_server_resp_400();
    }
    *p = '\0';

    if (0 == strncmp("GET", http_cmd, http_cmd_len))
    {
        return http_server_handle_req_get(path);
    }
    else if (0 == strncmp("DELETE", http_cmd, http_cmd_len))
    {
        return http_server_handle_req_delete(path);
    }
    else if (0 == strncmp("POST", http_cmd, http_cmd_len))
    {
        return http_server_handle_req_post(path, save_ptr);
    }
    else
    {
        return http_server_resp_400();
    }
}

void
http_server_netconn_serve(struct netconn *conn)
{
    struct netbuf *inbuf;
    char *         buf = NULL;
    char           fullbuf[FULLBUF_SIZE + 1];
    uint           fullsize = 0;
    u16_t          buflen;
    err_t          err;
    const char     new_line[2]   = "\n";
    bool           request_ready = false;

    while (request_ready == false)
    {
        err = netconn_recv(conn, &inbuf);
        if (err == ERR_OK)
        {
            netbuf_data(inbuf, (void **)&buf, &buflen);

            if (fullsize + buflen > FULLBUF_SIZE)
            {
                ESP_LOGW(TAG, "fullbuf full, fullsize: %d, buflen: %d", fullsize, buflen);
                netbuf_delete(inbuf);
                break;
            }
            else
            {
                memcpy(fullbuf + fullsize, buf, buflen);
                fullsize += buflen;
                fullbuf[fullsize] = 0; // zero terminated string
            }

            netbuf_delete(inbuf);

            // check if there should be more data coming from conn
            int   hLen = 0;
            char *cl   = http_server_get_header(fullbuf, "Content-Length: ", &hLen);
            if (cl)
            {
                int   body_len;
                int   clen = atoi(cl);
                char *b    = get_http_body(fullbuf, fullsize, &body_len);
                if (b)
                {
                    ESP_LOGD(TAG, "Header Content-Length: %d, HTTP body length: %d", clen, body_len);
                    if (clen == body_len)
                    {
                        // HTTP request is full
                        request_ready = true;
                    }
                    else
                    {
                        ESP_LOGD(TAG, "request not full yet");
                    }
                }
                else
                {
                    ESP_LOGD(TAG, "Header Content-Length: %d, body not found", clen);
                    // read more data
                }
            }
            else
            {
                // ESP_LOGD(TAG, "no Content-Length header");
                request_ready = true;
            }
        }
        else
        {
            ESP_LOGW(TAG, "netconn recv: %d", err);
            break;
        }
    }

    if (!request_ready)
    {
        ESP_LOGW(TAG, "the connection was closed by the client side");
        return;
    }

    buf            = fullbuf;
    char *save_ptr = fullbuf;
    buflen         = fullsize;

    ESP_LOGD(TAG, "req: %s", buf);

    char *line = strtok_r(save_ptr, new_line, &save_ptr);

    if (line)
    {
        /* captive portal functionality: redirect to access point IP for HOST that are not the access point IP OR the
         * STA IP */
        int   lenH = 0;
        char *host = http_server_get_header(save_ptr, "Host: ", &lenH);
        /* determine if Host is from the STA IP address */

        const sta_ip_string_t ip_str = sta_ip_safe_get(portMAX_DELAY);
        bool access_from_sta_ip      = ('\0' == ip_str.buf[0]) || ((lenH > 0) && (NULL != strstr(host, ip_str.buf)));

        ESP_LOGD(TAG, "Host: %.*s", lenH, host);
        ESP_LOGD(TAG, "StaticIP: %s", ip_str.buf);
        if ((lenH > 0) && (NULL == strstr(host, DEFAULT_AP_IP)) && (!access_from_sta_ip))
        {
            http_server_netconn_resp_302(conn);
        }
        else
        {
            const http_server_resp_t resp = http_server_handle_req(line, save_ptr);
            switch (resp.http_resp_code)
            {
                case HTTP_RESP_CODE_200:
                    http_server_netconn_resp_200(conn, &resp);
                    break;
                case HTTP_RESP_CODE_400:
                    http_server_netconn_resp_400(conn);
                    break;
                case HTTP_RESP_CODE_404:
                    http_server_netconn_resp_404(conn);
                    break;
                case HTTP_RESP_CODE_503:
                    http_server_netconn_resp_503(conn);
                    break;
                default:
                    http_server_netconn_resp_503(conn);
                    break;
            }
        }
    }
    else
    {
        http_server_netconn_resp_404(conn);
    }
}
