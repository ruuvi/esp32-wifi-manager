/**
 * @file sta_ip_unsafe.h
 * @author TheSomeMan
 * @date 2020-08-25
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_ESP32_WIFI_MANAGER_STA_IP_UNSAFE_H
#define RUUVI_ESP32_WIFI_MANAGER_STA_IP_UNSAFE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void
sta_ip_unsafe_init(void);

void
sta_ip_unsafe_deinit(void);

void
sta_ip_unsafe_set(const uint32_t ip);

void
sta_ip_unsafe_reset(void);

const char *
sta_ip_unsafe_get_str(void);

uint32_t
sta_ip_unsafe_conv_str_to_ip(const char *p_ip_addr_str);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_ESP32_WIFI_MANAGER_STA_IP_UNSAFE_H
