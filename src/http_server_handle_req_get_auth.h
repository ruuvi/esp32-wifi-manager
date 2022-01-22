/**
 * @file http_server_handle_req_get_auth.h
 * @author TheSomeMan
 * @date 2021-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef ESP32_WIFI_MANAGER_HTTP_SERVER_HANDLE_REQ_GET_AUTH_H
#define ESP32_WIFI_MANAGER_HTTP_SERVER_HANDLE_REQ_GET_AUTH_H

#include "http_server_resp.h"
#include "http_req.h"
#include "http_server_auth.h"
#include "sta_ip.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum http_server_auth_api_key_e
{
    HTTP_SERVER_AUTH_API_KEY_NOT_USED = 0,
    HTTP_SERVER_AUTH_API_KEY_ALLOWED,
    HTTP_SERVER_AUTH_API_KEY_PROHIBITED,
} http_server_auth_api_key_e;

http_server_resp_t
http_server_handle_req_check_auth(
    const bool                           flag_access_from_lan,
    const http_req_header_t              http_header,
    const sta_ip_string_t *const         p_remote_ip,
    const http_server_auth_info_t *const p_auth_info,
    const wifi_ssid_t *const             p_ap_ssid,
    http_header_extra_fields_t *const    p_extra_header_fields,
    http_server_auth_api_key_e *const    p_allow_access_by_api_key);

http_server_resp_t
http_server_handle_req_get_auth(
    const bool                           flag_access_from_lan,
    const http_req_header_t              http_header,
    const sta_ip_string_t *const         p_remote_ip,
    const http_server_auth_info_t *const p_auth_info,
    const wifi_ssid_t *const             p_ap_ssid,
    http_header_extra_fields_t *const    p_extra_header_fields);

#ifdef __cplusplus
}
#endif

#endif // ESP32_WIFI_MANAGER_HTTP_SERVER_HANDLE_REQ_GET_AUTH_H
