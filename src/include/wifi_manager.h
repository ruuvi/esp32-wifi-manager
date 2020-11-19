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

@file wifi_manager.h
@author Tony Pottier
@brief Defines all functions necessary for esp32 to connect to a wifi/scan wifis

Contains the freeRTOS task and all necessary support

@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#ifndef WIFI_MANAGER_H_INCLUDED
#define WIFI_MANAGER_H_INCLUDED

#include "esp_wifi.h"
#include "wifi_manager_defs.h"
#include "http_server_resp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*wifi_manager_cb_ptr)(void *);

typedef struct
{
    wifi_ant_gpio_config_t wifi_ant_gpio_config;
    wifi_ant_config_t      wifi_ant_config;
} WiFiAntConfig_t;

/**
 * @brief Allocate heap memory for the wifi manager and start the wifi_manager RTOS task.
 */
void
wifi_manager_start(
    const WiFiAntConfig_t *        p_wifi_ant_config,
    wifi_manager_http_callback_t   cb_on_http_get,
    wifi_manager_http_cb_on_post_t cb_on_http_post,
    wifi_manager_http_callback_t   cb_on_http_delete);

/**
 * @brief Stop wifi manager and deallocate resources.
 */
void
wifi_manager_stop(void);

/**
 * @brief clears the current STA wifi config in flash ram storage.
 */
bool
wifi_manager_clear_sta_config(void);

/**
 * @brief requests a connection to an access point that will be process in the main task thread.
 */
void
wifi_manager_connect_async(void);

/**
 * @brief requests a wifi scan
 */
void
wifi_manager_scan_async(void);

/**
 * @brief requests to disconnect and forget about the access point.
 */
void
wifi_manager_disconnect_async(void);

/**
 * @brief Register a callback to a custom function when specific event message_code happens.
 */
void
wifi_manager_set_callback(message_code_e message_code, wifi_manager_cb_ptr func_ptr);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_MANAGER_H_INCLUDED */
