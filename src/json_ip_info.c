#include "json_ip_info.h"
#include <stddef.h>
#include "json.h"
#include "wifi_manager_defs.h"

static char g_json_ip_info_buf[JSON_IP_INFO_SIZE];

void
json_ip_info_init(void)
{
    json_ip_info_clear();
}

void
json_ip_info_deinit(void)
{
}

const char *
json_ip_info_get(void)
{
    return g_json_ip_info_buf;
}

void
json_ip_info_generate(
    const char *              ssid,
    const network_info_str_t *p_network_info,
    update_reason_code_t      update_reason_code)
{
    if (NULL != ssid)
    {
        str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(g_json_ip_info_buf);
        str_buf_printf(&str_buf, "{\"ssid\":");
        json_print_escaped_string(&str_buf, ssid);

        if (UPDATE_CONNECTION_OK != update_reason_code)
        {
            static const network_info_str_t g_network_info_empty = {
                .ip      = { "0" },
                .gw      = { "0" },
                .netmask = { "0" },
            };
            p_network_info = &g_network_info_empty;
        }

        str_buf_printf(
            &str_buf,
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
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(g_json_ip_info_buf);
    str_buf_printf(&str_buf, "{}\n");
}
