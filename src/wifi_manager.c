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

@file wifi_manager.c
@author Tony Pottier
@brief Defines all functions necessary for esp32 to connect to a wifi/scan wifis

Contains the freeRTOS task and all necessary support

@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#include "wifi_manager.h"
#include "wifi_manager_internal.h"
#include <stdio.h>
#include <stdbool.h>
#include "esp_system.h"
#include <esp_task_wdt.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "nvs.h"
#include "lwip/ip4_addr.h"
#include "esp_netif.h"
#include "json_network_info.h"
#include "json_access_points.h"
#include "sta_ip_safe.h"
#include "ap_ssid.h"
#include "wifiman_msg.h"
#include "http_req.h"
#include "os_timer.h"
#include "http_server_ecdh.h"
#include "wifiman_config.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

/* @brief tag used for ESP serial console messages */
static const char TAG[] = "wifi_manager";

EventGroupHandle_t        g_p_wifi_manager_event_group;
static StaticEventGroup_t g_wifi_manager_event_group_mem;

static os_timer_periodic_cptr_without_arg_t* g_p_wifi_manager_timer_task_watchdog;
static os_timer_periodic_static_t            g_wifi_manager_timer_task_watchdog_mem;

void
wifi_manager_disconnect_eth(void)
{
    wifiman_msg_send_cmd_disconnect_eth();
}

void
wifi_manager_disconnect_wifi(void)
{
    wifiman_msg_send_cmd_disconnect_sta();
}

bool
wifi_manager_start(
    const bool                                 flag_connect_sta,
    const wifiman_config_t* const              p_wifi_cfg,
    const wifi_manager_antenna_config_t* const p_wifi_ant_config,
    const wifi_manager_callbacks_t* const      p_callbacks,
    wifi_manager_ecdh_f_rng                    f_rng,
    void*                                      p_rng)
{
    wifi_manager_init_mutex();
    wifi_manager_lock();

    if (NULL == g_p_wifi_manager_event_group)
    {
        // wifi_manager can be re-started after stopping,
        // this global variable is not released on stopping,
        // so, we need to initialize it only on the first start.
        g_p_wifi_manager_event_group = xEventGroupCreateStatic(&g_wifi_manager_event_group_mem);
    }

    if (!http_server_ecdh_init(f_rng, p_rng))
    {
        xEventGroupClearBits(g_p_wifi_manager_event_group, WIFI_MANAGER_IS_WORKING);
        wifi_manager_unlock();
        return false;
    }

    if (!wifi_manager_init(flag_connect_sta, p_wifi_cfg, p_wifi_ant_config, p_callbacks))
    {
        xEventGroupClearBits(g_p_wifi_manager_event_group, WIFI_MANAGER_IS_WORKING);
        wifi_manager_unlock();
        return false;
    }

    wifi_manager_unlock();
    return true;
}

void
wifi_manager_set_config_ap(const wifiman_config_ap_t* const p_wifi_cfg_ap)
{
    wifiman_config_ap_set(p_wifi_cfg_ap);
    wifi_manager_esp_wifi_configure_ap();
}

void
wifi_manager_set_config_sta(const wifiman_config_sta_t* const p_wifi_cfg_sta)
{
    wifiman_config_sta_set(p_wifi_cfg_sta);

    /* STA - Wifi Station configuration setup */
    wifi_manager_netif_configure_sta();

    /* by default the mode is STA because wifi_manager will not start the access point unless it has to! */
    const esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ESP_OK != err)
    {
        LOG_ERR("%s failed", "esp_wifi_set_mode");
    }
}

bool
wifi_manager_reconfigure(const bool flag_connect_sta, const wifiman_config_t* const p_wifi_cfg)
{
    wifi_manager_set_config_ap(&p_wifi_cfg->ap);
    wifi_manager_set_config_sta(&p_wifi_cfg->sta);

    esp_netif_t* const       p_netif_sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    const wifiman_hostinfo_t hostinfo    = wifiman_config_sta_get_hostinfo();
    LOG_INFO("### Set hostinfo for WiFi interface: %s", hostinfo.hostname.buf);
    esp_err_t err = esp_netif_set_hostname(p_netif_sta, hostinfo.hostname.buf);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_netif_set_hostname");
    }
    if (flag_connect_sta)
    {
        wifi_manager_reconnect_sta();
    }
    return true;
}

void
wifi_manager_update_network_connection_info(
    const update_reason_code_e       update_reason_code,
    const wifiman_wifi_ssid_t* const p_ssid,
    const esp_netif_ip_info_t* const p_ip_info,
    const esp_ip4_addr_t* const      p_dhcp_ip)
{
    network_info_str_t ip_info_str = {
        .ip      = { "0" },
        .gw      = { "0" },
        .netmask = { "0" },
        .dhcp    = { "" },
    };
    if (UPDATE_CONNECTION_OK == update_reason_code)
    {
        if (NULL == p_ssid)
        {
            LOG_INFO("WIFI_MANAGER:EV_STATE: Set WIFI_MANAGER_ETH_CONNECTED_BIT");
            xEventGroupSetBits(g_p_wifi_manager_event_group, WIFI_MANAGER_ETH_CONNECTED_BIT);
        }
        else
        {
            LOG_INFO("WIFI_MANAGER:EV_STATE: Set WIFI_MANAGER_WIFI_CONNECTED_BIT");
            xEventGroupSetBits(g_p_wifi_manager_event_group, WIFI_MANAGER_WIFI_CONNECTED_BIT);
        }
        if (NULL != p_ip_info)
        {
            /* save IP as a string for the HTTP server host */
            sta_ip_safe_set(p_ip_info->ip.addr);

            esp_ip4addr_ntoa(&p_ip_info->ip, ip_info_str.ip, sizeof(ip_info_str.ip));
            esp_ip4addr_ntoa(&p_ip_info->netmask, ip_info_str.netmask, sizeof(ip_info_str.netmask));
            esp_ip4addr_ntoa(&p_ip_info->gw, ip_info_str.gw, sizeof(ip_info_str.gw));
            if (NULL != p_dhcp_ip)
            {
                esp_ip4addr_ntoa(p_dhcp_ip, ip_info_str.dhcp.buf, sizeof(ip_info_str.dhcp.buf));
                LOG_INFO("DHCP IP: %s", ip_info_str.dhcp.buf);
            }
        }
        else
        {
            sta_ip_safe_reset();
        }
    }
    else
    {
        sta_ip_safe_reset();
        LOG_INFO("WIFI_MANAGER:EV_STATE: Clear WIFI_MANAGER_WIFI_CONNECTED_BIT | WIFI_MANAGER_ETH_CONNECTED_BIT");
        xEventGroupClearBits(
            g_p_wifi_manager_event_group,
            WIFI_MANAGER_WIFI_CONNECTED_BIT | WIFI_MANAGER_ETH_CONNECTED_BIT);
    }
    json_network_info_update(p_ssid, &ip_info_str, update_reason_code);
}

void
wifi_manager_connect_async(void)
{
    /* in order to avoid a false positive on the front end app we need to quickly flush the ip json
     * There'se a risk the front end sees an IP or a password error when in fact
     * it's a remnant from a previous connection
     */
    wifi_manager_lock();
    json_network_info_clear();
    wifi_manager_unlock();
    LOG_INFO("%s: wifiman_msg_send_cmd_connect_sta: CONNECTION_REQUEST_USER", __func__);
    wifiman_msg_send_cmd_connect_sta(CONNECTION_REQUEST_USER);
}

void
wifi_manager_stop_ap(void)
{
    LOG_INFO("%s", __func__);
    wifiman_msg_send_cmd_stop_ap();
}

void
wifi_manager_start_ap(const bool flag_block_req_from_lan)
{
    LOG_INFO("%s", __func__);
    wifiman_msg_send_cmd_start_ap(flag_block_req_from_lan);
}

void
wifi_manager_stop(void)
{
    if (!wifiman_msg_send_cmd_stop_and_destroy())
    {
        LOG_ERR("%s failed", "wifiman_msg_send_cmd_stop_and_destroy");
    }
}

static void
wifi_manager_timer_cb_task_watchdog_feed(const os_timer_periodic_cptr_without_arg_t* const p_timer)
{
    (void)p_timer;
    wifiman_msg_send_cmd_task_watchdog_feed();
}

static void
wifi_manager_wdt_add_and_start(void)
{
    LOG_INFO("TaskWatchdog: Register current thread");
    const esp_err_t err = esp_task_wdt_add(xTaskGetCurrentTaskHandle());
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_add");
    }
    LOG_INFO("TaskWatchdog: Start timer");
    os_timer_periodic_cptr_without_arg_start(g_p_wifi_manager_timer_task_watchdog);
}

void
wifi_manager_task(void)
{
    LOG_INFO("TaskWatchdog: Create timer");
    g_p_wifi_manager_timer_task_watchdog = os_timer_periodic_cptr_without_arg_create_static(
        &g_wifi_manager_timer_task_watchdog_mem,
        "wifi:wdog",
        WIFI_MANAGER_TASK_WATCHDOG_FEEDING_PERIOD_TICKS,
        &wifi_manager_timer_cb_task_watchdog_feed);

    wifi_manager_wdt_add_and_start();

    for (;;)
    {
        if (wifi_manager_recv_and_handle_msg())
        {
            break;
        }
    }

    LOG_INFO("Finish task");
    wifi_manager_lock();

    LOG_INFO("TaskWatchdog: Unregister current thread");
    esp_task_wdt_delete(xTaskGetCurrentTaskHandle());
    LOG_INFO("TaskWatchdog: Stop timer");
    os_timer_periodic_cptr_without_arg_stop(g_p_wifi_manager_timer_task_watchdog);
    LOG_INFO("TaskWatchdog: Delete timer");
    os_timer_periodic_cptr_without_arg_delete(&g_p_wifi_manager_timer_task_watchdog);

    wifi_manager_stop_timer_reconnect_sta_after_timeout();
    wifi_manager_scan_timer_stop();

    // Do not delete gh_wifi_json_mutex
    // Do not delete g_p_wifi_manager_event_group
    xEventGroupClearBits(
        g_p_wifi_manager_event_group,
        WIFI_MANAGER_IS_WORKING | WIFI_MANAGER_WIFI_CONNECTED_BIT | WIFI_MANAGER_AP_STA_CONNECTED_BIT
            | WIFI_MANAGER_AP_STA_IP_ASSIGNED_BIT | WIFI_MANAGER_AP_STARTED_BIT | WIFI_MANAGER_REQUEST_STA_CONNECT_BIT
            | WIFI_MANAGER_REQUEST_RESTORE_STA_BIT | WIFI_MANAGER_SCAN_BIT | WIFI_MANAGER_REQUEST_DISCONNECT_BIT
            | WIFI_MANAGER_AP_ACTIVE);

    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_manager_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_manager_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &wifi_manager_event_handler);

    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();

    /* heap buffers */
    json_network_info_deinit();
    sta_ip_safe_deinit();

    wifiman_msg_deinit();

    esp_netif_t* const p_netif_sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_action_stop(p_netif_sta, NULL, 0, NULL);

    esp_netif_t* const p_netif_ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    esp_netif_action_stop(p_netif_ap, NULL, 0, NULL);

    wifi_manager_unlock();
}

bool
wifi_manager_is_working(void)
{
    return (0 != (xEventGroupGetBits(g_p_wifi_manager_event_group) & WIFI_MANAGER_IS_WORKING));
}

bool
wifi_manager_is_ap_active(void)
{
    return (0 != (xEventGroupGetBits(g_p_wifi_manager_event_group) & WIFI_MANAGER_AP_ACTIVE));
}

bool
wifi_manager_is_req_from_lan_blocked_while_ap_is_active(void)
{
    return (
        (WIFI_MANAGER_AP_ACTIVE | WIFI_MANAGER_BLOCK_REQ_FROM_LAN_WHILE_AP_ACTIVE)
        == (xEventGroupGetBits(g_p_wifi_manager_event_group)
            & (WIFI_MANAGER_AP_ACTIVE | WIFI_MANAGER_BLOCK_REQ_FROM_LAN_WHILE_AP_ACTIVE)));
}

bool
wifi_manager_is_connected_to_wifi(void)
{
    return (0 != (xEventGroupGetBits(g_p_wifi_manager_event_group) & WIFI_MANAGER_WIFI_CONNECTED_BIT));
}

bool
wifi_manager_is_connected_to_ethernet(void)
{
    return (0 != (xEventGroupGetBits(g_p_wifi_manager_event_group) & WIFI_MANAGER_ETH_CONNECTED_BIT));
}

bool
wifi_manager_is_connected_to_wifi_or_ethernet(void)
{
    const uint32_t event_group_bit_mask = WIFI_MANAGER_WIFI_CONNECTED_BIT | WIFI_MANAGER_ETH_CONNECTED_BIT;
    return (0 != (xEventGroupGetBits(g_p_wifi_manager_event_group) & event_group_bit_mask));
}

bool
wifi_manager_is_ap_sta_ip_assigned(void)
{
    return (0 != (xEventGroupGetBits(g_p_wifi_manager_event_group) & WIFI_MANAGER_AP_STA_IP_ASSIGNED_BIT));
}

bool
wifi_manager_is_sta_configured(void)
{
    return wifiman_config_sta_is_ssid_configured();
}

void
wifi_manager_set_extra_info_for_status_json(const char* const p_extra)
{
    json_network_set_extra_info(p_extra);
}
