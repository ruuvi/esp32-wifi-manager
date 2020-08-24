#ifndef ESP32_WIFI_MANAGER_JSON_NETWORK_INFO_H
#define ESP32_WIFI_MANAGER_JSON_NETWORK_INFO_H

#include <stdint.h>
#include <stdbool.h>
#include "wifi_manager_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

void
json_network_info_init(void);

void
json_network_info_deinit(void);

const char *
json_network_info_get(void);

/**
 * @brief Generates the connection status json: ssid and IP addresses.
 * @note This is not thread-safe and should be called only if wifi_manager_lock_json_buffer call is successful.
 */
void
json_network_info_generate(
    const char *              ssid,
    const network_info_str_t *p_network_info,
    update_reason_code_t      update_reason_code);

/**
 * @brief Clears the connection status json.
 * @note This is not thread-safe and should be called only if wifi_manager_lock_json_buffer call is successful.
 */
void
json_network_info_clear(void);

#ifdef __cplusplus
}
#endif

#endif // ESP32_WIFI_MANAGER_JSON_NETWORK_INFO_H
