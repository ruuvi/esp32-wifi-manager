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
#include "wifi_manager_defs.h"
#include "wifiman_cfg_blob_convert.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#if (LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG) && !RUUVI_TESTS
#warning Debug log level prints out the passwords as a "plaintext".
#endif

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
wifiman_cfg_blob_read_by_handle(const nvs_handle handle, wifiman_cfg_blob_t *const p_blob)
{
    if (!wifiman_config_nvs_get_blob(handle, "ssid", p_blob->sta_ssid.ssid_buf, MAX_SSID_SIZE))
    {
        return false;
    }
    if (!wifiman_config_nvs_get_blob(handle, "password", p_blob->sta_password.password_buf, MAX_PASSWORD_SIZE))
    {
        return false;
    }
    if (!wifiman_config_nvs_get_blob(handle, "settings", &p_blob->wifi_settings, sizeof(p_blob->wifi_settings)))
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

    wifiman_cfg_blob_t cfg_blob = { 0 };
    const bool         res      = wifiman_cfg_blob_read_by_handle(handle, &cfg_blob);

    nvs_close(handle);

    if (!res)
    {
        LOG_DBG("%s failed", "wifiman_cfg_blob_read_by_handle");
        return false;
    }

    wifiman_cfg_blob_convert(&cfg_blob, p_cfg);

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
