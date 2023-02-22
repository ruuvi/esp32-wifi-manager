/**
 * @file wifiman_config.h
 * @author TheSomeMan
 * @date 2022-04-13
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef WIFIMAN_CONFIG_H
#define WIFIMAN_CONFIG_H

#include "wifi_manager_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

const wifiman_config_t*
wifiman_default_config_init(const wifiman_wifi_ssid_t* const p_wifi_ssid, const wifiman_hostname_t* const p_hostname);

const wifiman_config_t*
wifiman_default_config_get(void);

void
wifiman_default_config_set(const wifiman_config_t* const p_wifi_cfg);

void
wifiman_config_init(const wifiman_config_t* const p_wifi_cfg);

void
wifiman_config_ap_set(const wifiman_config_ap_t* const p_wifi_cfg_ap);

void
wifiman_config_sta_set(const wifiman_config_sta_t* const p_wifi_cfg_sta);

void
wifiman_config_sta_save(void);

wifi_sta_config_t
wifiman_config_sta_get_config(void);

wifi_settings_sta_t
wifiman_config_sta_get_settings(void);

wifiman_wifi_ssid_t
wifiman_config_sta_get_ssid(void);

bool
wifiman_config_sta_is_ssid_configured(void);

void
wifiman_config_sta_set_ssid_and_password(
    const wifiman_wifi_ssid_t* const     p_ssid,
    const wifiman_wifi_password_t* const p_password);

wifiman_hostname_t
wifiman_config_sta_get_hostname(void);

wifi_ap_config_t
wifiman_config_ap_get_config(void);

wifi_settings_ap_t
wifiman_config_ap_get_settings(void);

wifiman_ip4_addr_str_t
wifiman_config_ap_get_ip_str(void);

esp_ip4_addr_t
wifiman_config_ap_get_ip(void);

wifiman_wifi_ssid_t
wifiman_config_ap_get_ssid(void);

#ifdef __cplusplus
}
#endif

#endif // WIFIMAN_CONFIG_H
