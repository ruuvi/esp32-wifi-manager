#include "json_ip_info.h"
#include <stddef.h>
#include <stdio.h>
#include "json.h"
#include "wifi_manager_defs.h"

static char g_ip_info_json[JSON_IP_INFO_SIZE];

bool
json_ip_info_init(void)
{
    json_ip_info_clear();
    return true;
}

void
json_ip_info_deinit(void)
{
}

const char *
json_ip_info_get(void)
{
    return g_ip_info_json;
}

void
json_ip_info_generate(
    const char *              ssid,
    const network_info_str_t *p_network_info,
    update_reason_code_t      update_reason_code)
{
    if (NULL != ssid)
    {
        int32_t buf_idx = 0;
        json_snprintf(&buf_idx, g_ip_info_json, sizeof(g_ip_info_json), "{\"ssid\":");
        json_print_escaped_string(&buf_idx, g_ip_info_json, sizeof(g_ip_info_json), ssid);

        if (UPDATE_CONNECTION_OK != update_reason_code)
        {
            static const network_info_str_t g_network_info_empty = {
                .ip      = { "0" },
                .gw      = { "0" },
                .netmask = { "0" },
            };
            p_network_info = &g_network_info_empty;
        }

        json_snprintf(
            &buf_idx,
            g_ip_info_json,
            sizeof(g_ip_info_json),
            ",\"ip\":\"%s\",\"netmask\":\"%s\",\"gw\":\"%s\",\"urc\":%d}\n",
            p_network_info->ip,
            p_network_info->netmask,
            p_network_info->gw,
            (int)update_reason_code);
    }
    else
    {
        json_ip_info_clear();
    }
}

void
json_ip_info_clear(void)
{
    snprintf(g_ip_info_json, sizeof(g_ip_info_json), "{}\n");
}