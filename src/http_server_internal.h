/**
 * @file http_server_internal.h
 * @author TheSomeMan
 * @date 2026-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_ESP32_WIFI_MANAGER_HTTP_SERVER_INTERNAL_H
#define RUUVI_ESP32_WIFI_MANAGER_HTTP_SERVER_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

void
http_server_task_wdt_reset(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_ESP32_WIFI_MANAGER_HTTP_SERVER_INTERNAL_H
