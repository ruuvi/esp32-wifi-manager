/**
 * @file wifiman_config.c
 * @author TheSomeMan
 * @date 2022-04-13
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "wifiman_config.h"
#include <string.h>
#include "esp_wifi_types.h"
#include "wifi_manager_defs.h"
#include "wifi_manager_internal.h"
#include "wifi_manager.h"
#include "os_mutex.h"

#if 0
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

static const char TAG[] = "wifi_manager";
#endif

typedef void (*wifiman_const_config_callback_without_param_t)(const wifiman_config_t* const p_cfg);
typedef void (*wifiman_const_config_callback_void_t)(const wifiman_config_t* const p_cfg, void* const p_param);
typedef void (*wifiman_config_callback_void_cptr_t)(wifiman_config_t* const p_cfg, const void* const p_param);

static const wifiman_config_t g_wifiman_config_default_const = {
    .ap = {
        .wifi_config_ap = {
            /* Soft-AP SSID is initialized by wifiman_default_config_init */
            .ssid = { "" },

            /* Defines access point's password.
         * In the case of an open access point, the password must be an empty string "",
         * '.authmode' in this case will be set to WIFI_AUTH_OPEN automatically,
         * otherwise '.authmode' will be WIFI_AUTH_WPA2_PSK */
            .password = { CONFIG_DEFAULT_AP_PASSWORD },

            .ssid_len = 0,

            /* Defines access point's channel.
         *  Good practice for minimal channel interference to use
         *  For 20 MHz: 1, 6 or 11 in USA and 1, 5, 9 or 13 in most parts of the world
         *  For 40 MHz: 3 in USA and 3 or 11 in most parts of the world */
            .channel = CONFIG_DEFAULT_AP_CHANNEL,

            /* Defines auth mode for Soft-AP
         * By default WIFI_AUTH_WPA2_PSK,
         * if password is empty, then auth mode will be changed to WIFI_AUTH_OPEN automatically */
            .authmode = WIFI_AUTH_WPA2_PSK,

            .ssid_hidden = 0,
            .max_connection = CONFIG_DEFAULT_AP_MAX_CONNECTIONS,
            .beacon_interval = CONFIG_DEFAULT_AP_BEACON_INTERVAL,
        },
        .wifi_settings_ap = {

            /* Defines access point's bandwidth.
         *  Value: WIFI_BW_HT20 for 20 MHz  or  WIFI_BW_HT40 for 40 MHz
         *  20 MHz minimize channel interference but is not suitable for
         *  applications with high data speeds */
            .ap_bandwidth = WIFI_BW_HT20,

            .ap_ip = { CONFIG_DEFAULT_AP_IP },
            .ap_gw = { CONFIG_DEFAULT_AP_GATEWAY },
            .ap_netmask = { CONFIG_DEFAULT_AP_NETMASK },
        },
    },
    .sta = {
        .wifi_config_sta = {
            .ssid = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
            .password = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
            .scan_method = WIFI_FAST_SCAN,
            .bssid_set = false,
            .bssid = {0, 0, 0, 0, 0, 0},
            .channel = 0,
            .listen_interval = 0,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            .threshold = {
                .rssi = 0,
                .authmode = WIFI_AUTH_OPEN
            },
            .pmf_cfg = {
                .capable = false,
                .required = false,
            },
        },
        .wifi_settings_sta = {
            .sta_power_save       = WIFI_PS_NONE,
            .sta_static_ip        = false,
            .sta_static_ip_config = {
                .ip               = { 0 },
                .netmask          = { 0 },
                .gw               = { 0 },
            },
        },
    },
};

static wifiman_config_t  g_wifiman_config_default;
static wifiman_config_t  g_wifiman_config;
static os_mutex_static_t g_wifiman_config_mutex_mem;
static os_mutex_t        g_p_wifiman_config_mutex;

_Static_assert(
    MAX_SSID_SIZE == sizeof(g_wifiman_config.sta.wifi_config_sta.ssid),
    "sizeof(g_wifiman_config.wifi_config_sta.ssid) == MAX_SSID_SIZE");
_Static_assert(
    MAX_PASSWORD_SIZE == sizeof(g_wifiman_config.sta.wifi_config_sta.password),
    "sizeof(g_wifiman_config.wifi_config_sta.password) == MAX_PASSWORD_SIZE");

const wifiman_config_t*
wifiman_default_config_init(const wifiman_wifi_ssid_t* const p_wifi_ssid, const wifiman_hostinfo_t* const p_hostinfo)
{
    g_wifiman_config_default = g_wifiman_config_default_const;
    (void)snprintf(
        (char*)g_wifiman_config_default.ap.wifi_config_ap.ssid,
        sizeof(g_wifiman_config_default.ap.wifi_config_ap.ssid),
        "%s",
        p_wifi_ssid->ssid_buf);
    (void)snprintf(
        (char*)g_wifiman_config_default.sta.hostinfo.hostname.buf,
        sizeof(g_wifiman_config_default.sta.hostinfo.hostname.buf),
        "%s",
        p_hostinfo->hostname.buf);
    (void)snprintf(
        (char*)g_wifiman_config_default.sta.hostinfo.fw_ver.buf,
        sizeof(g_wifiman_config_default.sta.hostinfo.fw_ver.buf),
        "%s",
        p_hostinfo->fw_ver.buf);
    (void)snprintf(
        (char*)g_wifiman_config_default.sta.hostinfo.nrf52_fw_ver.buf,
        sizeof(g_wifiman_config_default.sta.hostinfo.nrf52_fw_ver.buf),
        "%s",
        p_hostinfo->nrf52_fw_ver.buf);

    g_wifiman_config_default.ap.wifi_config_ap.authmode = ('\0'
                                                           == g_wifiman_config_default.ap.wifi_config_ap.password[0])
                                                              ? WIFI_AUTH_OPEN
                                                              : WIFI_AUTH_WPA2_PSK;
    return &g_wifiman_config_default;
}

const wifiman_config_t*
wifiman_default_config_get(void)
{
    return &g_wifiman_config_default;
}

void
wifiman_default_config_set(const wifiman_config_t* const p_wifi_cfg)
{
    g_wifiman_config_default = *p_wifi_cfg;

    if ('\0' == g_wifiman_config_default.ap.wifi_config_ap.password[0])
    {
        if (WIFI_AUTH_OPEN != g_wifiman_config_default.ap.wifi_config_ap.authmode)
        {
            g_wifiman_config_default.ap.wifi_config_ap.authmode = WIFI_AUTH_OPEN;
        }
    }
    else
    {
        if (WIFI_AUTH_OPEN == g_wifiman_config_default.ap.wifi_config_ap.authmode)
        {
            g_wifiman_config_default.ap.wifi_config_ap.authmode = WIFI_AUTH_WPA2_PSK;
        }
    }
}

static wifiman_config_t*
wifiman_config_lock(void)
{
    if (NULL == g_p_wifiman_config_mutex)
    {
        g_p_wifiman_config_mutex = os_mutex_create_static(&g_wifiman_config_mutex_mem);
    }
    os_mutex_lock(g_p_wifiman_config_mutex);
    return &g_wifiman_config;
}

static void
wifiman_config_unlock(wifiman_config_t** pp_cfg)
{
    *pp_cfg = NULL;
    os_mutex_unlock(g_p_wifiman_config_mutex);
}

static void
wifiman_const_config_unlock(const wifiman_config_t** pp_cfg)
{
    *pp_cfg = NULL;
    os_mutex_unlock(g_p_wifiman_config_mutex);
}

static void
wifiman_const_config_transaction_without_param(wifiman_const_config_callback_without_param_t cb_func)
{
    const wifiman_config_t* p_cfg = wifiman_config_lock();
    cb_func(p_cfg);
    wifiman_const_config_unlock(&p_cfg);
}

static void
wifiman_const_config_safe_transaction(wifiman_const_config_callback_void_t cb_func, void* const p_param)
{
    const wifiman_config_t* p_cfg = wifiman_config_lock();
    cb_func(p_cfg, p_param);
    wifiman_const_config_unlock(&p_cfg);
}

static void
wifiman_config_safe_transaction_with_const_param(wifiman_config_callback_void_cptr_t cb_func, const void* const p_param)
{
    wifiman_config_t* p_cfg = wifiman_config_lock();
    cb_func(p_cfg, p_param);
    wifiman_config_unlock(&p_cfg);
}

static void
wifiman_config_cb_do_init(wifiman_config_t* const p_cfg, const void* const p_param)
{
    const wifiman_config_t* const p_cfg_src = p_param;
    *p_cfg                                  = *p_cfg_src;
}

void
wifiman_config_init(const wifiman_config_t* const p_wifi_cfg)
{
    return wifiman_config_safe_transaction_with_const_param(&wifiman_config_cb_do_init, p_wifi_cfg);
}

static void
wifiman_config_cb_do_config_ap_set(wifiman_config_t* const p_cfg, const void* const p_param)
{
    const wifiman_config_ap_t* const p_cfg_ap_src = p_param;
    p_cfg->ap                                     = *p_cfg_ap_src;
}

void
wifiman_config_ap_set(const wifiman_config_ap_t* const p_wifi_cfg_ap)
{
    return wifiman_config_safe_transaction_with_const_param(&wifiman_config_cb_do_config_ap_set, p_wifi_cfg_ap);
}

static void
wifiman_config_cb_do_config_sta_set(wifiman_config_t* const p_cfg, const void* const p_param)
{
    const wifiman_config_sta_t* const p_cfg_sta_src = p_param;
    p_cfg->sta                                      = *p_cfg_sta_src;
}

void
wifiman_config_sta_set(const wifiman_config_sta_t* const p_wifi_cfg_sta)
{
    return wifiman_config_safe_transaction_with_const_param(&wifiman_config_cb_do_config_sta_set, p_wifi_cfg_sta);
}

static void
wifiman_config_do_save_config_sta(const wifiman_config_t* const p_cfg)
{
    wifi_manager_cb_save_wifi_config_sta(&p_cfg->sta);
}

void
wifiman_config_sta_save(void)
{
    wifiman_const_config_transaction_without_param(&wifiman_config_do_save_config_sta);
}

static void
wifiman_config_do_sta_get_config(const wifiman_config_t* const p_cfg, void* const p_param)
{
    wifi_sta_config_t* const p_wifi_sta_cfg = p_param;
    *p_wifi_sta_cfg                         = p_cfg->sta.wifi_config_sta;
}

wifi_sta_config_t
wifiman_config_sta_get_config(void)
{
    wifi_sta_config_t wifi_sta_config = { 0 };
    wifiman_const_config_safe_transaction(&wifiman_config_do_sta_get_config, &wifi_sta_config);
    return wifi_sta_config;
}

static void
wifiman_config_sta_do_get_settings(const wifiman_config_t* const p_cfg, void* const p_param)
{
    wifi_settings_sta_t* const p_sta_settings = p_param;
    *p_sta_settings                           = p_cfg->sta.wifi_settings_sta;
}

wifi_settings_sta_t
wifiman_config_sta_get_settings(void)
{
    wifi_settings_sta_t wifi_sta_settings = { 0 };
    wifiman_const_config_safe_transaction(&wifiman_config_sta_do_get_settings, &wifi_sta_settings);
    return wifi_sta_settings;
}

static void
wifiman_config_sta_do_get_ssid(const wifiman_config_t* const p_cfg, void* const p_param)
{
    wifiman_wifi_ssid_t* const p_ssid = p_param;
    (void)snprintf(
        &p_ssid->ssid_buf[0],
        sizeof(p_ssid->ssid_buf),
        "%s",
        (const char*)&p_cfg->sta.wifi_config_sta.ssid[0]);
}

wifiman_wifi_ssid_t
wifiman_config_sta_get_ssid(void)
{
    wifiman_wifi_ssid_t ssid = { 0 };
    wifiman_const_config_safe_transaction(&wifiman_config_sta_do_get_ssid, &ssid);
    return ssid;
}

bool
wifiman_config_sta_is_ssid_configured(void)
{
    wifiman_wifi_ssid_t ssid = { 0 };
    wifiman_const_config_safe_transaction(&wifiman_config_sta_do_get_ssid, &ssid);
    return ('\0' != ssid.ssid_buf[0]) ? true : false;
}

typedef struct wifiman_set_ssid_password_t
{
    const wifiman_wifi_ssid_t* const     p_ssid;
    const wifiman_wifi_password_t* const p_password;
} wifiman_set_ssid_password_t;

static void
wifiman_config_sta_do_set_ssid_and_password(wifiman_config_t* const p_cfg, const void* const p_param)
{
    const wifiman_set_ssid_password_t* const p_info = p_param;

    memset(&p_cfg->sta.wifi_config_sta, 0x00, sizeof(p_cfg->sta.wifi_config_sta));
    (void)snprintf(
        (char*)p_cfg->sta.wifi_config_sta.ssid,
        sizeof(p_cfg->sta.wifi_config_sta.ssid),
        "%s",
        p_info->p_ssid->ssid_buf);
    (void)snprintf(
        (char*)p_cfg->sta.wifi_config_sta.password,
        sizeof(p_cfg->sta.wifi_config_sta.password),
        "%s",
        (NULL != p_info->p_password) ? p_info->p_password->password_buf : "");
}

void
wifiman_config_sta_set_ssid_and_password(
    const wifiman_wifi_ssid_t* const     p_ssid,
    const wifiman_wifi_password_t* const p_password)
{
    wifiman_set_ssid_password_t info = {
        .p_ssid     = p_ssid,
        .p_password = p_password,
    };
    wifiman_config_safe_transaction_with_const_param(&wifiman_config_sta_do_set_ssid_and_password, &info);
}

static void
wifiman_config_sta_do_get_hostinfo(const wifiman_config_t* const p_cfg, void* const p_param)
{
    wifiman_hostinfo_t* const p_hostinfo = p_param;
    *p_hostinfo                          = p_cfg->sta.hostinfo;
}

wifiman_hostinfo_t
wifiman_config_sta_get_hostinfo(void)
{
    wifiman_hostinfo_t hostinfo = { 0 };
    wifiman_const_config_safe_transaction(&wifiman_config_sta_do_get_hostinfo, &hostinfo);
    return hostinfo;
}

static void
wifiman_config_ap_do_get_config(const wifiman_config_t* const p_cfg, void* const p_param)
{
    wifi_ap_config_t* const p_ap_config = p_param;
    *p_ap_config                        = p_cfg->ap.wifi_config_ap;
}

wifi_ap_config_t
wifiman_config_ap_get_config(void)
{
    wifi_ap_config_t wifi_ap_config = { 0 };
    wifiman_const_config_safe_transaction(&wifiman_config_ap_do_get_config, &wifi_ap_config);
    return wifi_ap_config;
}

static void
wifiman_config_ap_do_get_settings(const wifiman_config_t* const p_cfg, void* const p_param)
{
    wifi_settings_ap_t* const p_ap_settings = p_param;
    *p_ap_settings                          = p_cfg->ap.wifi_settings_ap;
}

wifi_settings_ap_t
wifiman_config_ap_get_settings(void)
{
    wifi_settings_ap_t wifi_ap_settings = { 0 };
    wifiman_const_config_safe_transaction(&wifiman_config_ap_do_get_settings, &wifi_ap_settings);
    return wifi_ap_settings;
}

wifiman_ip4_addr_str_t
wifiman_config_ap_get_ip_str(void)
{
    wifi_settings_ap_t wifi_ap_settings = { 0 };
    wifiman_const_config_safe_transaction(&wifiman_config_ap_do_get_settings, &wifi_ap_settings);
    return wifi_ap_settings.ap_ip;
}

esp_ip4_addr_t
wifiman_config_ap_get_ip(void)
{
    wifiman_ip4_addr_str_t ap_ip_str = wifiman_config_ap_get_ip_str();
    const esp_ip4_addr_t   ip_addr   = {
            .addr = esp_ip4addr_aton(ap_ip_str.buf),
    };
    return ip_addr;
}
