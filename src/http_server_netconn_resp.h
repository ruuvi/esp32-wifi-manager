/**
 * @file http_server_netconn_resp.h
 * @author TheSomeMan
 * @date 2026-05-07
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_HTTP_SERVER_NETCONN_RESP_H
#define RUUVI_GATEWAY_ESP_HTTP_SERVER_NETCONN_RESP_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "lwip/api.h"
#include "lwip/err.h"
#include "wifi_manager_defs.h"
#include "http_server_resp.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP_HEADER_DATE_EXAMPLE "Date: Thu, 01 Jan 2021 00:00:00 GMT\r\n"

typedef struct http_header_date_str_t
{
    char buf[sizeof(HTTP_HEADER_DATE_EXAMPLE)];
} http_header_date_str_t;

extern http_header_extra_fields_t g_http_server_extra_header_fields;

const char*
conv_lwip_err_to_str(const err_enum_t err);

bool
http_server_netconn_write(
    struct netconn* const p_conn,
    const void* const     p_buf,
    const size_t          buf_len,
    const uint8_t         netconn_flags);

void
http_server_netconn_resp_302(struct netconn* const p_conn);

void
http_server_netconn_resp_400(struct netconn* const p_conn, http_server_resp_t* const p_resp);

void
http_server_netconn_resp_401(
    struct netconn* const                   p_conn,
    http_server_resp_t* const               p_resp,
    const http_header_extra_fields_t* const p_extra_header_fields);

void
http_server_netconn_resp_403(
    struct netconn* const                   p_conn,
    http_server_resp_t* const               p_resp,
    const http_header_extra_fields_t* const p_extra_header_fields);

void
http_server_netconn_resp_404(struct netconn* const p_conn, http_server_resp_t* const p_resp);

void
http_server_netconn_resp_409(struct netconn* const p_conn, http_server_resp_t* const p_resp);

void
http_server_netconn_resp_429(struct netconn* const p_conn, http_server_resp_t* const p_resp);

void
http_server_netconn_resp_500(struct netconn* const p_conn, http_server_resp_t* const p_resp);

void
http_server_netconn_resp_502(struct netconn* const p_conn, http_server_resp_t* const p_resp);

void
http_server_netconn_resp_503(struct netconn* const p_conn, http_server_resp_t* const p_resp);

void
http_server_netconn_resp_504(struct netconn* const p_conn, http_server_resp_t* const p_resp);

void
http_server_netconn_resp(struct netconn* const p_conn, http_server_resp_t* const p_resp, const char* const p_hostname);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_HTTP_SERVER_NETCONN_RESP_H
