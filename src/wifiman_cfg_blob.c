/**
 * @file wifiman_cfg_blob.c
 * @author TheSomeMan
 * @date 2020-11-15
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "wifi_manager.h"
#include <stdbool.h>
#include <string.h>
#include "nvs.h"
#include "esp_wifi_types.h"
#include "wifi_manager_defs.h"
#include "wifiman_config.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#if LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG
#warning Debug log level prints out the passwords as a "plaintext".
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

static const char TAG[] = "wifi_manager";

static const char wifi_manager_nvs_namespace[] = "espwifimgr";

static bool
wifiman_config_nvs_open(const nvs_open_mode_t open_mode, nvs_handle_t *const p_handle)
{
    const char *nvs_name = wifi_manager_nvs_namespace;
    esp_err_t   err      = nvs_open(nvs_name, open_mode, p_handle);
    if (ESP_OK != err)
    {
        if (ESP_ERR_NVS_NOT_INITIALIZED == err)
        {
            LOG_WARN("NVS namespace '%s': StorageState is INVALID, need to erase NVS", nvs_name);
        }
        else if (ESP_ERR_NVS_NOT_FOUND == err)
        {
            LOG_DBG("NVS namespace '%s' doesn't exist", nvs_name);
        }
        else
        {
            LOG_ERR_ESP(err, "Can't open NVS namespace: '%s'", nvs_name);
        }
        return false;
    }
    return true;
}

static bool
wifiman_config_nvs_get_blob(const nvs_handle_t handle, const char *const key, void *const p_out_buf, size_t length)
{
    const esp_err_t esp_err = nvs_get_blob(handle, key, p_out_buf, &length);
    if (ESP_OK != esp_err)
    {
        LOG_DBG("nvs_get_blob failed for key '%s', err=%d", key, esp_err);
        return false;
    }
    return true;
}

static bool
wifiman_cfg_blob_read_by_handle(
    const nvs_handle                        handle,
    wifiman_cfg_blob_wifi_ssid_t *const     p_ssid,
    wifiman_cfg_blob_wifi_password_t *const p_password,
    wifiman_cfg_blob_wifi_settings_t *const p_wifi_settings)
{
    if (!wifiman_config_nvs_get_blob(handle, "ssid", p_ssid->ssid_buf, MAX_SSID_SIZE))
    {
        return false;
    }
    if (!wifiman_config_nvs_get_blob(handle, "password", p_password->password_buf, MAX_PASSWORD_SIZE))
    {
        return false;
    }
    if (!wifiman_config_nvs_get_blob(handle, "settings", p_wifi_settings, sizeof(*p_wifi_settings)))
    {
        return false;
    }
    return true;
}

bool
wifi_manager_cfg_blob_read(wifiman_config_t *const p_cfg)
{
    nvs_handle handle = 0;
    if (!wifiman_config_nvs_open(NVS_READONLY, &handle))
    {
        LOG_DBG("%s failed", "wifiman_config_nvs_open");
        return false;
    }

    wifiman_cfg_blob_wifi_ssid_t     sta_ssid      = { 0 };
    wifiman_cfg_blob_wifi_password_t sta_password  = { 0 };
    wifiman_cfg_blob_wifi_settings_t wifi_settings = { 0 };
    const bool res = wifiman_cfg_blob_read_by_handle(handle, &sta_ssid, &sta_password, &wifi_settings);

    nvs_close(handle);

    if (!res)
    {
        LOG_DBG("%s failed", "wifiman_cfg_blob_read_by_handle");
        return false;
    }

    *p_cfg = *wifiman_default_config_get();

    p_cfg->wifi_config_ap.channel     = wifi_settings.ap_channel;
    p_cfg->wifi_config_ap.ssid_hidden = (0 != wifi_settings.ap_ssid_hidden) ? 1 : 0;

    switch (wifi_settings.ap_bandwidth)
    {
        case WIFI_BW_HT20:
        case WIFI_BW_HT40:
            p_cfg->wifi_settings_ap.ap_bandwidth = wifi_settings.ap_bandwidth;
            break;
        default:
            LOG_WARN("%s: Unknown ap_bandwidth=%d, force set to WIFI_BW_HT20", __func__, wifi_settings.ap_bandwidth);
            p_cfg->wifi_settings_ap.ap_bandwidth = WIFI_BW_HT20;
            break;
    }

    (void)snprintf((char *)p_cfg->wifi_config_sta.ssid, sizeof(p_cfg->wifi_config_sta.ssid), "%s", sta_ssid.ssid_buf);
    (void)snprintf(
        (char *)p_cfg->wifi_config_sta.password,
        sizeof(p_cfg->wifi_config_sta.password),
        "%s",
        sta_password.password_buf);

    switch (wifi_settings.sta_power_save)
    {
        case WIFI_PS_NONE:
        case WIFI_PS_MIN_MODEM:
        case WIFI_PS_MAX_MODEM:
            p_cfg->wifi_settings_sta.sta_power_save = wifi_settings.sta_power_save;
            break;
        default:
            LOG_WARN(
                "%s: Unknown sta_power_save=%d, force set to WIFI_PS_NONE",
                __func__,
                wifi_settings.sta_power_save);
            p_cfg->wifi_settings_sta.sta_power_save = WIFI_PS_NONE;
            break;
    }
    p_cfg->wifi_settings_sta.sta_static_ip        = !!wifi_settings.sta_static_ip;
    p_cfg->wifi_settings_sta.sta_static_ip_config = wifi_settings.sta_static_ip_config;

    return true;
}

bool
wifi_manager_cfg_blob_erase_if_exist(void)
{
    nvs_handle handle = 0;
    if (!wifiman_config_nvs_open(NVS_READWRITE, &handle))
    {
        return false;
    }

    LOG_INFO("Erase deprecated wifi_cfg BLOB");

    bool res = true;

    esp_err_t err = nvs_erase_all(handle);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "nvs_erase_all");
        res = false;
    }
    err = nvs_commit(handle);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "nvs_commit");
        res = false;
    }
    nvs_close(handle);
    return res;
}
