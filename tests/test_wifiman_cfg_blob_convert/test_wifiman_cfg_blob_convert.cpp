/**
 * @file test_wifiman_cfg_blob_convert.cpp
 * @author TheSomeMan
 * @date 2022-04-24
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gtest/gtest.h"
#include <string>
#include "wifiman_cfg_blob_convert.h"
#include "wifiman_config.h"
#include "wifi_manager_defs.h"
#include "os_mutex.h"
#include "lwip/ip4_addr.h"
#include "esp_log_wrapper.hpp"

using namespace std;

class TestWifiManCfgBlobConvert;
static TestWifiManCfgBlobConvert *g_pTestClass;

/*** Google-test class implementation *********************************************************************************/

class TestWifiManCfgBlobConvert : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        esp_log_wrapper_init();
        g_pTestClass                        = this;
        const wifiman_wifi_ssid_t wifi_ssid = { "RuuviGatewayAABB" };
        wifiman_default_config_init(&wifi_ssid);
    }

    void
    TearDown() override
    {
        g_pTestClass = nullptr;
        esp_log_wrapper_deinit();
    }

public:
    TestWifiManCfgBlobConvert();

    ~TestWifiManCfgBlobConvert() override;
};

TestWifiManCfgBlobConvert::TestWifiManCfgBlobConvert()
    : Test()
{
}

TestWifiManCfgBlobConvert::~TestWifiManCfgBlobConvert() = default;

extern "C" {

const char *
os_task_get_name(void)
{
    static const char g_task_name[] = "main";
    return const_cast<char *>(g_task_name);
}

os_mutex_t
os_mutex_create_static(os_mutex_static_t *const p_mutex_static)
{
    return reinterpret_cast<os_mutex_t>(p_mutex_static);
}

void
os_mutex_delete(os_mutex_t *const ph_mutex)
{
    *ph_mutex = nullptr;
}

void
os_mutex_lock(os_mutex_t const h_mutex)
{
}

void
os_mutex_unlock(os_mutex_t const h_mutex)
{
}

uint32_t
esp_ip4addr_aton(const char *addr)
{
    return ipaddr_addr(addr);
}

void
wifi_manager_cb_save_wifi_config(const wifiman_config_t *const p_cfg)
{
}

} // extern "C"

#define TEST_CHECK_LOG_RECORD(level_, msg_) ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("wifi_manager", level_, msg_)

/*** Unit-Tests *******************************************************************************************************/

TEST_F(TestWifiManCfgBlobConvert, test_blob_empty) // NOLINT
{
    const wifiman_cfg_blob_t cfg_blob = { 0 };
    wifiman_config_t         cfg      = { 0 };
    wifiman_cfg_blob_convert(&cfg_blob, &cfg);
    ASSERT_EQ(string("RuuviGatewayAABB"), string(reinterpret_cast<const char *>(cfg.wifi_config_ap.ssid)));
    ASSERT_EQ(string(""), string(reinterpret_cast<const char *>(cfg.wifi_config_ap.password)));
    ASSERT_EQ(0, cfg.wifi_config_ap.ssid_len);
    ASSERT_EQ(0, cfg.wifi_config_ap.channel);
    ASSERT_EQ(WIFI_AUTH_OPEN, cfg.wifi_config_ap.authmode);
    ASSERT_EQ(0, cfg.wifi_config_ap.ssid_hidden);
    ASSERT_EQ(4, cfg.wifi_config_ap.max_connection);
    ASSERT_EQ(100, cfg.wifi_config_ap.beacon_interval);

    ASSERT_EQ(WIFI_BW_HT20, cfg.wifi_settings_ap.ap_bandwidth);
    ASSERT_EQ(string("10.10.0.1"), string(cfg.wifi_settings_ap.ap_ip.buf));
    ASSERT_EQ(string("10.10.0.1"), string(cfg.wifi_settings_ap.ap_gw.buf));
    ASSERT_EQ(string("255.255.255.0"), string(cfg.wifi_settings_ap.ap_netmask.buf));

    ASSERT_EQ(string(""), string(reinterpret_cast<const char *>(cfg.wifi_config_sta.ssid)));
    ASSERT_EQ(string(""), string(reinterpret_cast<const char *>(cfg.wifi_config_sta.password)));
    ASSERT_EQ(WIFI_FAST_SCAN, cfg.wifi_config_sta.scan_method);
    ASSERT_EQ(0, cfg.wifi_config_sta.bssid_set);
    ASSERT_EQ(
        vector<uint8_t>({ 0, 0, 0, 0, 0, 0 }),
        std::vector<uint8_t>(
            cfg.wifi_config_sta.bssid,
            cfg.wifi_config_sta.bssid + sizeof(cfg.wifi_config_sta.bssid) / sizeof(cfg.wifi_config_sta.bssid[0])));
    ASSERT_EQ(0, cfg.wifi_config_sta.channel);
    ASSERT_EQ(0, cfg.wifi_config_sta.listen_interval);
    ASSERT_EQ(WIFI_CONNECT_AP_BY_SIGNAL, cfg.wifi_config_sta.sort_method);
    ASSERT_EQ(0, cfg.wifi_config_sta.threshold.rssi);
    ASSERT_EQ(WIFI_AUTH_OPEN, cfg.wifi_config_sta.threshold.authmode);
    ASSERT_EQ(0, cfg.wifi_config_sta.pmf_cfg.capable);
    ASSERT_EQ(0, cfg.wifi_config_sta.pmf_cfg.required);

    ASSERT_EQ(WIFI_PS_NONE, cfg.wifi_settings_sta.sta_power_save);
    ASSERT_EQ(0, cfg.wifi_settings_sta.sta_static_ip);
    ASSERT_EQ(esp_ip4addr_aton("0.0.0.0"), cfg.wifi_settings_sta.sta_static_ip_config.ip.addr);
    ASSERT_EQ(esp_ip4addr_aton("0.0.0.0"), cfg.wifi_settings_sta.sta_static_ip_config.netmask.addr);
    ASSERT_EQ(esp_ip4addr_aton("0.0.0.0"), cfg.wifi_settings_sta.sta_static_ip_config.gw.addr);

    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "wifiman_cfg_blob_convert: Unknown ap_bandwidth=0, force set to WIFI_BW_HT20");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestWifiManCfgBlobConvert, test_blob_default) // NOLINT
{
    const wifiman_cfg_blob_t cfg_blob = {
        .sta_ssid = { "" },
        .sta_password = { "" },
        .wifi_settings = {
            .ap_ssid = { 0 },
            .ap_pwd = { 0 },
            .ap_channel = 0,
            .ap_ssid_hidden = 0,
            .ap_bandwidth = WIFI_BW_HT20,
            .sta_only = false,
            .sta_power_save = WIFI_PS_NONE,
            .sta_static_ip = false,
            .sta_static_ip_config = {
                .ip = { esp_ip4addr_aton("0.0.0.0"), },
                .netmask = { esp_ip4addr_aton("0.0.0.0"), },
                .gw = { esp_ip4addr_aton("0.0.0.0"), },
            },
        },
    };
    wifiman_config_t cfg = { 0 };
    wifiman_cfg_blob_convert(&cfg_blob, &cfg);
    ASSERT_EQ(string("RuuviGatewayAABB"), string(reinterpret_cast<const char *>(cfg.wifi_config_ap.ssid)));
    ASSERT_EQ(string(""), string(reinterpret_cast<const char *>(cfg.wifi_config_ap.password)));
    ASSERT_EQ(0, cfg.wifi_config_ap.ssid_len);
    ASSERT_EQ(0, cfg.wifi_config_ap.channel);
    ASSERT_EQ(WIFI_AUTH_OPEN, cfg.wifi_config_ap.authmode);
    ASSERT_EQ(0, cfg.wifi_config_ap.ssid_hidden);
    ASSERT_EQ(4, cfg.wifi_config_ap.max_connection);
    ASSERT_EQ(100, cfg.wifi_config_ap.beacon_interval);

    ASSERT_EQ(WIFI_BW_HT20, cfg.wifi_settings_ap.ap_bandwidth);
    ASSERT_EQ(string("10.10.0.1"), string(cfg.wifi_settings_ap.ap_ip.buf));
    ASSERT_EQ(string("10.10.0.1"), string(cfg.wifi_settings_ap.ap_gw.buf));
    ASSERT_EQ(string("255.255.255.0"), string(cfg.wifi_settings_ap.ap_netmask.buf));

    ASSERT_EQ(string(""), string(reinterpret_cast<const char *>(cfg.wifi_config_sta.ssid)));
    ASSERT_EQ(string(""), string(reinterpret_cast<const char *>(cfg.wifi_config_sta.password)));
    ASSERT_EQ(WIFI_FAST_SCAN, cfg.wifi_config_sta.scan_method);
    ASSERT_EQ(0, cfg.wifi_config_sta.bssid_set);
    ASSERT_EQ(
        vector<uint8_t>({ 0, 0, 0, 0, 0, 0 }),
        std::vector<uint8_t>(
            cfg.wifi_config_sta.bssid,
            cfg.wifi_config_sta.bssid + sizeof(cfg.wifi_config_sta.bssid) / sizeof(cfg.wifi_config_sta.bssid[0])));
    ASSERT_EQ(0, cfg.wifi_config_sta.channel);
    ASSERT_EQ(0, cfg.wifi_config_sta.listen_interval);
    ASSERT_EQ(WIFI_CONNECT_AP_BY_SIGNAL, cfg.wifi_config_sta.sort_method);
    ASSERT_EQ(0, cfg.wifi_config_sta.threshold.rssi);
    ASSERT_EQ(WIFI_AUTH_OPEN, cfg.wifi_config_sta.threshold.authmode);
    ASSERT_EQ(0, cfg.wifi_config_sta.pmf_cfg.capable);
    ASSERT_EQ(0, cfg.wifi_config_sta.pmf_cfg.required);

    ASSERT_EQ(WIFI_PS_NONE, cfg.wifi_settings_sta.sta_power_save);
    ASSERT_EQ(0, cfg.wifi_settings_sta.sta_static_ip);
    ASSERT_EQ(esp_ip4addr_aton("0.0.0.0"), cfg.wifi_settings_sta.sta_static_ip_config.ip.addr);
    ASSERT_EQ(esp_ip4addr_aton("0.0.0.0"), cfg.wifi_settings_sta.sta_static_ip_config.netmask.addr);
    ASSERT_EQ(esp_ip4addr_aton("0.0.0.0"), cfg.wifi_settings_sta.sta_static_ip_config.gw.addr);
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestWifiManCfgBlobConvert, test_blob_non_default) // NOLINT
{
    const wifiman_cfg_blob_t cfg_blob = {
        .sta_ssid = { "sta_ssid1" },
        .sta_password = { "sta_pass1" },
        .wifi_settings = {
            .ap_ssid = { 'a', 'p', '_', 's', 's', 'i', 'd', '1', '\0' },
            .ap_pwd = { 'a', 'p', '_', 'p', 'w', 'd', '1', '\0' },
            .ap_channel = 14,
            .ap_ssid_hidden = 1,
            .ap_bandwidth = WIFI_BW_HT40,
            .sta_only = true,
            .sta_power_save = WIFI_PS_MAX_MODEM,
            .sta_static_ip = true,
            .sta_static_ip_config = {
                .ip = { esp_ip4addr_aton("192.168.1.22"), },
                .netmask = { esp_ip4addr_aton("255.255.255.0"), },
                .gw = { esp_ip4addr_aton("192.168.1.1"), },
            },
        },
    };
    wifiman_config_t cfg = { 0 };
    wifiman_cfg_blob_convert(&cfg_blob, &cfg);
    ASSERT_EQ(string("RuuviGatewayAABB"), string(reinterpret_cast<const char *>(cfg.wifi_config_ap.ssid)));
    ASSERT_EQ(string(""), string(reinterpret_cast<const char *>(cfg.wifi_config_ap.password)));
    ASSERT_EQ(0, cfg.wifi_config_ap.ssid_len);
    ASSERT_EQ(14, cfg.wifi_config_ap.channel);
    ASSERT_EQ(WIFI_AUTH_OPEN, cfg.wifi_config_ap.authmode);
    ASSERT_EQ(1, cfg.wifi_config_ap.ssid_hidden);
    ASSERT_EQ(4, cfg.wifi_config_ap.max_connection);
    ASSERT_EQ(100, cfg.wifi_config_ap.beacon_interval);

    ASSERT_EQ(WIFI_BW_HT40, cfg.wifi_settings_ap.ap_bandwidth);
    ASSERT_EQ(string("10.10.0.1"), string(cfg.wifi_settings_ap.ap_ip.buf));
    ASSERT_EQ(string("10.10.0.1"), string(cfg.wifi_settings_ap.ap_gw.buf));
    ASSERT_EQ(string("255.255.255.0"), string(cfg.wifi_settings_ap.ap_netmask.buf));

    ASSERT_EQ(string("sta_ssid1"), string(reinterpret_cast<const char *>(cfg.wifi_config_sta.ssid)));
    ASSERT_EQ(string("sta_pass1"), string(reinterpret_cast<const char *>(cfg.wifi_config_sta.password)));
    ASSERT_EQ(WIFI_FAST_SCAN, cfg.wifi_config_sta.scan_method);
    ASSERT_EQ(0, cfg.wifi_config_sta.bssid_set);
    ASSERT_EQ(
        vector<uint8_t>({ 0, 0, 0, 0, 0, 0 }),
        std::vector<uint8_t>(
            cfg.wifi_config_sta.bssid,
            cfg.wifi_config_sta.bssid + sizeof(cfg.wifi_config_sta.bssid) / sizeof(cfg.wifi_config_sta.bssid[0])));
    ASSERT_EQ(0, cfg.wifi_config_sta.channel);
    ASSERT_EQ(0, cfg.wifi_config_sta.listen_interval);
    ASSERT_EQ(WIFI_CONNECT_AP_BY_SIGNAL, cfg.wifi_config_sta.sort_method);
    ASSERT_EQ(0, cfg.wifi_config_sta.threshold.rssi);
    ASSERT_EQ(WIFI_AUTH_OPEN, cfg.wifi_config_sta.threshold.authmode);
    ASSERT_EQ(0, cfg.wifi_config_sta.pmf_cfg.capable);
    ASSERT_EQ(0, cfg.wifi_config_sta.pmf_cfg.required);

    ASSERT_EQ(WIFI_PS_MAX_MODEM, cfg.wifi_settings_sta.sta_power_save);
    ASSERT_EQ(true, cfg.wifi_settings_sta.sta_static_ip);
    ASSERT_EQ(esp_ip4addr_aton("192.168.1.22"), cfg.wifi_settings_sta.sta_static_ip_config.ip.addr);
    ASSERT_EQ(esp_ip4addr_aton("255.255.255.0"), cfg.wifi_settings_sta.sta_static_ip_config.netmask.addr);
    ASSERT_EQ(esp_ip4addr_aton("192.168.1.1"), cfg.wifi_settings_sta.sta_static_ip_config.gw.addr);
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestWifiManCfgBlobConvert, test_blob_invalid) // NOLINT
{
    const wifiman_cfg_blob_t cfg_blob = {
        .sta_ssid = { "sta_ssid1" },
        .sta_password = { "sta_pass1" },
        .wifi_settings = {
            .ap_ssid = { 'a', 'p', '_', 's', 's', 'i', 'd', '1', '\0' },
            .ap_pwd = { 'a', 'p', '_', 'p', 'w', 'd', '1', '\0' },
            .ap_channel = 15,
            .ap_ssid_hidden = 2,
            .ap_bandwidth = (wifi_bandwidth_t)((int)WIFI_BW_HT40 + 1),
            .sta_only = (bool)2,
            .sta_power_save = (wifi_ps_type_t)((int)WIFI_PS_MAX_MODEM + 1),
            .sta_static_ip = (bool)2,
            .sta_static_ip_config = {
                .ip = { esp_ip4addr_aton("192.168.1.22"), },
                .netmask = { esp_ip4addr_aton("255.255.255.0"), },
                .gw = { esp_ip4addr_aton("192.168.1.1"), },
            },
        },
    };
    wifiman_config_t cfg = { 0 };
    wifiman_cfg_blob_convert(&cfg_blob, &cfg);
    ASSERT_EQ(string("RuuviGatewayAABB"), string(reinterpret_cast<const char *>(cfg.wifi_config_ap.ssid)));
    ASSERT_EQ(string(""), string(reinterpret_cast<const char *>(cfg.wifi_config_ap.password)));
    ASSERT_EQ(0, cfg.wifi_config_ap.ssid_len);
    ASSERT_EQ(0, cfg.wifi_config_ap.channel); // force set to 0
    ASSERT_EQ(WIFI_AUTH_OPEN, cfg.wifi_config_ap.authmode);
    ASSERT_EQ(1, cfg.wifi_config_ap.ssid_hidden);
    ASSERT_EQ(4, cfg.wifi_config_ap.max_connection);
    ASSERT_EQ(100, cfg.wifi_config_ap.beacon_interval);

    ASSERT_EQ(WIFI_BW_HT20, cfg.wifi_settings_ap.ap_bandwidth); // force set to WIFI_BW_HT20
    ASSERT_EQ(string("10.10.0.1"), string(cfg.wifi_settings_ap.ap_ip.buf));
    ASSERT_EQ(string("10.10.0.1"), string(cfg.wifi_settings_ap.ap_gw.buf));
    ASSERT_EQ(string("255.255.255.0"), string(cfg.wifi_settings_ap.ap_netmask.buf));

    ASSERT_EQ(string("sta_ssid1"), string(reinterpret_cast<const char *>(cfg.wifi_config_sta.ssid)));
    ASSERT_EQ(string("sta_pass1"), string(reinterpret_cast<const char *>(cfg.wifi_config_sta.password)));
    ASSERT_EQ(WIFI_FAST_SCAN, cfg.wifi_config_sta.scan_method);
    ASSERT_EQ(0, cfg.wifi_config_sta.bssid_set);
    ASSERT_EQ(
        vector<uint8_t>({ 0, 0, 0, 0, 0, 0 }),
        std::vector<uint8_t>(
            cfg.wifi_config_sta.bssid,
            cfg.wifi_config_sta.bssid + sizeof(cfg.wifi_config_sta.bssid) / sizeof(cfg.wifi_config_sta.bssid[0])));
    ASSERT_EQ(0, cfg.wifi_config_sta.channel);
    ASSERT_EQ(0, cfg.wifi_config_sta.listen_interval);
    ASSERT_EQ(WIFI_CONNECT_AP_BY_SIGNAL, cfg.wifi_config_sta.sort_method);
    ASSERT_EQ(0, cfg.wifi_config_sta.threshold.rssi);
    ASSERT_EQ(WIFI_AUTH_OPEN, cfg.wifi_config_sta.threshold.authmode);
    ASSERT_EQ(0, cfg.wifi_config_sta.pmf_cfg.capable);
    ASSERT_EQ(0, cfg.wifi_config_sta.pmf_cfg.required);

    ASSERT_EQ(WIFI_PS_NONE, cfg.wifi_settings_sta.sta_power_save); // force set to WIFI_PS_MAX_NONE
    ASSERT_EQ(true, cfg.wifi_settings_sta.sta_static_ip);
    ASSERT_EQ(esp_ip4addr_aton("192.168.1.22"), cfg.wifi_settings_sta.sta_static_ip_config.ip.addr);
    ASSERT_EQ(esp_ip4addr_aton("255.255.255.0"), cfg.wifi_settings_sta.sta_static_ip_config.netmask.addr);
    ASSERT_EQ(esp_ip4addr_aton("192.168.1.1"), cfg.wifi_settings_sta.sta_static_ip_config.gw.addr);

    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "wifiman_cfg_blob_convert: Unknown ap_channel=15, force set to 0");
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "wifiman_cfg_blob_convert: Unknown ap_bandwidth=3, force set to WIFI_BW_HT20");
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_WARN,
        "wifiman_cfg_blob_convert: Unknown sta_power_save=3, force set to WIFI_PS_NONE");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}
