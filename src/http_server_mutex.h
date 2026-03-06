/**
 * @file http_server_mutex.h
 * @author TheSomeMan
 * @date 2026-03-06
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_ESP_WIFI_MANAGER_HTTP_SERVER_MUTEX_H
#define RUUVI_ESP_WIFI_MANAGER_HTTP_SERVER_MUTEX_H

#include "os_mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

os_mutex_t
http_server_get_mutex(void);

#ifdef __cplusplus
}
#endif

#endif // ESP_WIFI_MANAGER_HTTP_SERVER_MUTEX_H
