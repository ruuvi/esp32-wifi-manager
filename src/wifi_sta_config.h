/**
 * @file wifi_sta_config.h
 * @author TheSomeMan
 * @date 2020-11-15
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef WIFI_STA_CONFIG_H
#define WIFI_STA_CONFIG_H

#include <stdbool.h>
#include "esp_wifi_types.h"
#include "wifi_manager_defs.h"
#include "esp_netif.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Clears the current STA wifi config in RAM storage.
 * @param p_gw_wifi_ssid - ptr to wifi_ssid_t with AP SSID.
 */
void
wifi_sta_config_init(const wifi_ssid_t *const p_gw_wifi_ssid, const wifi_sta_config_t *const p_wifi_sta_default_cfg);

/**
 * @brief Clears the current STA wifi config in NVS and RAM storage.
 */
bool
wifi_sta_config_clear(void);

/**
 * @brief Saves the current STA wifi config to NVS from RAM storage.
 */
bool
wifi_config_save(void);

/**
 * @brief Check if the STA wifi config in NVS is valid.
 * @return true if it is valid.
 */
bool
wifi_config_check(void);

/**
 * @brief Fetch a previously STA wifi config from NVS to the RAM storage.
 * @return true if a previously saved config was found, false otherwise.
 */
bool
wifi_config_fetch(void);

/**
 * @brief Get the copy of the current STA wifi config in RAM storage.
 */
wifi_sta_config_t
wifi_sta_config_get_copy(void);

wifi_settings_t
wifi_config_get_wifi_settings(void);

/**
 * @brief Check if SSID is configured.
 * @return true if SSID is configured.
 */
bool
wifi_config_sta_is_ssid_configured(void);

/**
 * @brief Copy SSID from the current STA wifi config in RAM.
 */
wifi_ssid_t
wifi_config_sta_get_ssid(void);

void
wifi_config_sta_set_ssid_and_password(
    const char  *constp_ssid,
    const size_t ssid_len,
    const char *constp_password,
    const size_t password_len);

wifi_ssid_t
wifi_settings_ap_get_ssid(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_STA_CONFIG_H
