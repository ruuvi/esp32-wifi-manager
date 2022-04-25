/**
 * @file wifiman_cfg_blob_convert.c
 * @author TheSomeMan
 * @date 2022-04-24
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "wifiman_cfg_blob_convert.h"
#include "wifiman_config.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG) && !RUUVI_TESTS
#warning Debug log level prints out the passwords as a "plaintext".
#endif

static const char TAG[] = "wifi_manager";

void
wifiman_cfg_blob_convert(const wifiman_cfg_blob_t *const p_cfg_blob_src, wifiman_config_t *const p_cfg_dst)
{
    const wifiman_config_t *const p_cfg_default = wifiman_default_config_get();

    *p_cfg_dst = *p_cfg_default;

    if (p_cfg_blob_src->wifi_settings.ap_channel <= 14)
    {
        p_cfg_dst->wifi_config_ap.channel = p_cfg_blob_src->wifi_settings.ap_channel;
    }
    else
    {
        LOG_WARN("%s: Unknown ap_channel=%d, force set to 0", __func__, p_cfg_blob_src->wifi_settings.ap_channel);
        p_cfg_dst->wifi_config_ap.channel = 0;
    }
    p_cfg_dst->wifi_config_ap.ssid_hidden = (0 != p_cfg_blob_src->wifi_settings.ap_ssid_hidden) ? 1 : 0;

    switch (p_cfg_blob_src->wifi_settings.ap_bandwidth)
    {
        case WIFI_BW_HT20:
        case WIFI_BW_HT40:
            p_cfg_dst->wifi_settings_ap.ap_bandwidth = p_cfg_blob_src->wifi_settings.ap_bandwidth;
            break;
        default:
            LOG_WARN(
                "%s: Unknown ap_bandwidth=%d, force set to WIFI_BW_HT20",
                __func__,
                p_cfg_blob_src->wifi_settings.ap_bandwidth);
            p_cfg_dst->wifi_settings_ap.ap_bandwidth = p_cfg_default->wifi_settings_ap.ap_bandwidth;
            break;
    }

    (void)snprintf(
        (char *)p_cfg_dst->wifi_config_sta.ssid,
        sizeof(p_cfg_dst->wifi_config_sta.ssid),
        "%s",
        p_cfg_blob_src->sta_ssid.ssid_buf);
    (void)snprintf(
        (char *)p_cfg_dst->wifi_config_sta.password,
        sizeof(p_cfg_dst->wifi_config_sta.password),
        "%s",
        p_cfg_blob_src->sta_password.password_buf);

    switch (p_cfg_blob_src->wifi_settings.sta_power_save)
    {
        case WIFI_PS_NONE:
        case WIFI_PS_MIN_MODEM:
        case WIFI_PS_MAX_MODEM:
            p_cfg_dst->wifi_settings_sta.sta_power_save = p_cfg_blob_src->wifi_settings.sta_power_save;
            break;
        default:
            LOG_WARN(
                "%s: Unknown sta_power_save=%d, force set to WIFI_PS_NONE",
                __func__,
                p_cfg_blob_src->wifi_settings.sta_power_save);
            p_cfg_dst->wifi_settings_sta.sta_power_save = p_cfg_dst->wifi_settings_sta.sta_power_save;
            break;
    }
    p_cfg_dst->wifi_settings_sta.sta_static_ip        = !!p_cfg_blob_src->wifi_settings.sta_static_ip;
    p_cfg_dst->wifi_settings_sta.sta_static_ip_config = p_cfg_blob_src->wifi_settings.sta_static_ip_config;
}
