/**
 * @file wifi_manager_internal.h
 * @author TheSomeMan
 * @date 2020-10-28
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_WIFI_MANAGER_INTERNAL_H
#define RUUVI_WIFI_MANAGER_INTERNAL_H

#include "wifi_manager_defs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "os_wrapper_types.h"
#include "os_sema.h"
#include "os_timer.h"
#include "http_req.h"
#include "esp_wps.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_MANAGER_RECONNECT_STA_DEFAULT_TIMEOUT_SEC            (1U)
#define WIFI_MANAGER_RECONNECT_STA_AFTER_MIC_FAILURE1_TIMEOUT_SEC (1U * TIME_UNITS_SECONDS_PER_MINUTE)
#define WIFI_MANAGER_RECONNECT_STA_AFTER_MIC_FAILURE2_TIMEOUT_SEC (5U * TIME_UNITS_SECONDS_PER_MINUTE)
#define WIFI_MANAGER_RECONNECT_STA_AFTER_MIC_FAILURE3_TIMEOUT_SEC (10U * TIME_UNITS_SECONDS_PER_MINUTE)
#define WIFI_MANAGER_RECONNECT_STA_AFTER_MIC_FAILURE4_TIMEOUT_SEC (20U * TIME_UNITS_SECONDS_PER_MINUTE)
#define WIFI_MANAGER_RECONNECT_STA_AFTER_MIC_FAILURE5_TIMEOUT_SEC (30U * TIME_UNITS_SECONDS_PER_MINUTE)

/* @brief indicates that wifi_manager is working. */
#define WIFI_MANAGER_IS_WORKING ((uint32_t)(BIT0))

/* @brief indicates that the ESP32 is currently connected. */
#define WIFI_MANAGER_WIFI_CONNECTED_BIT ((uint32_t)(BIT1))

#define WIFI_MANAGER_AP_STA_CONNECTED_BIT   ((uint32_t)(BIT2))
#define WIFI_MANAGER_AP_STA_IP_ASSIGNED_BIT ((uint32_t)(BIT3))

/* @brief Set automatically once the SoftAP is started */
#define WIFI_MANAGER_AP_STARTED_BIT ((uint32_t)(BIT4))

/* @brief When set, means a client requested to connect to an access point.*/
#define WIFI_MANAGER_REQUEST_STA_CONNECT_BIT ((uint32_t)(BIT5))

/* @brief When set, means the wifi manager attempts to restore a previously saved connection at startup. */
#define WIFI_MANAGER_REQUEST_RESTORE_STA_BIT ((uint32_t)(BIT6))

/* @brief When set, means a scan is in progress */
#define WIFI_MANAGER_SCAN_BIT ((uint32_t)(BIT7))

/* @brief When set, means user requested for a disconnect */
#define WIFI_MANAGER_REQUEST_DISCONNECT_BIT ((uint32_t)(BIT8))

/* @brief indicate that the device is currently connected to Ethernet. */
#define WIFI_MANAGER_ETH_CONNECTED_BIT ((uint32_t)(BIT9))

/* @brief indicate that WiFi access point is active. */
#define WIFI_MANAGER_AP_ACTIVE ((uint32_t)(BIT10))

/* @brief Block requests from LAN while WiFi access point is active. */
#define WIFI_MANAGER_BLOCK_REQ_FROM_LAN_WHILE_AP_ACTIVE ((uint32_t)(BIT11))

/* @brief indicates that the ESP32 is working as Wi-Fi station and trying to connect or connected to a hotspot. */
#define WIFI_MANAGER_STA_ACTIVE_BIT ((uint32_t)(BIT12))

/* @brief indicates that the ESP32 is trying to connect to a hotspot for the first time. */
#define WIFI_MANAGER_INITIAL_CONNECTION_BIT ((uint32_t)(BIT13))

/* @brief indicates that command to connect to STA was sent, but has not handled yet. */
#define WIFI_MANAGER_CMD_STA_CONNECT_BIT ((uint32_t)(BIT14))

#define WIFI_MANAGER_DELAY_BETWEEN_SCANNING_WIFI_CHANNELS_MS (200U)

#define WIFI_MANAGER_WIFI_COUNTRY_DEFAULT_FIRST_CHANNEL (1U)
#define WIFI_MANAGER_WIFI_COUNTRY_DEFAULT_NUM_CHANNELS  (13U)

#define WIFI_MANAGER_TASK_WATCHDOG_FEEDING_PERIOD_TICKS (pdMS_TO_TICKS(1000))

typedef struct wifi_manager_antenna_config_t wifi_manager_antenna_config_t;

typedef struct wifi_manager_scan_info_t
{
    uint8_t  first_chan;
    uint8_t  last_chan;
    uint8_t  cur_chan;
    uint16_t num_access_points;
} wifi_manager_scan_info_t;

extern EventGroupHandle_t g_p_wifi_manager_event_group;

extern esp_wps_config_t g_wps_config;
extern bool             g_wifi_wps_enabled;

bool
wifi_manager_is_initialized(void);

void
wifi_manager_init_mutex(void);

/**
 * @brief Tries to lock access to wifi_manager internal structures using mutex
 * (it also locks the access points and connection status json buffers).
 *
 * The HTTP server can try to access the json to serve clients while the wifi manager thread can try
 * to update it.
 * Also, wifi_manager can be asynchronously stopped and HTTP server or other threads
 * can try to call wifi_manager while it is de-initializing.
 *
 * @param ticks_to_wait The time in ticks to wait for the mutex to become available.
 * @return true in success, false otherwise.
 */
bool
wifi_manager_lock_with_timeout(const os_delta_ticks_t ticks_to_wait);

/**
 * @brief Lock the wifi_manager mutex.
 */
void
wifi_manager_lock(void);

/**
 * @brief Releases the wifi_manager mutex.
 */
void
wifi_manager_unlock(void);

void
wifi_manager_task(void);

void
wifi_manager_event_handler(
    ATTR_UNUSED void*      p_ctx,
    const esp_event_base_t p_event_base,
    const int32_t          event_id,
    void*                  p_event_data);

void
wifi_manager_cb_on_user_req(const http_server_user_req_code_e req_code);

http_server_resp_t
wifi_manager_cb_on_http_get(
    const char* const               p_path,
    const char* const               p_uri_params,
    const bool                      flag_access_from_lan,
    const http_server_resp_t* const p_resp_auth);

http_server_resp_t
wifi_manager_cb_on_http_post(
    const char* const     p_path,
    const char* const     p_uri_params,
    const http_req_body_t http_body,
    const bool            flag_access_from_lan);

http_server_resp_t
wifi_manager_cb_on_http_delete(
    const char* const               p_path,
    const char* const               p_uri_params,
    const bool                      flag_access_from_lan,
    const http_server_resp_t* const p_resp_auth);

bool
wifi_manager_recv_and_handle_msg(void);

const char*
wifi_manager_generate_access_points_json(void);

bool
wifi_manager_init(
    const bool                                 flag_connect_sta,
    const wifiman_config_t* const              p_wifi_cfg,
    const wifi_manager_antenna_config_t* const p_wifi_ant_config,
    const wifi_manager_callbacks_t* const      p_callbacks);

void
wifi_manager_esp_wifi_configure_ap(void);

void
wifi_manager_netif_configure_sta(void);

void
wifi_manager_scan_timer_start(void);

void
wifi_manager_scan_timer_stop(void);

void
wifi_callback_on_connect_eth_cmd(void);

void
wifi_callback_on_wps_started(void);

void
wifi_callback_on_wps_stopped(void);

void
wifi_callback_on_ap_started(void);

void
wifi_callback_on_ap_stopped(void);

void
wifi_callback_on_ap_sta_connected(void);

void
wifi_callback_on_ap_sta_disconnected(void);

void
wifi_callback_on_ap_sta_ip_assigned(void);

void
wifi_callback_on_disconnect_eth_cmd(void);

void
wifi_callback_on_disconnect_sta_cmd(void);

void
wifi_manager_cb_save_wifi_config_sta(const wifiman_config_sta_t* const p_cfg_sta);

void
wifi_manager_cb_on_request_status_json(void);

void
wifi_manger_notify_scan_done(void);

void
wifi_manager_start_timer_reconnect_sta_after_timeout(const os_delta_ticks_t delay_ticks);

void
wifi_manager_stop_timer_reconnect_sta_after_timeout(void);

bool
wifi_man_set_wdog_feed_flag(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_WIFI_MANAGER_INTERNAL_H
