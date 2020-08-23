#include "gtest/gtest.h"
#include "json_access_points.h"
#include <stdio.h>
#include <cstring>
#include <memory.h>
#include <string>
#include "wifi_manager_defs.h"

using namespace std;

/*** Google-test class implementation *********************************************************************************/

class TestJsonAccessPoints : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        json_access_points_init();
    }

    void
    TearDown() override
    {
        json_access_points_deinit();
    }

public:
    TestJsonAccessPoints();

    ~TestJsonAccessPoints() override;
};

TestJsonAccessPoints::TestJsonAccessPoints()
{
}

TestJsonAccessPoints::~TestJsonAccessPoints()
{
}

/*** Unit-Tests *******************************************************************************************************/

TEST_F(TestJsonAccessPoints, test_after_init) // NOLINT
{
    const char *json_str = json_access_points_get();
    ASSERT_EQ(string("[]\n"), string(json_str));
}

TEST_F(TestJsonAccessPoints, test_after_clear) // NOLINT
{
    json_access_points_clear();
    const char *json_str = json_access_points_get();
    ASSERT_EQ(string("[]\n"), string(json_str));
}

TEST_F(TestJsonAccessPoints, test_generate_0) // NOLINT
{
    json_access_points_generate(nullptr, 0);
    const char *json_str = json_access_points_get();
    ASSERT_EQ(string("[]\n"), string(json_str));
}

TEST_F(TestJsonAccessPoints, test_generate_1) // NOLINT
{
    wifi_ap_record_t access_points[1] = {};
    {
        wifi_ap_record_t *p_ap     = &access_points[0];
        const uint8_t     bssid[6] = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16 };
        const char *      ssid     = "my_ssid123";
        memcpy(&p_ap->bssid[0], bssid, sizeof(p_ap->bssid));
        snprintf(reinterpret_cast<char *>(p_ap->ssid), sizeof(p_ap->ssid), "%s", ssid);
        p_ap->primary         = 9;                            /**< channel of AP */
        p_ap->second          = WIFI_SECOND_CHAN_NONE;        /**< secondary channel of AP */
        p_ap->rssi            = -99;                          /**< signal strength of AP */
        p_ap->authmode        = WIFI_AUTH_WPA_WPA2_PSK;       /**< authmode of AP */
        p_ap->pairwise_cipher = WIFI_CIPHER_TYPE_AES_CMAC128; /**< pairwise cipher of AP */
        p_ap->group_cipher    = WIFI_CIPHER_TYPE_TKIP_CCMP;   /**< group cipher of AP */
        p_ap->ant             = WIFI_ANT_ANT0;                /**< antenna used to receive beacon from AP */
        p_ap->phy_11b         = 0; /**< bit: 0 flag to identify if 11b mode is enabled or not */
        p_ap->phy_11g         = 0; /**< bit: 1 flag to identify if 11g mode is enabled or not */
        p_ap->phy_11n         = 0; /**< bit: 2 flag to identify if 11n mode is enabled or not */
        p_ap->phy_lr          = 0; /**< bit: 3 flag to identify if low rate is enabled or not */
        p_ap->wps             = 0; /**< bit: 4 flag to identify if WPS is supported or not */

        /**< country information of AP */
        const char country_code[3] = "EN";
        memcpy(p_ap->country.cc, country_code, sizeof(p_ap->country.cc));
        p_ap->country.schan        = 7;
        p_ap->country.nchan        = 11;
        p_ap->country.max_tx_power = 15;
        p_ap->country.policy       = WIFI_COUNTRY_POLICY_AUTO;
    }
    json_access_points_generate(access_points, sizeof(access_points) / sizeof(access_points[0]));
    const char *json_str = json_access_points_get();
    ASSERT_EQ(
        string("["
               "{\"ssid\":\"my_ssid123\",\"chan\":9,\"rssi\":-99,\"auth\":4}\n"
               "]\n"),
        string(json_str));
}

TEST_F(TestJsonAccessPoints, test_generate_2) // NOLINT
{
    wifi_ap_record_t access_points[2] = {};
    {
        wifi_ap_record_t *p_ap     = &access_points[0];
        const uint8_t     bssid[6] = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16 };
        const char *      ssid     = "my_ssid123";
        memcpy(&p_ap->bssid[0], bssid, sizeof(p_ap->bssid));
        snprintf(reinterpret_cast<char *>(p_ap->ssid), sizeof(p_ap->ssid), "%s", ssid);
        p_ap->primary         = 9;                            /**< channel of AP */
        p_ap->second          = WIFI_SECOND_CHAN_NONE;        /**< secondary channel of AP */
        p_ap->rssi            = -99;                          /**< signal strength of AP */
        p_ap->authmode        = WIFI_AUTH_WPA_WPA2_PSK;       /**< authmode of AP */
        p_ap->pairwise_cipher = WIFI_CIPHER_TYPE_AES_CMAC128; /**< pairwise cipher of AP */
        p_ap->group_cipher    = WIFI_CIPHER_TYPE_TKIP_CCMP;   /**< group cipher of AP */
        p_ap->ant             = WIFI_ANT_ANT0;                /**< antenna used to receive beacon from AP */
        p_ap->phy_11b         = 0; /**< bit: 0 flag to identify if 11b mode is enabled or not */
        p_ap->phy_11g         = 0; /**< bit: 1 flag to identify if 11g mode is enabled or not */
        p_ap->phy_11n         = 0; /**< bit: 2 flag to identify if 11n mode is enabled or not */
        p_ap->phy_lr          = 0; /**< bit: 3 flag to identify if low rate is enabled or not */
        p_ap->wps             = 0; /**< bit: 4 flag to identify if WPS is supported or not */

        /**< country information of AP */
        const char country_code[3] = "EN";
        memcpy(p_ap->country.cc, country_code, sizeof(p_ap->country.cc));
        p_ap->country.schan        = 7;
        p_ap->country.nchan        = 11;
        p_ap->country.max_tx_power = 15;
        p_ap->country.policy       = WIFI_COUNTRY_POLICY_AUTO;
    }
    {
        wifi_ap_record_t *p_ap     = &access_points[1];
        const uint8_t     bssid[6] = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16 };
        const char *      ssid     = "my_ssid456";
        memcpy(&p_ap->bssid[0], bssid, sizeof(p_ap->bssid));
        snprintf(reinterpret_cast<char *>(p_ap->ssid), sizeof(p_ap->ssid), "%s", ssid);
        p_ap->primary         = 10;                           /**< channel of AP */
        p_ap->second          = WIFI_SECOND_CHAN_NONE;        /**< secondary channel of AP */
        p_ap->rssi            = -98;                          /**< signal strength of AP */
        p_ap->authmode        = WIFI_AUTH_WPA_PSK;            /**< authmode of AP */
        p_ap->pairwise_cipher = WIFI_CIPHER_TYPE_AES_CMAC128; /**< pairwise cipher of AP */
        p_ap->group_cipher    = WIFI_CIPHER_TYPE_TKIP_CCMP;   /**< group cipher of AP */
        p_ap->ant             = WIFI_ANT_ANT0;                /**< antenna used to receive beacon from AP */
        p_ap->phy_11b         = 0; /**< bit: 0 flag to identify if 11b mode is enabled or not */
        p_ap->phy_11g         = 0; /**< bit: 1 flag to identify if 11g mode is enabled or not */
        p_ap->phy_11n         = 0; /**< bit: 2 flag to identify if 11n mode is enabled or not */
        p_ap->phy_lr          = 0; /**< bit: 3 flag to identify if low rate is enabled or not */
        p_ap->wps             = 0; /**< bit: 4 flag to identify if WPS is supported or not */

        /**< country information of AP */
        const char country_code[3] = "EN";
        memcpy(p_ap->country.cc, country_code, sizeof(p_ap->country.cc));
        p_ap->country.schan        = 7;
        p_ap->country.nchan        = 11;
        p_ap->country.max_tx_power = 15;
        p_ap->country.policy       = WIFI_COUNTRY_POLICY_AUTO;
    }
    json_access_points_generate(access_points, sizeof(access_points) / sizeof(access_points[0]));
    const char *json_str = json_access_points_get();
    ASSERT_EQ(
        string("["
               "{\"ssid\":\"my_ssid123\",\"chan\":9,\"rssi\":-99,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid456\",\"chan\":10,\"rssi\":-98,\"auth\":2}\n"
               "]\n"),
        string(json_str));
}
