/**
 * @file wifiman_cfg_blob_convert.h
 * @author TheSomeMan
 * @date 2022-04-24
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_WIFIMAN_CFG_BLOB_CONVERT_H
#define RUUVI_GATEWAY_ESP_WIFIMAN_CFG_BLOB_CONVERT_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_wifi_types.h"
#include "wifi_manager_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIFIMAN_CFG_BLOB_MAX_SSID_SIZE     (32U)
#define WIFIMAN_CFG_BLOB_MAX_PASSWORD_SIZE (64U)

typedef struct wifiman_cfg_blob_wifi_ssid_t
{
    char ssid_buf[WIFIMAN_CFG_BLOB_MAX_SSID_SIZE];
} wifiman_cfg_blob_wifi_ssid_t;

typedef struct wifiman_cfg_blob_wifi_password_t
{
    char password_buf[WIFIMAN_CFG_BLOB_MAX_PASSWORD_SIZE];
} wifiman_cfg_blob_wifi_password_t;

typedef struct wifiman_cfg_blob_wifi_settings_t
{
    uint8_t             ap_ssid[WIFIMAN_CFG_BLOB_MAX_SSID_SIZE];
    uint8_t             ap_pwd[WIFIMAN_CFG_BLOB_MAX_PASSWORD_SIZE];
    uint8_t             ap_channel;
    uint8_t             ap_ssid_hidden;
    wifi_bandwidth_t    ap_bandwidth;
    bool                sta_only;
    wifi_ps_type_t      sta_power_save;
    bool                sta_static_ip;
    esp_netif_ip_info_t sta_static_ip_config;
} wifiman_cfg_blob_wifi_settings_t;

typedef struct wifiman_cfg_blob_t
{
    wifiman_cfg_blob_wifi_ssid_t     sta_ssid;
    wifiman_cfg_blob_wifi_password_t sta_password;
    wifiman_cfg_blob_wifi_settings_t wifi_settings;
} wifiman_cfg_blob_t;

void
wifiman_cfg_blob_convert(const wifiman_cfg_blob_t* const p_cfg_blob_src, wifiman_config_t* const p_cfg_dst);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_WIFIMAN_CFG_BLOB_CONVERT_H
