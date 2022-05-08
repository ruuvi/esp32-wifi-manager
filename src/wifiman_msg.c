/**
 * @file wifiman_msg.c
 * @author TheSomeMan
 * @date 2020-11-07
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "wifiman_msg.h"
#include "wifi_manager_defs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "log.h"
#include "wifi_manager_internal.h"

static QueueHandle_t gh_wifiman_msg_queue;

/* @brief tag used for ESP serial console messages */
static const char TAG[] = "wifi_manager";

bool
wifiman_msg_init(void)
{
    const uint32_t queue_length = 3U;

    gh_wifiman_msg_queue = xQueueCreate(queue_length, sizeof(queue_message));
    if (NULL == gh_wifiman_msg_queue)
    {
        LOG_ERR("%s failed", "xQueueCreate");
        return false;
    }
    return true;
}

void
wifiman_msg_deinit(void)
{
    vQueueDelete(gh_wifiman_msg_queue);
    gh_wifiman_msg_queue = NULL;
}

bool
wifiman_msg_recv(queue_message *p_msg)
{
    const BaseType_t xStatus = xQueueReceive(gh_wifiman_msg_queue, p_msg, portMAX_DELAY);
    if (pdPASS != xStatus)
    {
        LOG_ERR("%s failed", "xQueueReceive");
        return false;
    }
    return true;
}

connection_request_made_by_code_e
wifiman_conv_param_to_conn_req(const wifiman_msg_param_t *p_param)
{
    const connection_request_made_by_code_e conn_req = (connection_request_made_by_code_e)p_param->val;
    return conn_req;
}

sta_ip_address_t
wifiman_conv_param_to_ip_addr(const wifiman_msg_param_t *p_param)
{
    const sta_ip_address_t ip_addr = p_param->val;
    return ip_addr;
}

wifiman_disconnection_reason_t
wifiman_conv_param_to_reason(const wifiman_msg_param_t *p_param)
{
    const wifiman_disconnection_reason_t reason = (wifiman_disconnection_reason_t)p_param->val;
    return reason;
}

const char *
wifiman_disconnection_reason_to_str(const wifiman_disconnection_reason_t reason)
{
    switch (reason)
    {
        case WIFI_REASON_UNSPECIFIED:
            return "UNSPECIFIED";
        case WIFI_REASON_AUTH_EXPIRE:
            return "AUTH_EXPIRE";
        case WIFI_REASON_AUTH_LEAVE:
            return "AUTH_LEAVE";
        case WIFI_REASON_ASSOC_EXPIRE:
            return "ASSOC_EXPIRE";
        case WIFI_REASON_ASSOC_TOOMANY:
            return "ASSOC_TOOMANY";
        case WIFI_REASON_NOT_AUTHED:
            return "NOT_AUTHED";
        case WIFI_REASON_NOT_ASSOCED:
            return "NOT_ASSOCED";
        case WIFI_REASON_ASSOC_LEAVE:
            return "ASSOC_LEAVE";
        case WIFI_REASON_ASSOC_NOT_AUTHED:
            return "ASSOC_NOT_AUTHED";
        case WIFI_REASON_DISASSOC_PWRCAP_BAD:
            return "DISASSOC_PWRCAP_BAD";
        case WIFI_REASON_DISASSOC_SUPCHAN_BAD:
            return "DISASSOC_SUPCHAN_BAD";
        case WIFI_REASON_IE_INVALID:
            return "IE_INVALID";
        case WIFI_REASON_MIC_FAILURE:
            return "MIC_FAILURE";
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
            return "4WAY_HANDSHAKE_TIMEOUT";
        case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
            return "GROUP_KEY_UPDATE_TIMEOUT";
        case WIFI_REASON_IE_IN_4WAY_DIFFERS:
            return "IE_IN_4WAY_DIFFERS";
        case WIFI_REASON_GROUP_CIPHER_INVALID:
            return "GROUP_CIPHER_INVALID";
        case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
            return "PAIRWISE_CIPHER_INVALID";
        case WIFI_REASON_AKMP_INVALID:
            return "AKMP_INVALID";
        case WIFI_REASON_UNSUPP_RSN_IE_VERSION:
            return "UNSUPP_RSN_IE_VERSION";
        case WIFI_REASON_INVALID_RSN_IE_CAP:
            return "INVALID_RSN_IE_CAP";
        case WIFI_REASON_802_1X_AUTH_FAILED:
            return "802_1X_AUTH_FAILED";
        case WIFI_REASON_CIPHER_SUITE_REJECTED:
            return "CIPHER_SUITE_REJECTED";
        case WIFI_REASON_INVALID_PMKID:
            return "INVALID_PMKID";
        case WIFI_REASON_BEACON_TIMEOUT:
            return "BEACON_TIMEOUT";
        case WIFI_REASON_NO_AP_FOUND:
            return "NO_AP_FOUND";
        case WIFI_REASON_AUTH_FAIL:
            return "AUTH_FAIL";
        case WIFI_REASON_ASSOC_FAIL:
            return "ASSOC_FAIL";
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
            return "HANDSHAKE_TIMEOUT";
        case WIFI_REASON_CONNECTION_FAIL:
            return "CONNECTION_FAIL";
        case WIFI_REASON_AP_TSF_RESET:
            return "AP_TSF_RESET";
    }
    return "Unknown";
}

static bool
wifiman_msg_send(const message_code_e code, const wifiman_msg_param_t msg_param)
{
    const queue_message msg = {
        .code      = code,
        .msg_param = msg_param,
    };
    wifi_manager_lock();
    // need to lock access to wifi_manager since gh_wifiman_msg_queue can be de-initialized asynchronously
    if (NULL == gh_wifiman_msg_queue)
    {
        LOG_WARN("wifiman_msg is not initialized");
        wifi_manager_unlock();
        return false;
    }
    if (pdTRUE != xQueueSend(gh_wifiman_msg_queue, &msg, portMAX_DELAY))
    {
        LOG_ERR("%s failed", "xQueueSend");
        wifi_manager_unlock();
        return false;
    }
    wifi_manager_unlock();
    return true;
}

bool
wifiman_msg_send_cmd_task_watchdog_feed(void)
{
    const wifiman_msg_param_t msg_param = {
        .ptr = NULL,
    };
    return wifiman_msg_send(ORDER_TASK_WATCHDOG_FEED, msg_param);
}

bool
wifiman_msg_send_cmd_start_ap(void)
{
    const wifiman_msg_param_t msg_param = {
        .ptr = NULL,
    };
    return wifiman_msg_send(ORDER_START_AP, msg_param);
}

bool
wifiman_msg_send_cmd_stop_ap(void)
{
    const wifiman_msg_param_t msg_param = {
        .ptr = NULL,
    };
    return wifiman_msg_send(ORDER_STOP_AP, msg_param);
}

bool
wifiman_msg_send_cmd_connect_eth(void)
{
    const wifiman_msg_param_t msg_param = {
        .ptr = NULL,
    };
    return wifiman_msg_send(ORDER_CONNECT_ETH, msg_param);
}

bool
wifiman_msg_send_cmd_connect_sta(const connection_request_made_by_code_e conn_req_code)
{
    const wifiman_msg_param_t msg_param = {
        .val = conn_req_code,
    };
    return wifiman_msg_send(ORDER_CONNECT_STA, msg_param);
}

bool
wifiman_msg_send_cmd_disconnect_eth(void)
{
    const wifiman_msg_param_t msg_param = {
        .ptr = NULL,
    };
    LOG_INFO("Send msg: ORDER_DISCONNECT_ETH");
    return wifiman_msg_send(ORDER_DISCONNECT_ETH, msg_param);
}

bool
wifiman_msg_send_cmd_disconnect_sta(void)
{
    const wifiman_msg_param_t msg_param = {
        .ptr = NULL,
    };
    LOG_INFO("Send msg: ORDER_DISCONNECT_STA");
    return wifiman_msg_send(ORDER_DISCONNECT_STA, msg_param);
}

bool
wifiman_msg_send_cmd_stop_and_destroy(void)
{
    const wifiman_msg_param_t msg_param = {
        .ptr = NULL,
    };
    LOG_INFO("Send msg: ORDER_STOP_AND_DESTROY");
    return wifiman_msg_send(ORDER_STOP_AND_DESTROY, msg_param);
}

bool
wifiman_msg_send_cmd_start_wifi_scan(void)
{
    const wifiman_msg_param_t msg_param = {
        .ptr = NULL,
    };
    return wifiman_msg_send(ORDER_START_WIFI_SCAN, msg_param);
}

bool
wifiman_msg_send_ev_scan_next(void)
{
    const wifiman_msg_param_t msg_param = {
        .ptr = NULL,
    };
    return wifiman_msg_send(EVENT_SCAN_NEXT, msg_param);
}

bool
wifiman_msg_send_ev_scan_done(void)
{
    const wifiman_msg_param_t msg_param = {
        .ptr = NULL,
    };
    return wifiman_msg_send(EVENT_SCAN_DONE, msg_param);
}

bool
wifiman_msg_send_ev_got_ip(const sta_ip_address_t ip_addr)
{
    const wifiman_msg_param_t msg_param = {
        .val = ip_addr,
    };
    return wifiman_msg_send(EVENT_STA_GOT_IP, msg_param);
}

bool
wifiman_msg_send_ev_ap_sta_connected(void)
{
    const wifiman_msg_param_t msg_param = {
        .val = 0,
    };
    return wifiman_msg_send(EVENT_AP_STA_CONNECTED, msg_param);
}

bool
wifiman_msg_send_ev_ap_sta_disconnected(void)
{
    const wifiman_msg_param_t msg_param = {
        .val = 0,
    };
    return wifiman_msg_send(EVENT_AP_STA_DISCONNECTED, msg_param);
}

bool
wifiman_msg_send_ev_ap_sta_ip_assigned(void)
{
    const wifiman_msg_param_t msg_param = {
        .val = 0,
    };
    return wifiman_msg_send(EVENT_AP_STA_IP_ASSIGNED, msg_param);
}

bool
wifiman_msg_send_ev_disconnected(const wifiman_disconnection_reason_t reason)
{
    const wifiman_msg_param_t msg_param = {
        .val = reason,
    };
    return wifiman_msg_send(EVENT_STA_DISCONNECTED, msg_param);
}
