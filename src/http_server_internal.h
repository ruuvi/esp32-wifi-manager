/**
 * @file http_server_internal.h
 * @author TheSomeMan
 * @date 2026-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_ESP32_WIFI_MANAGER_HTTP_SERVER_INTERNAL_H
#define RUUVI_ESP32_WIFI_MANAGER_HTTP_SERVER_INTERNAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP_SERVER_MAX_REQUEST_SIZE             (4U * 1024U)
#define HTTP_SERVER_MAX_UNENCRYPTED_CONTENT_SIZE (8U * 1024U)
#define HTTP_SERVER_MAX_ENCRYPTED_CONTENT_SIZE   ((((HTTP_SERVER_MAX_UNENCRYPTED_CONTENT_SIZE)*4) / 3) + 512)

#define HTTP_SERVER_BRUTE_FORCE_PROTECTION_TIMEOUT_MS (1U * 1000U)

void
http_server_task_wdt_reset(void);

uint32_t
http_server_get_task_wdog_feed_period_ms(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_ESP32_WIFI_MANAGER_HTTP_SERVER_INTERNAL_H
