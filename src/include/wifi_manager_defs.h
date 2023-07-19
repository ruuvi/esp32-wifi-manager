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

@file wifi_manager_defs.h
@author Tony Pottier
@brief Defines all functions necessary for esp32 to connect to a wifi/scan wifis

Contains the freeRTOS task and all necessary support

@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#ifndef ESP32_WIFI_MANAGER_TYPES_H
#define ESP32_WIFI_MANAGER_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_wifi_types.h"
#include "esp_type_wrapper.h"
#include "esp_netif_ip_addr.h"
#include "esp_netif_types.h"
#include "http_server.h"
#include "json_stream_gen.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Defines the maximum size of a SSID name. 32 is IEEE standard.
 * @warning limit is also hard coded in wifi_config_t. Never extend this value.
 */
#define MAX_SSID_SIZE 32

typedef struct wifiman_wifi_ssid_t
{
    char ssid_buf[MAX_SSID_SIZE];
} wifiman_wifi_ssid_t;

/**
 * @brief Defines the maximum size of a hostname according to DHCP Option 12.
 * @details The name starts with a letter, ends with a letter or a digit, and can have letters, hyphens,
 *          or digits in between the first and last characters.
 *          The maximum size supported for a hostname is 30 characters.
 *          The minimum number of characters supported for a hostname is one character.
 */
#define MAX_HOSTNAME_SIZE 30

typedef struct wifiman_hostname_t
{
    char buf[MAX_HOSTNAME_SIZE];
} wifiman_hostname_t;

#define WIFIMAN_FW_VERSION_SIZE (33)

typedef struct wifiman_fw_version_t
{
    char buf[WIFIMAN_FW_VERSION_SIZE];
} wifiman_fw_version_t;

typedef struct wifiman_hostinfo_t
{
    wifiman_hostname_t   hostname;
    wifiman_fw_version_t fw_ver;
    wifiman_fw_version_t nrf52_fw_ver;
} wifiman_hostinfo_t;

/**
 * @brief Defines the maximum size of a WPA2 passkey. 64 is IEEE standard.
 * @warning limit is also hard coded in wifi_config_t. Never extend this value.
 */
#define MAX_PASSWORD_SIZE 64

typedef struct wifiman_wifi_password_t
{
    char password_buf[MAX_PASSWORD_SIZE];
} wifiman_wifi_password_t;

#define WIFI_IP4_ADDR_STR_SIZE (3 + 1 + 3 + 1 + 3 + 1 + 3 + 1)

typedef struct wifi_ip4_addr_str_t
{
    char buf[WIFI_IP4_ADDR_STR_SIZE];
} wifiman_ip4_addr_str_t;

/**
 * @brief Defines the maximum number of access points that can be scanned.
 *
 * To save memory and avoid nasty out of memory errors,
 * we can limit the number of APs detected in a wifi scan.
 */
#define MAX_AP_NUM 30

/** @brief Defines the task priority of the wifi_manager.
 *
 * Tasks spawn by the manager will have a priority of WIFI_MANAGER_TASK_PRIORITY-1.
 * For this particular reason, minimum task priority is 1. It it highly not recommended to set
 * it to 1 though as the sub-tasks will now have a priority of 0 which is the priority
 * of freeRTOS' idle task.
 */
#define WIFI_MANAGER_TASK_PRIORITY CONFIG_WIFI_MANAGER_TASK_PRIORITY

/** @brief Defines access point's name prefix.
 * Default value: GuuviGateway.
 * Run 'idf.py menuconfig' to setup your own value or replace here by a string */
#define DEFAULT_AP_SSID CONFIG_DEFAULT_AP_SSID

/**
 * @brief Defines the maximum length in bytes of a JSON representation of an access point.
 *
 *  maximum ap string length with full 32 char ssid: 74 = 74
 *  example: {"ssid":"abcdefghijklmnopqrstuvwxyz012345","chan":12,"rssi":-100,"auth":4}
 *  BUT: we need to escape JSON. Imagine a ssid full of \" ? so it's 32 more bytes hence 74 + 32 = 106.\n
 *  this is an edge case but I don't think we should crash in a catastrophic manner just because
 *  someone decided to have a funny wifi name.
 */
#define JSON_ACCESS_POINT_ONE_RECORD_SIZE (106)

#define JSON_ACCESS_POINT_BUF_SIZE \
    (JSON_ACCESS_POINT_ONE_RECORD_SIZE * MAX_AP_NUM + 2 * (MAX_AP_NUM - 1) \
     + 5) /* 5 bytes for json encapsulation of "[\n" and "]\n\0" */

#define JSON_NETWORK_EXTRA_INFO_SIZE (100U)

/**
 * @brief Defines the maximum length in bytes of a JSON representation of the IP information
 * assuming all ips are 4*3 digits, and all characters in the ssid require to be escaped.
 * example:
 * {"ssid":"abcdefghijklmnopqrstuvwxyz012345","ip":"192.168.1.119","netmask":"255.255.255.0",
 *   "gw":"192.168.1.1","urc":0,"extra":{"fw_updating":1,"percentage":50}}
 */
#define JSON_IP_INFO_SIZE (150 + JSON_NETWORK_EXTRA_INFO_SIZE)

/**
 * @brief Defines the complete list of all messages that the wifi_manager can process.
 *
 * Some of these message are events ("EVENT"), and some of them are action ("ORDER")
 * Each of these messages can trigger a callback function and each callback function is stored
 * in a function pointer array for convenience. Because of this behavior, it is extremely important
 * to maintain a strict sequence and the top level special element 'MESSAGE_CODE_COUNT'
 *
 * @see wifi_manager_set_callback
 */
typedef enum message_code_e
{
    WIFI_MAN_MSG_CODE_NONE    = 0,
    ORDER_STOP_AND_DESTROY    = 1,
    ORDER_START_WIFI_SCAN     = 2,
    ORDER_CONNECT_ETH         = 4,
    ORDER_CONNECT_STA         = 5,
    ORDER_DISCONNECT_ETH      = 6,
    ORDER_DISCONNECT_STA      = 7,
    ORDER_START_AP            = 8,
    ORDER_STOP_AP             = 9,
    EVENT_STA_DISCONNECTED    = 10,
    EVENT_SCAN_NEXT           = 11,
    EVENT_SCAN_DONE           = 12,
    EVENT_STA_GOT_IP          = 13,
    EVENT_AP_STA_CONNECTED    = 14,
    EVENT_AP_STA_DISCONNECTED = 15,
    EVENT_AP_STA_IP_ASSIGNED  = 16,
    ORDER_TASK_WATCHDOG_FEED  = 17,
    MESSAGE_CODE_COUNT        = 18 /* important for the callback array */
} message_code_e;

typedef void (*wifi_manager_cb_ptr)(void*);

typedef int (*wifi_manager_ecdh_f_rng)(void*, unsigned char*, size_t);

/**
 * @brief simplified reason codes for a lost connection.
 *
 * esp-idf maintains a big list of reason codes which in practice are useless for most typical application.
 */
typedef enum update_reason_code_e
{
    UPDATE_CONNECTION_UNDEF = -1,
    UPDATE_CONNECTION_OK    = 0,
    UPDATE_FAILED_ATTEMPT   = 1,
    UPDATE_USER_DISCONNECT  = 2,
    UPDATE_LOST_CONNECTION  = 3
} update_reason_code_e;

typedef enum connection_request_made_by_code_e
{
    CONNECTION_REQUEST_NONE               = 0,
    CONNECTION_REQUEST_USER               = 1,
    CONNECTION_REQUEST_AUTO_RECONNECT     = 2,
    CONNECTION_REQUEST_RESTORE_CONNECTION = 3,
    CONNECTION_REQUEST_MAX                = 0x7fffffff /*force the creation of this enum as a 32 bit int */
} connection_request_made_by_code_e;

typedef union wifiman_msg_param_t
{
    void*     ptr;
    uintptr_t val;
} wifiman_msg_param_t;

/**
 * @brief Structure used to store one message in the queue.
 */
typedef struct
{
    message_code_e      code;
    wifiman_msg_param_t msg_param;
} queue_message;

#define NETWORK_INFO_STRLEN_MAX 16

typedef struct network_dhcp_str_t
{
    char buf[NETWORK_INFO_STRLEN_MAX];
} network_dhcp_str_t;

typedef struct network_info_str_t
{

#if defined(IP4ADDR_STRLEN_MAX)
#if NETWORK_INFO_STRLEN_MAX < IP4ADDR_STRLEN_MAX
#error NETWORK_INFO_STRLEN_MAX < IP4ADDR_STRLEN_MAX
#endif
#endif
    char               ip[NETWORK_INFO_STRLEN_MAX];
    char               gw[NETWORK_INFO_STRLEN_MAX];
    char               netmask[NETWORK_INFO_STRLEN_MAX];
    network_dhcp_str_t dhcp;
} network_info_str_t;

typedef enum http_resp_code_e
{
    HTTP_RESP_CODE_200 = 200, // OK
    HTTP_RESP_CODE_301 = 301, // Moved Permanently
    HTTP_RESP_CODE_302 = 302, // Found
    HTTP_RESP_CODE_400 = 400, // Bad Request
    HTTP_RESP_CODE_401 = 401, // Unauthorized
    HTTP_RESP_CODE_403 = 403, // Forbidden
    HTTP_RESP_CODE_404 = 404, // Not Found
    HTTP_RESP_CODE_429 = 429, // Too Many Requests
    HTTP_RESP_CODE_500 = 500, // Internal Server Error
    HTTP_RESP_CODE_502 = 502, // Bad Gateway
    HTTP_RESP_CODE_503 = 503, // Service Unavailable
    HTTP_RESP_CODE_504 = 504, // Gateway Timeout
} http_resp_code_e;

typedef enum http_content_type_e
{
    HTTP_CONENT_TYPE_TEXT_HTML,
    HTTP_CONENT_TYPE_TEXT_PLAIN,
    HTTP_CONENT_TYPE_TEXT_CSS,
    HTTP_CONENT_TYPE_TEXT_JAVASCRIPT,
    HTTP_CONENT_TYPE_IMAGE_PNG,
    HTTP_CONENT_TYPE_IMAGE_SVG_XML,
    HTTP_CONENT_TYPE_APPLICATION_JSON,
    HTTP_CONENT_TYPE_APPLICATION_OCTET_STREAM,
} http_content_type_e;

typedef enum http_content_encoding_e
{
    HTTP_CONENT_ENCODING_NONE,
    HTTP_CONENT_ENCODING_GZIP,
} http_content_encoding_e;

typedef enum http_content_location_e
{
    HTTP_CONTENT_LOCATION_NO_CONTENT,
    HTTP_CONTENT_LOCATION_FLASH_MEM,
    HTTP_CONTENT_LOCATION_STATIC_MEM,
    HTTP_CONTENT_LOCATION_HEAP,
    HTTP_CONTENT_LOCATION_FATFS,
    HTTP_CONTENT_LOCATION_JSON_GENERATOR,
} http_content_location_e;

typedef struct http_server_resp_t
{
    http_resp_code_e        http_resp_code;
    http_content_location_e content_location;
    bool                    flag_no_cache;
    bool                    flag_add_header_date;
    http_content_type_e     content_type;
    const char*             p_content_type_param;
    size_t                  content_len;
    http_content_encoding_e content_encoding;
    union
    {
        struct
        {
            const uint8_t* p_buf;
        } memory;
        struct
        {
            socket_t fd;
        } fatfs;
        struct
        {
            json_stream_gen_t* p_json_gen;
        } json_generator;
    } select_location;
} http_server_resp_t;

typedef void (*wifi_manager_http_cb_on_user_req_t)(const http_server_user_req_code_e req_code);

typedef http_server_resp_t (*wifi_manager_http_callback_t)(
    const char* const               p_path,
    const char* const               p_uri_params,
    const bool                      flag_access_from_lan,
    const http_server_resp_t* const p_resp_auth);

typedef http_server_resp_t (*wifi_manager_http_cb_on_post_t)(
    const char* const p_path,
    const char* const p_uri_params,
    const char* const p_body,
    const bool        flag_access_from_lan);

typedef struct wifiman_config_ap_t  wifiman_config_ap_t;
typedef struct wifiman_config_sta_t wifiman_config_sta_t;
typedef struct wifiman_config_t     wifiman_config_t;

typedef void (*wifi_manager_callback_on_cmd_connect_eth_t)(void);
typedef void (*wifi_manager_callback_on_cmd_disconnect_eth_t)(void);
typedef void (*wifi_manager_callback_on_cmd_disconnect_sta_t)(void);
typedef void (*wifi_manager_callback_on_ap_started_t)(void);
typedef void (*wifi_manager_callback_on_ap_stopped_t)(void);
typedef void (*wifi_manager_callback_on_ap_sta_connected_t)(void);
typedef void (*wifi_manager_callback_on_ap_sta_ip_assigned_t)(void);
typedef void (*wifi_manager_callback_on_ap_sta_disconnected_t)(void);
typedef void (*wifi_manager_callback_save_wifi_config_sta_t)(const wifiman_config_sta_t* const p_wifi_cfg_sta);
typedef void (*wifi_manager_callback_on_request_status_json_t)(void);

typedef struct wifi_manager_callbacks_t
{
    wifi_manager_http_cb_on_user_req_t             cb_on_http_user_req;
    wifi_manager_http_callback_t                   cb_on_http_get;
    wifi_manager_http_cb_on_post_t                 cb_on_http_post;
    wifi_manager_http_callback_t                   cb_on_http_delete;
    wifi_manager_callback_on_cmd_connect_eth_t     cb_on_connect_eth_cmd;
    wifi_manager_callback_on_cmd_disconnect_eth_t  cb_on_disconnect_eth_cmd;
    wifi_manager_callback_on_cmd_disconnect_sta_t  cb_on_disconnect_sta_cmd;
    wifi_manager_callback_on_ap_started_t          cb_on_ap_started;
    wifi_manager_callback_on_ap_stopped_t          cb_on_ap_stopped;
    wifi_manager_callback_on_ap_sta_connected_t    cb_on_ap_sta_connected;
    wifi_manager_callback_on_ap_sta_ip_assigned_t  cb_on_ap_sta_ip_assigned;
    wifi_manager_callback_on_ap_sta_disconnected_t cb_on_ap_sta_disconnected;
    wifi_manager_callback_save_wifi_config_sta_t   cb_save_wifi_config_sta;
    wifi_manager_callback_on_request_status_json_t cb_on_request_status_json;
} wifi_manager_callbacks_t;

typedef struct wifi_settings_ap_t
{
    wifi_bandwidth_t       ap_bandwidth;
    wifiman_ip4_addr_str_t ap_ip;
    wifiman_ip4_addr_str_t ap_gw;
    wifiman_ip4_addr_str_t ap_netmask;
} wifi_settings_ap_t;

typedef struct wifi_settings_sta_t
{
    wifi_ps_type_t      sta_power_save;
    bool                sta_static_ip;
    esp_netif_ip_info_t sta_static_ip_config;
} wifi_settings_sta_t;

struct wifiman_config_ap_t
{
    wifi_ap_config_t   wifi_config_ap;
    wifi_settings_ap_t wifi_settings_ap;
};

struct wifiman_config_sta_t
{
    wifi_sta_config_t   wifi_config_sta;
    wifi_settings_sta_t wifi_settings_sta;
    wifiman_hostinfo_t  hostinfo;
};

struct wifiman_config_t
{
    wifiman_config_ap_t  ap;
    wifiman_config_sta_t sta;
};

#ifdef __cplusplus
}
#endif

#endif // ESP32_WIFI_MANAGER_TYPES_H
