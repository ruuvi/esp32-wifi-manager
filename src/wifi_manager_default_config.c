/**
 * @file wifi_manager_default_config.c
 * @author TheSomeMan
 * @date 2022-04-17
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "wifi_manager.h"
#include "wifiman_config.h"

const wifiman_config_t*
wifi_manager_default_config_init(
    const wifiman_wifi_ssid_t* const p_wifi_ssid,
    const wifiman_hostinfo_t* const  p_hostinfo)
{
    return wifiman_default_config_init(p_wifi_ssid, p_hostinfo);
}

void
wifi_manager_set_default_config(const wifiman_config_t* const p_wifi_cfg)
{
    wifiman_default_config_set(p_wifi_cfg);
}
