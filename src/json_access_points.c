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

@file json_access_points.c
@author Tony Pottier
@brief Defines all functions necessary for esp32 to connect to a wifi/scan wifis
@note This file was created by extracting and rewriting the functions from wifi_manager.c by @author TheSomeMan

Contains the freeRTOS task and all necessary support

@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#include "json_access_points.h"
#include <stdint.h>
#include "esp_wifi_types.h"
#include "wifi_manager_defs.h"
#include "json.h"

void
json_access_points_generate_str_buf(
    str_buf_t* const              p_str_buf,
    const wifi_ap_record_t* const p_access_points,
    const uint32_t                num_access_points)
{
    str_buf_printf(p_str_buf, "[");
    const uint32_t num_ap_checked = (num_access_points <= MAX_AP_NUM) ? num_access_points : MAX_AP_NUM;
    for (uint32_t i = 0; i < num_ap_checked; ++i)
    {
        const wifi_ap_record_t* const p_ap = &p_access_points[i];

        str_buf_printf(p_str_buf, "{\"ssid\":");
        json_print_escaped_string(p_str_buf, (const char*)p_ap->ssid);

        /* print the rest of the json for this access point: no more string to escape */
        str_buf_printf(
            p_str_buf,
            ",\"chan\":%d,\"rssi\":%d,\"auth\":%d}%s\n",
            p_ap->primary,
            p_ap->rssi,
            p_ap->authmode,
            (i < (num_ap_checked - 1)) ? "," : "");
    }
    str_buf_printf(p_str_buf, "]\n");
}

const char*
json_access_points_generate(const wifi_ap_record_t* const p_access_points, const uint32_t num_access_points)
{
    str_buf_t str_buf = STR_BUF_INIT_NULL();
    json_access_points_generate_str_buf(&str_buf, p_access_points, num_access_points);
    if (!str_buf_init_with_alloc(&str_buf))
    {
        return NULL;
    }
    json_access_points_generate_str_buf(&str_buf, p_access_points, num_access_points);

    return str_buf.buf;
}
