#ifndef ESP32_WIFI_MANAGER_JSON_ACCESS_POINTS_H
#define ESP32_WIFI_MANAGER_JSON_ACCESS_POINTS_H

#include <stdint.h>
#include "esp_err.h"
#include "esp_wifi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t
json_access_points_init(void);

void
json_access_points_deinit(void);

/**
 * @brief Clear the list of access points.
 * @note This is not thread-safe and should be called only if wifi_manager_lock_json_buffer call is successful.
 */
void
json_access_points_clear(void);

/**
 * @brief Generates the list of access points after a wifi scan.
 * @note This is not thread-safe and should be called only if wifi_manager_lock_json_buffer call is successful.
 */
void
json_access_points_generate(const wifi_ap_record_t *p_access_points, const uint32_t num_access_points);

const char *
json_access_points_get(void);

#ifdef __cplusplus
}
#endif

#endif // ESP32_WIFI_MANAGER_JSON_ACCESS_POINTS_H
