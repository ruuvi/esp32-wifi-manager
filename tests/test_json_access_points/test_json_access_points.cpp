/**
 * @file json_test_access_points.cpp
 * @author TheSomeMan
 * @date 2020-08-23
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gtest/gtest.h"
#include "json_access_points.h"
#include <cstdio>
#include <cstring>
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

TestJsonAccessPoints::TestJsonAccessPoints() = default;

TestJsonAccessPoints::~TestJsonAccessPoints() = default;

/*** Unit-Tests *******************************************************************************************************/

TEST_F(TestJsonAccessPoints, test_generate_1) // NOLINT
{
    wifi_ap_record_t access_points[1]  = {};
    const size_t     num_access_points = sizeof(access_points) / sizeof(access_points[0]);
    {
        wifi_ap_record_t* p_ap     = &access_points[0];
        const uint8_t     bssid[6] = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16 };
        const char*       ssid     = "my_ssid123";
        memcpy(&p_ap->bssid[0], bssid, sizeof(p_ap->bssid));
        snprintf(reinterpret_cast<char*>(p_ap->ssid), sizeof(p_ap->ssid), "%s", ssid);
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
    const string json_str(json_access_points_generate(access_points, num_access_points));
    ASSERT_EQ(
        string("["
               "{\"ssid\":\"my_ssid123\",\"chan\":9,\"rssi\":-99,\"auth\":4}\n"
               "]\n"),
        json_str);
}

TEST_F(TestJsonAccessPoints, test_generate_2) // NOLINT
{
    wifi_ap_record_t access_points[2]  = {};
    const size_t     num_access_points = sizeof(access_points) / sizeof(access_points[0]);
    {
        wifi_ap_record_t* p_ap     = &access_points[0];
        const uint8_t     bssid[6] = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16 };
        const char*       ssid     = "my_ssid123";
        memcpy(&p_ap->bssid[0], bssid, sizeof(p_ap->bssid));
        snprintf(reinterpret_cast<char*>(p_ap->ssid), sizeof(p_ap->ssid), "%s", ssid);
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
        wifi_ap_record_t* p_ap     = &access_points[1];
        const uint8_t     bssid[6] = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16 };
        const char*       ssid     = "my_ssid456";
        memcpy(&p_ap->bssid[0], bssid, sizeof(p_ap->bssid));
        snprintf(reinterpret_cast<char*>(p_ap->ssid), sizeof(p_ap->ssid), "%s", ssid);
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
    const string json_str(json_access_points_generate(access_points, num_access_points));
    ASSERT_EQ(
        string("["
               "{\"ssid\":\"my_ssid123\",\"chan\":9,\"rssi\":-99,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid456\",\"chan\":10,\"rssi\":-98,\"auth\":2}\n"
               "]\n"),
        json_str);
}

TEST_F(TestJsonAccessPoints, test_generate_max_access_point_len_1) // NOLINT
{
    wifi_ap_record_t access_points[1]  = {};
    const size_t     num_access_points = sizeof(access_points) / sizeof(access_points[0]);
    {
        wifi_ap_record_t* p_ap     = &access_points[0];
        const uint8_t     bssid[6] = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16 };
        memcpy(&p_ap->bssid[0], bssid, sizeof(p_ap->bssid));

        const char* ssid = "abcdefghijklmnopqrstuvwxyz012345";
        memcpy(&p_ap->bssid[0], bssid, sizeof(p_ap->bssid));
        snprintf(reinterpret_cast<char*>(p_ap->ssid), sizeof(p_ap->ssid), "%s", ssid);

        p_ap->primary         = 11;                           /**< channel of AP */
        p_ap->second          = WIFI_SECOND_CHAN_NONE;        /**< secondary channel of AP */
        p_ap->rssi            = -100;                         /**< signal strength of AP */
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
        p_ap->country.schan        = 10;
        p_ap->country.nchan        = 11;
        p_ap->country.max_tx_power = 15;
        p_ap->country.policy       = WIFI_COUNTRY_POLICY_AUTO;
    }
    const string json_str(json_access_points_generate(access_points, num_access_points));
    ASSERT_EQ(75 + 3, json_str.length());
    ASSERT_EQ(
        string("["
               "{\"ssid\":\""
               "abcdefghijklmnopqrstuvwxyz012345"
               "\",\"chan\":11,\"rssi\":-100,\"auth\":4}\n"
               "]\n"),
        json_str);
}

TEST_F(TestJsonAccessPoints, test_generate_max_access_point_len_1_escaped) // NOLINT
{
    wifi_ap_record_t access_points[1]  = {};
    const size_t     num_access_points = sizeof(access_points) / sizeof(access_points[0]);
    {
        wifi_ap_record_t* p_ap     = &access_points[0];
        const uint8_t     bssid[6] = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16 };
        memcpy(&p_ap->bssid[0], bssid, sizeof(p_ap->bssid));

        // fill ssid with a character that needs to be escaped
        memset(p_ap->ssid, '"', sizeof(p_ap->ssid));
        p_ap->ssid[sizeof(p_ap->ssid) - 1] = '\0';

        p_ap->primary         = 11;                           /**< channel of AP */
        p_ap->second          = WIFI_SECOND_CHAN_NONE;        /**< secondary channel of AP */
        p_ap->rssi            = -100;                         /**< signal strength of AP */
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
        p_ap->country.schan        = 10;
        p_ap->country.nchan        = 11;
        p_ap->country.max_tx_power = 15;
        p_ap->country.policy       = WIFI_COUNTRY_POLICY_AUTO;
    }
    const string json_str(json_access_points_generate(access_points, num_access_points));
    ASSERT_EQ(75 + 32 + 3, json_str.length());
    ASSERT_EQ(
        string("["
               "{\"ssid\":\""
               "\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\""
               "\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\""
               "\",\"chan\":11,\"rssi\":-100,\"auth\":4}\n"
               "]\n"),
        json_str);
}

TEST_F(TestJsonAccessPoints, test_generate_max_access_point_len_2) // NOLINT
{
    wifi_ap_record_t access_points[2]  = {};
    const size_t     num_access_points = sizeof(access_points) / sizeof(access_points[0]);
    {
        wifi_ap_record_t* p_ap     = &access_points[0];
        const uint8_t     bssid[6] = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16 };
        memcpy(&p_ap->bssid[0], bssid, sizeof(p_ap->bssid));

        // fill ssid with a character that needs to be escaped
        memset(p_ap->ssid, '"', sizeof(p_ap->ssid));
        p_ap->ssid[sizeof(p_ap->ssid) - 1] = '\0';

        p_ap->primary         = 11;                           /**< channel of AP */
        p_ap->second          = WIFI_SECOND_CHAN_NONE;        /**< secondary channel of AP */
        p_ap->rssi            = -100;                         /**< signal strength of AP */
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
        p_ap->country.schan        = 10;
        p_ap->country.nchan        = 11;
        p_ap->country.max_tx_power = 15;
        p_ap->country.policy       = WIFI_COUNTRY_POLICY_AUTO;
    }
    for (int i = 1; i < num_access_points; ++i)
    {
        access_points[i] = access_points[0];
    }
    const string json_str(json_access_points_generate(access_points, num_access_points));
    ASSERT_EQ((75 + 32) * num_access_points + 1 * (num_access_points - 1) + 3, json_str.length());
    ASSERT_EQ(
        string("["
               "{\"ssid\":\""
               "\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\""
               "\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\""
               "\",\"chan\":11,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\""
               "\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\""
               "\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\"\\\""
               "\",\"chan\":11,\"rssi\":-100,\"auth\":4}\n"
               "]\n"),
        json_str);
}

TEST_F(TestJsonAccessPoints, test_generate_max_num_access_points) // NOLINT
{
    wifi_ap_record_t access_points[MAX_AP_NUM] = {};
    const size_t     num_access_points         = sizeof(access_points) / sizeof(access_points[0]);
    {
        wifi_ap_record_t* p_ap     = &access_points[0];
        const uint8_t     bssid[6] = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16 };
        memcpy(&p_ap->bssid[0], bssid, sizeof(p_ap->bssid));

        // fill ssid with a character that needs to be escaped
        memset(p_ap->ssid, '"', sizeof(p_ap->ssid));
        p_ap->ssid[sizeof(p_ap->ssid) - 1] = '\0';

        p_ap->primary         = 19;                           /**< channel of AP */
        p_ap->second          = WIFI_SECOND_CHAN_NONE;        /**< secondary channel of AP */
        p_ap->rssi            = -100;                         /**< signal strength of AP */
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
    for (int i = 0; i < num_access_points; ++i)
    {
        if (i > 0)
        {
            access_points[i] = access_points[0];
        }
        (void)snprintf(
            reinterpret_cast<char*>(access_points[i].ssid),
            sizeof(access_points[i].ssid),
            "my_ssid_%02d",
            i + 1);
    }
    const string json_str(json_access_points_generate(access_points, num_access_points));
    ASSERT_EQ(
        string("["
               "{\"ssid\":\"my_ssid_01\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_02\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_03\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_04\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_05\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_06\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_07\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_08\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_09\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_10\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_11\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_12\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_13\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_14\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_15\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_16\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_17\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_18\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_19\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_20\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_21\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_22\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_23\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_24\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_25\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_26\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_27\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_28\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_29\",\"chan\":19,\"rssi\":-100,\"auth\":4},\n"
               "{\"ssid\":\"my_ssid_30\",\"chan\":19,\"rssi\":-100,\"auth\":4}\n"
               "]\n"),
        json_str);
}
