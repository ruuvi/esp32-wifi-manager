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

@file main.c
@author Tony Pottier
@brief Entry point for the ESP32 application.
@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "wifi_manager.h"
#include "esp_wifi.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"

/* @brief tag used for ESP serial console messages */
static const char TAG[] = "main";

/**
 * @brief RTOS task that periodically prints the heap memory available.
 * @note Pure debug information, should not be ever started on production code! This is an example on how you can
 * integrate your code with wifi-manager
 */
void
monitoring_task(void* pvParameter)
{
    for (;;)
    {
        ESP_LOGI(TAG, "free heap: %d", esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

/* brief this is an exemple of a callback that you can setup in your own app to get notified of wifi manager event */
void
cb_connection_ok(void* pvParameter)
{
    ESP_LOGI(TAG, "I have a connection!");
}

static mbedtls_entropy_context  g_entropy;
static mbedtls_ctr_drbg_context g_ctr_drbg;

static void
configure_mbedtls_rng(void)
{
    mbedtls_entropy_init(&g_entropy);
    mbedtls_ctr_drbg_init(&g_ctr_drbg);

    uint8_t mac[6];
    esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);

    str_buf_t custom_seed = str_buf_printf_with_alloc(
        "%02X:%02X:%02X:%02X:%02X:%02X:%lu:%lu",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5],
        (printf_ulong_t)time(NULL),
        (printf_ulong_t)xTaskGetTickCount());

    if (0
        != mbedtls_ctr_drbg_seed(
            &g_ctr_drbg,
            mbedtls_entropy_func,
            &g_entropy,
            (const unsigned char*)custom_seed.buf,
            strlen(custom_seed.buf)))
    {
        // Handle error
    }
}

void
app_main()
{
    configure_mbedtls_rng();
    /* start the wifi manager */
    static const wifi_manager_callbacks_t g_wifi_callbacks = {
        .cb_on_http_user_req       = NULL,
        .cb_on_http_get            = NULL,
        .cb_on_http_post           = NULL,
        .cb_on_http_delete         = NULL,
        .cb_on_connect_eth_cmd     = NULL,
        .cb_on_disconnect_eth_cmd  = NULL,
        .cb_on_disconnect_sta_cmd  = NULL,
        .cb_on_ap_started          = NULL,
        .cb_on_ap_stopped          = NULL,
        .cb_on_ap_sta_connected    = NULL,
        .cb_on_ap_sta_ip_assigned  = NULL,
        .cb_on_ap_sta_disconnected = NULL,
        .cb_save_wifi_config_sta   = NULL,
        .cb_on_request_status_json = NULL,
    };
    wifiman_config_t wifi_cfg = {

    };
    const wifi_manager_antenna_config_t wifi_antenna_config = {

    };

    const bool flag_connect_sta = false;
    wifi_manager_start(
        flag_connect_sta,
        &wifi_cfg,
        &wifi_antenna_config,
        &g_wifi_callbacks,
        &mbedtls_ctr_drbg_random,
        &g_ctr_drbg);

    /* register a callback as an example to how you can integrate your code with the wifi manager */
    wifi_manager_set_callback(EVENT_STA_GOT_IP, &cb_connection_ok);

    /* your code should go here. Here we simply create a task on core 2 that monitors free heap memory */
    xTaskCreatePinnedToCore(&monitoring_task, "monitoring_task", 2048, NULL, 1, NULL, 1);
}
