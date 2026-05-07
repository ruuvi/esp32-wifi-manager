/**
 * @file http_server_netconn_serve_handle_req.h
 * @author TheSomeMan
 * @date 2026-05-07
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_ESP32_WIFI_MANAGER_HTTP_SERVER_NETCONN_SERVE_HANDLE_REQ_H
#define RUUVI_ESP32_WIFI_MANAGER_HTTP_SERVER_NETCONN_SERVE_HANDLE_REQ_H

#include "lwip/api.h"
#include "sta_ip.h"

#ifdef __cplusplus
extern "C" {
#endif

void
http_server_netconn_serve_handle_req(
    struct netconn* const        p_conn,
    char* const                  p_req_buf,
    const sta_ip_string_t* const p_local_ip_str,
    const sta_ip_string_t* const p_remote_ip_str);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_ESP32_WIFI_MANAGER_HTTP_SERVER_NETCONN_SERVE_HANDLE_REQ_H
