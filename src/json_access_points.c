#include "json_access_points.h"
#include <stdint.h>
#include <stdio.h>
#include "esp_err.h"
#include "esp_wifi_types.h"
#include "wifi_manager_defs.h"
#include "json.h"

static char g_json_access_points_buf[MAX_AP_NUM * JSON_ONE_APP_SIZE + 4]; /* 4 bytes for json encapsulation of "[\n" and
                                                                             "]\0" */

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
    int32_t buf_idx = 0;
    json_snprintf(&buf_idx, g_json_access_points_buf, sizeof(g_json_access_points_buf), "[");
    for (uint32_t i = 0; i < num_access_points; i++)
    {
        const wifi_ap_record_t ap = p_access_points[i];

        json_snprintf(&buf_idx, g_json_access_points_buf, sizeof(g_json_access_points_buf), "{\"ssid\":");
        json_print_escaped_string(
            &buf_idx,
            g_json_access_points_buf,
            sizeof(g_json_access_points_buf),
            (char *)ap.ssid);

        /* print the rest of the json for this access point: no more string to escape */
        json_snprintf(
            &buf_idx,
            g_json_access_points_buf,
            sizeof(g_json_access_points_buf),
            ",\"chan\":%d,\"rssi\":%d,\"auth\":%d}%s\n",
            ap.primary,
            ap.rssi,
            ap.authmode,
            ((i) < (num_access_points - 1)) ? "," : "");
    }
    json_snprintf(&buf_idx, g_json_access_points_buf, sizeof(g_json_access_points_buf), "]\n");
}

const char *
json_access_points_get(void)
{
    return g_json_access_points_buf;
}
