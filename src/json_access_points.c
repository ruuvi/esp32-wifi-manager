#include "json_access_points.h"
#include <stdint.h>
#include <stdio.h>
#include "esp_err.h"
#include "esp_wifi_types.h"
#include "wifi_manager_defs.h"
#include "json.h"

static char g_json_access_points_buf[JSON_ACCESS_POINT_BUF_SIZE];

esp_err_t
json_access_points_init(void)
{
    json_access_points_clear();
    return ESP_OK;
}

void
json_access_points_deinit(void)
{
}

void
json_access_points_clear(void)
{
    snprintf(g_json_access_points_buf, sizeof(g_json_access_points_buf), "[]\n");
}

void
json_access_points_generate(const wifi_ap_record_t *p_access_points, const uint32_t num_access_points)
{
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(g_json_access_points_buf);
    str_buf_printf(&str_buf, "[");
    const uint32_t num_ap_checked = (num_access_points <= MAX_AP_NUM) ? num_access_points : MAX_AP_NUM;
    for (uint32_t i = 0; i < num_ap_checked; i++)
    {
        const wifi_ap_record_t ap = p_access_points[i];

        str_buf_printf(&str_buf, "{\"ssid\":");
        json_print_escaped_string(&str_buf, (char *)ap.ssid);

        /* print the rest of the json for this access point: no more string to escape */
        str_buf_printf(
            &str_buf,
            ",\"chan\":%d,\"rssi\":%d,\"auth\":%d}%s\n",
            ap.primary,
            ap.rssi,
            ap.authmode,
            ((i) < (num_ap_checked - 1)) ? "," : "");
    }
    str_buf_printf(&str_buf, "]\n");
}

const char *
json_access_points_get(void)
{
    return g_json_access_points_buf;
}
