/**
 * @file test_http_server_handle_req_get_auth.cpp
 * @author TheSomeMan
 * @date 2021-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gtest/gtest.h"
#include "http_server_handle_req_get_auth.h"
#include "http_server_handle_req_post_auth.h"
#include "http_server_handle_req_delete_auth.h"
#include <string>
#include "mbedtls/base64.h"
#include "wifiman_sha256.h"
#include "wifiman_md5.h"

#define PASSWORD_BUF_SIZE (128U)

using namespace std;

/*** Google-test class implementation *********************************************************************************/

class TestHttpServerHandleReqGetAuth : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        this->m_idx_random_value = 0;
        std::fill(arr_of_random_values.begin(), arr_of_random_values.end(), 0);
        set_random_values(this->arr_of_random_values.data(), this->arr_of_random_values.size());
        http_server_auth_clear_authorized_sessions();
    }

    void
    TearDown() override
    {
        this->m_p_random_values   = nullptr;
        this->m_num_random_values = 0;
    }

public:
    const uint32_t*           m_p_random_values;
    size_t                    m_num_random_values;
    size_t                    m_idx_random_value;
    std::array<uint32_t, 100> arr_of_random_values;

    TestHttpServerHandleReqGetAuth();

    ~TestHttpServerHandleReqGetAuth() override;

    void
    set_random_values(const uint32_t* const p_random_values, const size_t num_random_values)
    {
        this->m_p_random_values   = p_random_values;
        this->m_num_random_values = num_random_values;
        this->m_idx_random_value  = 0;
    }
};

static TestHttpServerHandleReqGetAuth* g_pTestObj;

TestHttpServerHandleReqGetAuth::TestHttpServerHandleReqGetAuth()
    : Test()
    , m_p_random_values(nullptr)
    , m_num_random_values(0)
    , m_idx_random_value(0)
{
    g_pTestObj = this;
}

TestHttpServerHandleReqGetAuth::~TestHttpServerHandleReqGetAuth()
{
    g_pTestObj = nullptr;
}

#ifdef __cplusplus
extern "C" {
#endif

uint32_t
esp_random(void)
{
    assert(nullptr != g_pTestObj->m_p_random_values);
    assert(g_pTestObj->m_idx_random_value < g_pTestObj->m_num_random_values);
    return g_pTestObj->m_p_random_values[g_pTestObj->m_idx_random_value++];
}

#ifdef __cplusplus
}
#endif

/*** Unit-Tests *******************************************************************************************************/

TEST_F(TestHttpServerHandleReqGetAuth, test_req_get_auth_allow) // NOLINT
{
    {
        const http_server_auth_info_t auth_info = {
            HTTP_SERVER_AUTH_TYPE_ALLOW,
            "",
            "",
        };
        const wifiman_hostinfo_t hostinfo = {
            .hostname     = { "RuuviGatewayEEFF" },
            .fw_ver       = { "1.13.0" },
            .nrf52_fw_ver = { "1.0.0" },
        };

        const http_req_header_t    http_header         = { "" };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };

        const http_server_handle_req_auth_param_t param = {
            .flag_access_from_lan                   = true,
            .flag_check_rw_access_with_bearer_token = false,
            .http_header                            = http_header,
            .p_remote_ip                            = &remote_ip,
            .p_auth_info                            = &auth_info,
            .p_hostinfo                             = &hostinfo,
        };

        const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
        const string             exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_allow", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"66687aadf862bd776c8fc18b8e9f8e20089714856ee233b3902a591d0d5f2925\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"AAAAAAAAAAAAAAAA\"\r\n"
                   "Set-Cookie: RUUVISESSION=AAAAAAAAAAAAAAAA\r\n"),
            string(extra_header_fields.buf));
    }
    {
        const http_server_auth_info_t auth_info = {
            HTTP_SERVER_AUTH_TYPE_ALLOW,
            "",
            "",
        };
        const wifiman_hostinfo_t hostinfo = {
            .hostname     = { "RuuviGatewayEEFF" },
            .fw_ver       = { "1.13.0" },
            .nrf52_fw_ver = { "1.0.0" },
        };

        const http_req_header_t    http_header         = { "Cookie: RUUVISESSION=QQQQQQQQQQQQQQQQ\r\n" };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };

        const http_server_handle_req_auth_param_t param = {
            .flag_access_from_lan                   = true,
            .flag_check_rw_access_with_bearer_token = false,
            .http_header                            = http_header,
            .p_remote_ip                            = &remote_ip,
            .p_auth_info                            = &auth_info,
            .p_hostinfo                             = &hostinfo,
        };

        bool flag_access_by_bearer_token = false;

        const http_server_resp_t resp = http_server_handle_req_check_auth(
            &param,
            &extra_header_fields,
            &flag_access_by_bearer_token);
        const string exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_allow", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(string(""), string(extra_header_fields.buf));
    }
    {
        const http_server_auth_info_t auth_info = {
            HTTP_SERVER_AUTH_TYPE_ALLOW,
            "",
            "",
        };
        const wifiman_hostinfo_t hostinfo = {
            .hostname     = { "RuuviGatewayEEFF" },
            .fw_ver       = { "1.13.0" },
            .nrf52_fw_ver = { "1.0.0" },
        };

        const http_req_header_t    http_header         = { "Cookie: RUUVISESSION=AAAAAAAAAAAAAAAA\r\n" };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };

        const http_server_handle_req_auth_param_t param = {
            .flag_access_from_lan                   = true,
            .flag_check_rw_access_with_bearer_token = false,
            .http_header                            = http_header,
            .p_remote_ip                            = &remote_ip,
            .p_auth_info                            = &auth_info,
            .p_hostinfo                             = &hostinfo,
        };

        bool                     flag_access_by_bearer_token = false;
        const http_server_resp_t resp                        = http_server_handle_req_check_auth(
            &param,
            &extra_header_fields,
            &flag_access_by_bearer_token);
        const string exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_allow", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(string(""), string(extra_header_fields.buf));
    }
    {
        const http_server_auth_info_t auth_info = {
            HTTP_SERVER_AUTH_TYPE_ALLOW,
            "",
            "",
        };
        const wifiman_hostinfo_t hostinfo = {
            .hostname     = { "RuuviGatewayEEFF" },
            .fw_ver       = { "1.13.0" },
            .nrf52_fw_ver = { "1.0.0" },
        };

        const http_req_header_t    http_header         = { "Cookie: RUUVISESSION=AAAAAAAAAAAAAAAA\r\n" };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };

        const http_server_handle_req_auth_param_t param = {
            .flag_access_from_lan                   = true,
            .flag_check_rw_access_with_bearer_token = false,
            .http_header                            = http_header,
            .p_remote_ip                            = &remote_ip,
            .p_auth_info                            = &auth_info,
            .p_hostinfo                             = &hostinfo,
        };

        const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
        const string             exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_allow", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(string(""), string(extra_header_fields.buf));
    }
    {
        const http_server_auth_info_t auth_info = {
            HTTP_SERVER_AUTH_TYPE_ALLOW,
            "",
            "",
        };
        const wifiman_hostinfo_t hostinfo = {
            .hostname     = { "RuuviGatewayEEFF" },
            .fw_ver       = { "1.13.0" },
            .nrf52_fw_ver = { "1.0.0" },
        };

        const http_req_header_t    http_header         = { "Cookie: RUUVISESSION=QQQQQQQQQQQQQQQQ\r\n" };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };

        const http_server_handle_req_auth_param_t param = {
            .flag_access_from_lan                   = true,
            .flag_check_rw_access_with_bearer_token = false,
            .http_header                            = http_header,
            .p_remote_ip                            = &remote_ip,
            .p_auth_info                            = &auth_info,
            .p_hostinfo                             = &hostinfo,
        };

        const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
        const string             exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_allow", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"66687aadf862bd776c8fc18b8e9f8e20089714856ee233b3902a591d0d5f2925\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"AAAAAAAAAAAAAAAA\"\r\n"
                   "Set-Cookie: RUUVISESSION=AAAAAAAAAAAAAAAA\r\n"),
            string(extra_header_fields.buf));
    }
}

TEST_F(TestHttpServerHandleReqGetAuth, test_req_check_auth_allow) // NOLINT
{
    const http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_ALLOW,
        "",
        "",
    };
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };

    const http_req_header_t    http_header         = { "" };
    const sta_ip_string_t      remote_ip           = { "192.168.1.10" };
    http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };

    const http_server_handle_req_auth_param_t param = {
        .flag_access_from_lan                   = true,
        .flag_check_rw_access_with_bearer_token = false,
        .http_header                            = http_header,
        .p_remote_ip                            = &remote_ip,
        .p_auth_info                            = &auth_info,
        .p_hostinfo                             = &hostinfo,
    };

    bool                     flag_access_by_bearer_token = false;
    const http_server_resp_t resp                        = http_server_handle_req_check_auth(
        &param,
        &extra_header_fields,
        &flag_access_by_bearer_token);
    const string exp_json_resp
        = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_allow", "lan": true})";
    ASSERT_FALSE(flag_access_by_bearer_token);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(exp_json_resp.length(), resp.content_len);
    ASSERT_EQ(string(""), string(extra_header_fields.buf));
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_allow_when_access_not_from_lan) // NOLINT
{
    const http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_ALLOW,
        "",
        "",
    };
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };

    const http_req_header_t    http_header         = { "" };
    const sta_ip_string_t      remote_ip           = { "192.168.1.10" };
    http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };

    const http_server_handle_req_auth_param_t param = {
        .flag_access_from_lan                   = false,
        .flag_check_rw_access_with_bearer_token = false,
        .http_header                            = http_header,
        .p_remote_ip                            = &remote_ip,
        .p_auth_info                            = &auth_info,
        .p_hostinfo                             = &hostinfo,
    };

    const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
    const string             exp_json_resp
        = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_allow", "lan": false})";
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(exp_json_resp.length(), resp.content_len);
    ASSERT_EQ(string(""), string(extra_header_fields.buf));
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_deny) // NOLINT
{
    const http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_DENY,
        "",
        "",
    };
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };

    const http_req_header_t    http_header         = { "" };
    const sta_ip_string_t      remote_ip           = { "192.168.1.10" };
    http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };

    const http_server_handle_req_auth_param_t param = {
        .flag_access_from_lan                   = true,
        .flag_check_rw_access_with_bearer_token = false,
        .http_header                            = http_header,
        .p_remote_ip                            = &remote_ip,
        .p_auth_info                            = &auth_info,
        .p_hostinfo                             = &hostinfo,
    };
    const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
    const string             exp_json_resp
        = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_deny", "lan": true})";
    ASSERT_EQ(HTTP_RESP_CODE_403, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(exp_json_resp.length(), resp.content_len);
    ASSERT_EQ(string(""), string(extra_header_fields.buf));
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_basic_success) // NOLINT
{
    const string                                 raw_user_pass = "user1:qwe";
    std::array<unsigned char, PASSWORD_BUF_SIZE> encoded_buf {};
    size_t                                       olen = 0;
    assert(!mbedtls_base64_encode(
        encoded_buf.begin(),
        encoded_buf.size(),
        &olen,
        reinterpret_cast<const unsigned char*>(raw_user_pass.c_str()),
        raw_user_pass.length()));
    const string encoded_pass = string(reinterpret_cast<const char*>(encoded_buf.cbegin()));

    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_BASIC,
        "user1",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", encoded_pass.c_str());
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };

    const string               auth_header         = string("Authorization: Basic ") + encoded_pass + string("\r\n");
    const http_req_header_t    http_header         = { auth_header.c_str() };
    http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
    const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

    const http_server_handle_req_auth_param_t param = {
        .flag_access_from_lan                   = true,
        .flag_check_rw_access_with_bearer_token = false,
        .http_header                            = http_header,
        .p_remote_ip                            = &remote_ip,
        .p_auth_info                            = &auth_info,
        .p_hostinfo                             = &hostinfo,
    };
    const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
    const string             exp_json_resp
        = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_basic", "lan": true})";
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(exp_json_resp.length(), resp.content_len);
    ASSERT_EQ(string(""), string(extra_header_fields.buf));
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_basic_fail_no_header_authorization) // NOLINT
{
    const string                                 raw_user_pass = "user1:qwe";
    std::array<unsigned char, PASSWORD_BUF_SIZE> encoded_buf {};
    size_t                                       olen = 0;
    assert(!mbedtls_base64_encode(
        encoded_buf.begin(),
        encoded_buf.size(),
        &olen,
        reinterpret_cast<const unsigned char*>(raw_user_pass.c_str()),
        raw_user_pass.length()));
    const string encoded_pass = string(reinterpret_cast<const char*>(encoded_buf.cbegin()));

    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_BASIC,
        "user1",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", encoded_pass.c_str());
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };

    const http_req_header_t    http_header         = { "" };
    http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
    const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

    const http_server_handle_req_auth_param_t param = {
        .flag_access_from_lan                   = true,
        .flag_check_rw_access_with_bearer_token = false,
        .http_header                            = http_header,
        .p_remote_ip                            = &remote_ip,
        .p_auth_info                            = &auth_info,
        .p_hostinfo                             = &hostinfo,
    };
    const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
    const string             exp_json_resp
        = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_basic", "lan": true})";
    ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(exp_json_resp.length(), resp.content_len);
    ASSERT_EQ(
        string("WWW-Authenticate: Basic realm=\"RuuviGatewayEEFF\", charset=\"UTF-8\"\r\n"),
        string(extra_header_fields.buf));
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_basic_fail_wrong_header_authorization) // NOLINT
{
    const string                                 raw_user_pass = "user1:qwe";
    std::array<unsigned char, PASSWORD_BUF_SIZE> encoded_buf {};
    size_t                                       olen = 0;
    assert(!mbedtls_base64_encode(
        encoded_buf.begin(),
        encoded_buf.size(),
        &olen,
        reinterpret_cast<const unsigned char*>(raw_user_pass.c_str()),
        raw_user_pass.length()));
    const string encoded_pass = string(reinterpret_cast<const char*>(encoded_buf.cbegin()));

    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_BASIC,
        "user1",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", encoded_pass.c_str());
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };

    const string               auth_header         = string("Authorization: unknown ") + encoded_pass;
    const http_req_header_t    http_header         = { auth_header.c_str() };
    http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
    const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

    const http_server_handle_req_auth_param_t param = {
        .flag_access_from_lan                   = true,
        .flag_check_rw_access_with_bearer_token = false,
        .http_header                            = http_header,
        .p_remote_ip                            = &remote_ip,
        .p_auth_info                            = &auth_info,
        .p_hostinfo                             = &hostinfo,
    };
    const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
    const string             exp_json_resp
        = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_basic", "lan": true})";
    ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(exp_json_resp.length(), resp.content_len);
    ASSERT_EQ(
        string("WWW-Authenticate: Basic realm=\"RuuviGatewayEEFF\", charset=\"UTF-8\"\r\n"),
        string(extra_header_fields.buf));
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_basic_fail_short_password) // NOLINT
{
    const string                                 raw_user_pass = "user1:qwe";
    std::array<unsigned char, PASSWORD_BUF_SIZE> encoded_buf {};
    size_t                                       olen = 0;
    assert(!mbedtls_base64_encode(
        encoded_buf.begin(),
        encoded_buf.size(),
        &olen,
        reinterpret_cast<const unsigned char*>(raw_user_pass.c_str()),
        raw_user_pass.length()));
    const string encoded_pass = string(reinterpret_cast<const char*>(encoded_buf.cbegin()));

    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_BASIC,
        "user1",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", encoded_pass.c_str());
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };

    const string               auth_header         = string("Authorization: Basic ") + encoded_pass.substr(0, 2);
    const http_req_header_t    http_header         = { auth_header.c_str() };
    http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
    const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

    const http_server_handle_req_auth_param_t param = {
        .flag_access_from_lan                   = true,
        .flag_check_rw_access_with_bearer_token = false,
        .http_header                            = http_header,
        .p_remote_ip                            = &remote_ip,
        .p_auth_info                            = &auth_info,
        .p_hostinfo                             = &hostinfo,
    };
    const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
    const string             exp_json_resp
        = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_basic", "lan": true})";
    ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(exp_json_resp.length(), resp.content_len);
    ASSERT_EQ(
        string("WWW-Authenticate: Basic realm=\"RuuviGatewayEEFF\", charset=\"UTF-8\"\r\n"),
        string(extra_header_fields.buf));
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_basic_fail_incorrect_password) // NOLINT
{
    const string                                 raw_user_pass = "user1:qwe";
    std::array<unsigned char, PASSWORD_BUF_SIZE> encoded_buf {};
    size_t                                       olen = 0;
    assert(!mbedtls_base64_encode(
        encoded_buf.begin(),
        encoded_buf.size(),
        &olen,
        reinterpret_cast<const unsigned char*>(raw_user_pass.c_str()),
        raw_user_pass.length()));
    const string encoded_pass = string(reinterpret_cast<const char*>(encoded_buf.cbegin()));

    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_BASIC,
        "user1",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", encoded_pass.c_str());
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };

    const string               auth_header         = string("Authorization: Basic qqqqwwwweeee");
    const http_req_header_t    http_header         = { auth_header.c_str() };
    http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
    const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

    const http_server_handle_req_auth_param_t param = {
        .flag_access_from_lan                   = true,
        .flag_check_rw_access_with_bearer_token = false,
        .http_header                            = http_header,
        .p_remote_ip                            = &remote_ip,
        .p_auth_info                            = &auth_info,
        .p_hostinfo                             = &hostinfo,
    };
    const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
    const string             exp_json_resp
        = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_basic", "lan": true})";
    ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(exp_json_resp.length(), resp.content_len);
    ASSERT_EQ(
        string("WWW-Authenticate: Basic realm=\"RuuviGatewayEEFF\", charset=\"UTF-8\"\r\n"),
        string(extra_header_fields.buf));
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_digest_success) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 32> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_DIGEST,
        "user1",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);

    const string auth_header
        = string(
              R"(Authorization: Digest username="user1", realm="RuuviGatewayEEFF", nonce="9689933745abb987e2cfae61d46f50c9efe2fbe9cfa6ad9c3ceb3c54fa2a2833", uri="/auth", response="32a8cf9eae6af8a897ed57a2c51f055d", opaque="d3f1a85625217a33bdda63c646418c2be492100d9d1dec34d6e738c3a1766bc4", qop=auth, nc=00000001, cnonce="3e48baed2616a1e9")")
          + string("\r\n");
    const http_req_header_t    http_header         = { auth_header.c_str() };
    http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
    const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

    const http_server_handle_req_auth_param_t param = {
        .flag_access_from_lan                   = true,
        .flag_check_rw_access_with_bearer_token = false,
        .http_header                            = http_header,
        .p_remote_ip                            = &remote_ip,
        .p_auth_info                            = &auth_info,
        .p_hostinfo                             = &hostinfo,
    };
    const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
    const string             exp_json_resp
        = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_digest", "lan": true})";
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(exp_json_resp.length(), resp.content_len);
    ASSERT_EQ(string(""), string(extra_header_fields.buf));
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_digest_fail_no_header_authorization) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 32> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_DIGEST,
        "user1",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);

    const string               auth_header         = string(R"()");
    const http_req_header_t    http_header         = { auth_header.c_str() };
    http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
    const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

    const http_server_handle_req_auth_param_t param = {
        .flag_access_from_lan                   = true,
        .flag_check_rw_access_with_bearer_token = false,
        .http_header                            = http_header,
        .p_remote_ip                            = &remote_ip,
        .p_auth_info                            = &auth_info,
        .p_hostinfo                             = &hostinfo,
    };
    const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
    const string             exp_json_resp
        = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_digest", "lan": true})";
    ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(exp_json_resp.length(), resp.content_len);
    ASSERT_EQ(
        string("WWW-Authenticate: Digest realm=\"RuuviGatewayEEFF\" qop=\"auth\" "
               "nonce=\"0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2\" "
               "opaque=\"d3f1a85625217a33bdda63c646418c2be492100d9d1dec34d6e738c3a1766bc4\"\r\n"),
        string(extra_header_fields.buf));
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_digest_fail_wrong_header_authorization) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 32> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_DIGEST,
        "user1",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);

    const string               auth_header         = string(R"(Authorization: unknown)");
    const http_req_header_t    http_header         = { auth_header.c_str() };
    http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
    const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

    const http_server_handle_req_auth_param_t param = {
        .flag_access_from_lan                   = true,
        .flag_check_rw_access_with_bearer_token = false,
        .http_header                            = http_header,
        .p_remote_ip                            = &remote_ip,
        .p_auth_info                            = &auth_info,
        .p_hostinfo                             = &hostinfo,
    };
    const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
    const string             exp_json_resp
        = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_digest", "lan": true})";
    ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(exp_json_resp.length(), resp.content_len);
    ASSERT_EQ(
        string("WWW-Authenticate: Digest realm=\"RuuviGatewayEEFF\" qop=\"auth\" "
               "nonce=\"0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2\" "
               "opaque=\"d3f1a85625217a33bdda63c646418c2be492100d9d1dec34d6e738c3a1766bc4\"\r\n"),
        string(extra_header_fields.buf));
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_digest_fail_wrong_password) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":abc");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 32> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_DIGEST,
        "user1",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);

    const string auth_header = string(
        R"(Authorization: Digest username="user1", realm="RuuviGatewayEEFF", nonce="9689933745abb987e2cfae61d46f50c9efe2fbe9cfa6ad9c3ceb3c54fa2a2833", uri="/auth", response="32a8cf9eae6af8a897ed57a2c51f055d", opaque="d3f1a85625217a33bdda63c646418c2be492100d9d1dec34d6e738c3a1766bc4", qop=auth, nc=00000001, cnonce="3e48baed2616a1e9")");
    const http_req_header_t    http_header         = { auth_header.c_str() };
    http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
    const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

    const http_server_handle_req_auth_param_t param = {
        .flag_access_from_lan                   = true,
        .flag_check_rw_access_with_bearer_token = false,
        .http_header                            = http_header,
        .p_remote_ip                            = &remote_ip,
        .p_auth_info                            = &auth_info,
        .p_hostinfo                             = &hostinfo,
    };
    const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
    const string             exp_json_resp
        = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_digest", "lan": true})";
    ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(exp_json_resp.length(), resp.content_len);
    ASSERT_EQ(
        string("WWW-Authenticate: Digest realm=\"RuuviGatewayEEFF\" qop=\"auth\" "
               "nonce=\"0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2\" "
               "opaque=\"d3f1a85625217a33bdda63c646418c2be492100d9d1dec34d6e738c3a1766bc4\"\r\n"),
        string(extra_header_fields.buf));
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_digest_fail_wrong_user) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user2:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 32> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_DIGEST,
        "user2",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);

    const string auth_header = string(
        R"(Authorization: Digest username="user1", realm="RuuviGatewayEEFF", nonce="9689933745abb987e2cfae61d46f50c9efe2fbe9cfa6ad9c3ceb3c54fa2a2833", uri="/auth", response="32a8cf9eae6af8a897ed57a2c51f055d", opaque="d3f1a85625217a33bdda63c646418c2be492100d9d1dec34d6e738c3a1766bc4", qop=auth, nc=00000001, cnonce="3e48baed2616a1e9")");
    const http_req_header_t    http_header         = { auth_header.c_str() };
    http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
    const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

    const http_server_handle_req_auth_param_t param = {
        .flag_access_from_lan                   = true,
        .flag_check_rw_access_with_bearer_token = false,
        .http_header                            = http_header,
        .p_remote_ip                            = &remote_ip,
        .p_auth_info                            = &auth_info,
        .p_hostinfo                             = &hostinfo,
    };
    const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
    const string             exp_json_resp
        = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_digest", "lan": true})";
    ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(exp_json_resp.length(), resp.content_len);
    ASSERT_EQ(
        string("WWW-Authenticate: Digest realm=\"RuuviGatewayEEFF\" qop=\"auth\" "
               "nonce=\"0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2\" "
               "opaque=\"d3f1a85625217a33bdda63c646418c2be492100d9d1dec34d6e738c3a1766bc4\"\r\n"),
        string(extra_header_fields.buf));
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_ruuvi_success) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 128> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        "user1",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);

    // ------ GET /auth -------------------------------------------------------------
    {
        const http_req_header_t    http_header         = { "" };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

        const http_server_handle_req_auth_param_t param = {
            .flag_access_from_lan                   = true,
            .flag_check_rw_access_with_bearer_token = false,
            .http_header                            = http_header,
            .p_remote_ip                            = &remote_ip,
            .p_auth_info                            = &auth_info,
            .p_hostinfo                             = &hostinfo,
        };
        const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
        const string             exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"EVMDULCTKBSJARIZ\"\r\nSet-Cookie: "
                   "RUUVISESSION=EVMDULCTKBSJARIZ\r\n"),
            string(extra_header_fields.buf));
    }

    // ------ POST /auth -------------------------------------------------------------
    {
        const string password_with_challenge = string(
                                                   "0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2:")
                                               + string(user_pass_md5.buf);
        const wifiman_sha256_digest_hex_str_t password_sha256 = wifiman_sha256_calc_hex_str(
            password_with_challenge.c_str(),
            password_with_challenge.length());
        ASSERT_EQ(
            string("a9c36943b9e6e9536bfd5afb610a0f26aea0bcfed5c2cb5d982f13154eab8bce"),
            string(password_sha256.buf));

        const string            http_header_str = string(R"(Cookie: RUUVISESSION=EVMDULCTKBSJARIZ)") + string("\r\n");
        const http_req_header_t http_header     = { http_header_str.c_str() };
        const string http_body_str = string(R"({"login": "user1", "password": ")") + string(password_sha256.buf)
                                     + string("\"}");
        const http_req_body_t      http_body           = { http_body_str.c_str() };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

        const http_server_resp_t resp = http_server_handle_req_post_auth(
            true,
            http_header,
            &remote_ip,
            http_body,
            &auth_info,
            &hostinfo,
            &extra_header_fields);
        const string exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(string(""), string(extra_header_fields.buf));
    }

    // ------ GET /auth -------------------------------------------------------------
    {
        const string            http_header_str = string(R"(Cookie: RUUVISESSION=EVMDULCTKBSJARIZ)") + string("\r\n");
        const http_req_header_t http_header     = { http_header_str.c_str() };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

        const http_server_handle_req_auth_param_t param = {
            .flag_access_from_lan                   = true,
            .flag_check_rw_access_with_bearer_token = false,
            .http_header                            = http_header,
            .p_remote_ip                            = &remote_ip,
            .p_auth_info                            = &auth_info,
            .p_hostinfo                             = &hostinfo,
        };
        const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
        const string             exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(string(""), string(extra_header_fields.buf));
    }

    // ------ DELETE /auth -------------------------------------------------------------
    {
        const string            http_header_str = string(R"(Cookie: RUUVISESSION=EVMDULCTKBSJARIZ)") + string("\r\n");
        const http_req_header_t http_header     = { http_header_str.c_str() };
        const sta_ip_string_t   remote_ip       = { "192.168.1.10" };

        const http_server_resp_t resp = http_server_handle_req_delete_auth(
            http_header,
            &remote_ip,
            &auth_info,
            &hostinfo);
        const string exp_json_resp = R"({})";
        ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
    }

    // ------ GET /auth -------------------------------------------------------------
    {
        const string               http_header_str     = string(R"(Cookie: RUUVISESSION=EVMDULCTKBSJARIZ)");
        const http_req_header_t    http_header         = { http_header_str.c_str() };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

        const http_server_handle_req_auth_param_t param = {
            .flag_access_from_lan                   = true,
            .flag_check_rw_access_with_bearer_token = false,
            .http_header                            = http_header,
            .p_remote_ip                            = &remote_ip,
            .p_auth_info                            = &auth_info,
            .p_hostinfo                             = &hostinfo,
        };
        const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
        const string             exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"d75d22fbdff14ed940298f251b8ed41e4b94947a4cb8c5e029a99b6ea6fe2ab3\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"OFWNEVMDULCTKBSJ\"\r\nSet-Cookie: "
                   "RUUVISESSION=OFWNEVMDULCTKBSJ\r\n"),
            string(extra_header_fields.buf));
    }
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_ruuvi_fail_wrong_password) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 128> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        "user1",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);

    // ------ GET /auth -------------------------------------------------------------
    {
        const http_req_header_t    http_header         = { "" };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

        const http_server_handle_req_auth_param_t param = {
            .flag_access_from_lan                   = true,
            .flag_check_rw_access_with_bearer_token = false,
            .http_header                            = http_header,
            .p_remote_ip                            = &remote_ip,
            .p_auth_info                            = &auth_info,
            .p_hostinfo                             = &hostinfo,
        };
        const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
        const string             exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"EVMDULCTKBSJARIZ\"\r\nSet-Cookie: "
                   "RUUVISESSION=EVMDULCTKBSJARIZ\r\n"),
            string(extra_header_fields.buf));
    }

    // ------ POST /auth -------------------------------------------------------------
    {
        const string            http_header_str = string(R"(Cookie: RUUVISESSION=EVMDULCTKBSJARIZ)");
        const http_req_header_t http_header     = { http_header_str.c_str() };

        const string incorrect_raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":QWE");
        const wifiman_md5_digest_hex_str_t incorrect_user_pass_md5 = wifiman_md5_calc_hex_str(
            incorrect_raw_user_pass.c_str(),
            incorrect_raw_user_pass.length());
        const string password_with_challenge = string(
                                                   "0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2")
                                               + string(":") + string(incorrect_user_pass_md5.buf);
        const wifiman_sha256_digest_hex_str_t password_sha256 = wifiman_sha256_calc_hex_str(
            password_with_challenge.c_str(),
            password_with_challenge.length());

        const string http_body_str = string(R"({"login": "user1", "password": ")") + string(password_sha256.buf)
                                     + string("\"}");
        const http_req_body_t      http_body           = { http_body_str.c_str() };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

        const http_server_resp_t resp = http_server_handle_req_post_auth(
            true,
            http_header,
            &remote_ip,
            http_body,
            &auth_info,
            &hostinfo,
            &extra_header_fields);
        const string exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"d75d22fbdff14ed940298f251b8ed41e4b94947a4cb8c5e029a99b6ea6fe2ab3\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"OFWNEVMDULCTKBSJ\"\r\nSet-Cookie: "
                   "RUUVISESSION=OFWNEVMDULCTKBSJ\r\n"),
            string(extra_header_fields.buf));
    }
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_ruuvi_fail_wrong_user) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 128> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        "user1",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);

    // ------ GET /auth -------------------------------------------------------------
    {
        const http_req_header_t    http_header         = { "" };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

        const http_server_handle_req_auth_param_t param = {
            .flag_access_from_lan                   = true,
            .flag_check_rw_access_with_bearer_token = false,
            .http_header                            = http_header,
            .p_remote_ip                            = &remote_ip,
            .p_auth_info                            = &auth_info,
            .p_hostinfo                             = &hostinfo,
        };
        const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
        const string             exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"EVMDULCTKBSJARIZ\"\r\nSet-Cookie: "
                   "RUUVISESSION=EVMDULCTKBSJARIZ\r\n"),
            string(extra_header_fields.buf));
    }

    // ------ POST /auth -------------------------------------------------------------
    {
        const string            http_header_str = string(R"(Cookie: RUUVISESSION=EVMDULCTKBSJARIZ)");
        const http_req_header_t http_header     = { http_header_str.c_str() };

        const string incorrect_raw_user_pass = string("user2:") + string(hostinfo.hostname.buf) + string(":qwe");
        const wifiman_md5_digest_hex_str_t incorrect_user_pass_md5 = wifiman_md5_calc_hex_str(
            incorrect_raw_user_pass.c_str(),
            incorrect_raw_user_pass.length());
        const string password_with_challenge = string(
                                                   "0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2")
                                               + string(":") + string(incorrect_user_pass_md5.buf);
        const wifiman_sha256_digest_hex_str_t password_sha256 = wifiman_sha256_calc_hex_str(
            password_with_challenge.c_str(),
            password_with_challenge.length());

        const string http_body_str = string(R"({"login": "user2", "password": ")") + string(password_sha256.buf)
                                     + string("\"}");
        const http_req_body_t      http_body           = { http_body_str.c_str() };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

        const http_server_resp_t resp = http_server_handle_req_post_auth(
            true,
            http_header,
            &remote_ip,
            http_body,
            &auth_info,
            &hostinfo,
            &extra_header_fields);
        const string exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"d75d22fbdff14ed940298f251b8ed41e4b94947a4cb8c5e029a99b6ea6fe2ab3\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"OFWNEVMDULCTKBSJ\"\r\nSet-Cookie: "
                   "RUUVISESSION=OFWNEVMDULCTKBSJ\r\n"),
            string(extra_header_fields.buf));
    }
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_ruuvi_fail_empty_user) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 128> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        "user1",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);

    // ------ GET /auth -------------------------------------------------------------
    {
        const http_req_header_t    http_header         = { "" };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

        const http_server_handle_req_auth_param_t param = {
            .flag_access_from_lan                   = true,
            .flag_check_rw_access_with_bearer_token = false,
            .http_header                            = http_header,
            .p_remote_ip                            = &remote_ip,
            .p_auth_info                            = &auth_info,
            .p_hostinfo                             = &hostinfo,
        };
        const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
        const string             exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"EVMDULCTKBSJARIZ\"\r\nSet-Cookie: "
                   "RUUVISESSION=EVMDULCTKBSJARIZ\r\n"),
            string(extra_header_fields.buf));
    }

    // ------ POST /auth -------------------------------------------------------------
    {
        const string            http_header_str = string(R"(Cookie: RUUVISESSION=EVMDULCTKBSJARIZ)");
        const http_req_header_t http_header     = { http_header_str.c_str() };

        const string incorrect_raw_user_pass = string(":") + string(hostinfo.hostname.buf) + string(":qwe");
        const wifiman_md5_digest_hex_str_t incorrect_user_pass_md5 = wifiman_md5_calc_hex_str(
            incorrect_raw_user_pass.c_str(),
            incorrect_raw_user_pass.length());
        const string password_with_challenge = string(
                                                   "0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2")
                                               + string(":") + string(incorrect_user_pass_md5.buf);
        const wifiman_sha256_digest_hex_str_t password_sha256 = wifiman_sha256_calc_hex_str(
            password_with_challenge.c_str(),
            password_with_challenge.length());

        const string http_body_str = string(R"({"login": "", "password": ")") + string(password_sha256.buf)
                                     + string("\"}");
        const http_req_body_t      http_body           = { http_body_str.c_str() };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

        const http_server_resp_t resp = http_server_handle_req_post_auth(
            true,
            http_header,
            &remote_ip,
            http_body,
            &auth_info,
            &hostinfo,
            &extra_header_fields);
        const string exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"d75d22fbdff14ed940298f251b8ed41e4b94947a4cb8c5e029a99b6ea6fe2ab3\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"OFWNEVMDULCTKBSJ\"\r\nSet-Cookie: "
                   "RUUVISESSION=OFWNEVMDULCTKBSJ\r\n"),
            string(extra_header_fields.buf));
    }
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_ruuvi_fail_wrong_realm) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 128> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        "user1",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);

    // ------ GET /auth -------------------------------------------------------------
    {
        const http_req_header_t    http_header         = { "" };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

        const http_server_handle_req_auth_param_t param = {
            .flag_access_from_lan                   = true,
            .flag_check_rw_access_with_bearer_token = false,
            .http_header                            = http_header,
            .p_remote_ip                            = &remote_ip,
            .p_auth_info                            = &auth_info,
            .p_hostinfo                             = &hostinfo,
        };
        const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
        const string             exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"EVMDULCTKBSJARIZ\"\r\nSet-Cookie: "
                   "RUUVISESSION=EVMDULCTKBSJARIZ\r\n"),
            string(extra_header_fields.buf));
    }

    // ------ POST /auth -------------------------------------------------------------
    {
        const string            http_header_str = string(R"(Cookie: RUUVISESSION=EVMDULCTKBSJARIZ)");
        const http_req_header_t http_header     = { http_header_str.c_str() };

        const string incorrect_raw_user_pass = string("user1:") + string("RuuviGateway0101") + string(":qwe");
        const wifiman_md5_digest_hex_str_t incorrect_user_pass_md5 = wifiman_md5_calc_hex_str(
            incorrect_raw_user_pass.c_str(),
            incorrect_raw_user_pass.length());
        const string password_with_challenge = string(
                                                   "0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2")
                                               + string(":") + string(incorrect_user_pass_md5.buf);
        const wifiman_sha256_digest_hex_str_t password_sha256 = wifiman_sha256_calc_hex_str(
            password_with_challenge.c_str(),
            password_with_challenge.length());

        const string http_body_str = string(R"({"login": "user1", "password": ")") + string(password_sha256.buf)
                                     + string("\"}");
        const http_req_body_t      http_body           = { http_body_str.c_str() };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

        const http_server_resp_t resp = http_server_handle_req_post_auth(
            true,
            http_header,
            &remote_ip,
            http_body,
            &auth_info,
            &hostinfo,
            &extra_header_fields);
        const string exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"d75d22fbdff14ed940298f251b8ed41e4b94947a4cb8c5e029a99b6ea6fe2ab3\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"OFWNEVMDULCTKBSJ\"\r\nSet-Cookie: "
                   "RUUVISESSION=OFWNEVMDULCTKBSJ\r\n"),
            string(extra_header_fields.buf));
    }
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_ruuvi_fail_wrong_remote_ip) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 128> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        "user1",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);

    // ------ GET /auth -------------------------------------------------------------
    {
        const http_req_header_t    http_header         = { "" };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

        const http_server_handle_req_auth_param_t param = {
            .flag_access_from_lan                   = true,
            .flag_check_rw_access_with_bearer_token = false,
            .http_header                            = http_header,
            .p_remote_ip                            = &remote_ip,
            .p_auth_info                            = &auth_info,
            .p_hostinfo                             = &hostinfo,
        };
        const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
        const string             exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"EVMDULCTKBSJARIZ\"\r\nSet-Cookie: "
                   "RUUVISESSION=EVMDULCTKBSJARIZ\r\n"),
            string(extra_header_fields.buf));
    }

    // ------ POST /auth -------------------------------------------------------------
    {
        const string            http_header_str = string(R"(Cookie: RUUVISESSION=EVMDULCTKBSJARIZ)");
        const http_req_header_t http_header     = { http_header_str.c_str() };

        const string incorrect_raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
        const wifiman_md5_digest_hex_str_t incorrect_user_pass_md5 = wifiman_md5_calc_hex_str(
            incorrect_raw_user_pass.c_str(),
            incorrect_raw_user_pass.length());
        const string password_with_challenge = string(
                                                   "0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2")
                                               + string(":") + string(incorrect_user_pass_md5.buf);
        const wifiman_sha256_digest_hex_str_t password_sha256 = wifiman_sha256_calc_hex_str(
            password_with_challenge.c_str(),
            password_with_challenge.length());

        const string http_body_str = string(R"({"login": "user1", "password": ")") + string(password_sha256.buf)
                                     + string("\"}");
        const http_req_body_t      http_body           = { http_body_str.c_str() };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.11" };

        const http_server_resp_t resp = http_server_handle_req_post_auth(
            true,
            http_header,
            &remote_ip,
            http_body,
            &auth_info,
            &hostinfo,
            &extra_header_fields);
        const string exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"d75d22fbdff14ed940298f251b8ed41e4b94947a4cb8c5e029a99b6ea6fe2ab3\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"OFWNEVMDULCTKBSJ\"\r\nSet-Cookie: "
                   "RUUVISESSION=OFWNEVMDULCTKBSJ\r\n"),
            string(extra_header_fields.buf));
    }
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_ruuvi_fail_wrong_session_id) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 128> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        "user1",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);

    // ------ GET /auth -------------------------------------------------------------
    {
        const http_req_header_t    http_header         = { "" };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

        const http_server_handle_req_auth_param_t param = {
            .flag_access_from_lan                   = true,
            .flag_check_rw_access_with_bearer_token = false,
            .http_header                            = http_header,
            .p_remote_ip                            = &remote_ip,
            .p_auth_info                            = &auth_info,
            .p_hostinfo                             = &hostinfo,
        };
        const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
        const string             exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"EVMDULCTKBSJARIZ\"\r\nSet-Cookie: "
                   "RUUVISESSION=EVMDULCTKBSJARIZ\r\n"),
            string(extra_header_fields.buf));
    }

    // ------ POST /auth -------------------------------------------------------------
    {
        const string            http_header_str = string(R"(Cookie: RUUVISESSION=EVMDULCTKBSJARIA)");
        const http_req_header_t http_header     = { http_header_str.c_str() };

        const string incorrect_raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
        const wifiman_md5_digest_hex_str_t incorrect_user_pass_md5 = wifiman_md5_calc_hex_str(
            incorrect_raw_user_pass.c_str(),
            incorrect_raw_user_pass.length());
        const string password_with_challenge = string(
                                                   "0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2")
                                               + string(":") + string(incorrect_user_pass_md5.buf);
        const wifiman_sha256_digest_hex_str_t password_sha256 = wifiman_sha256_calc_hex_str(
            password_with_challenge.c_str(),
            password_with_challenge.length());

        const string http_body_str = string(R"({"login": "user1", "password": ")") + string(password_sha256.buf)
                                     + string("\"}");
        const http_req_body_t      http_body           = { http_body_str.c_str() };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

        const http_server_resp_t resp = http_server_handle_req_post_auth(
            true,
            http_header,
            &remote_ip,
            http_body,
            &auth_info,
            &hostinfo,
            &extra_header_fields);
        const string exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"d75d22fbdff14ed940298f251b8ed41e4b94947a4cb8c5e029a99b6ea6fe2ab3\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"OFWNEVMDULCTKBSJ\"\r\nSet-Cookie: "
                   "RUUVISESSION=OFWNEVMDULCTKBSJ\r\n"),
            string(extra_header_fields.buf));
    }
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_ruuvi_fail_empty_session_id) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 128> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        "user1",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);

    // ------ GET /auth -------------------------------------------------------------
    {
        const http_req_header_t    http_header         = { "" };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

        const http_server_handle_req_auth_param_t param = {
            .flag_access_from_lan                   = true,
            .flag_check_rw_access_with_bearer_token = false,
            .http_header                            = http_header,
            .p_remote_ip                            = &remote_ip,
            .p_auth_info                            = &auth_info,
            .p_hostinfo                             = &hostinfo,
        };
        const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
        const string             exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"EVMDULCTKBSJARIZ\"\r\nSet-Cookie: "
                   "RUUVISESSION=EVMDULCTKBSJARIZ\r\n"),
            string(extra_header_fields.buf));
    }

    // ------ POST /auth -------------------------------------------------------------
    {
        const string            http_header_str = string(R"(Cookie: RUUVISESSION=)");
        const http_req_header_t http_header     = { http_header_str.c_str() };

        const string incorrect_raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
        const wifiman_md5_digest_hex_str_t incorrect_user_pass_md5 = wifiman_md5_calc_hex_str(
            incorrect_raw_user_pass.c_str(),
            incorrect_raw_user_pass.length());
        const string password_with_challenge = string(
                                                   "0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2")
                                               + string(":") + string(incorrect_user_pass_md5.buf);
        const wifiman_sha256_digest_hex_str_t password_sha256 = wifiman_sha256_calc_hex_str(
            password_with_challenge.c_str(),
            password_with_challenge.length());

        const string http_body_str = string(R"({"login": "user1", "password": ")") + string(password_sha256.buf)
                                     + string("\"}");
        const http_req_body_t      http_body           = { http_body_str.c_str() };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

        const http_server_resp_t resp = http_server_handle_req_post_auth(
            true,
            http_header,
            &remote_ip,
            http_body,
            &auth_info,
            &hostinfo,
            &extra_header_fields);
        const string exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"d75d22fbdff14ed940298f251b8ed41e4b94947a4cb8c5e029a99b6ea6fe2ab3\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"OFWNEVMDULCTKBSJ\"\r\nSet-Cookie: "
                   "RUUVISESSION=OFWNEVMDULCTKBSJ\r\n"),
            string(extra_header_fields.buf));
    }
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_ruuvi_fail_no_session_id) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 128> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        "user1",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);

    // ------ GET /auth -------------------------------------------------------------
    {
        const http_req_header_t    http_header         = { "" };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

        const http_server_handle_req_auth_param_t param = {
            .flag_access_from_lan                   = true,
            .flag_check_rw_access_with_bearer_token = false,
            .http_header                            = http_header,
            .p_remote_ip                            = &remote_ip,
            .p_auth_info                            = &auth_info,
            .p_hostinfo                             = &hostinfo,
        };
        const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
        const string             exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"EVMDULCTKBSJARIZ\"\r\nSet-Cookie: "
                   "RUUVISESSION=EVMDULCTKBSJARIZ\r\n"),
            string(extra_header_fields.buf));
    }

    // ------ POST /auth -------------------------------------------------------------
    {
        const http_req_header_t http_header = { "" };

        const string incorrect_raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
        const wifiman_md5_digest_hex_str_t incorrect_user_pass_md5 = wifiman_md5_calc_hex_str(
            incorrect_raw_user_pass.c_str(),
            incorrect_raw_user_pass.length());
        const string password_with_challenge = string(
                                                   "0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2")
                                               + string(":") + string(incorrect_user_pass_md5.buf);
        const wifiman_sha256_digest_hex_str_t password_sha256 = wifiman_sha256_calc_hex_str(
            password_with_challenge.c_str(),
            password_with_challenge.length());

        const string http_body_str = string(R"({"login": "user1", "password": ")") + string(password_sha256.buf)
                                     + string("\"}");
        const http_req_body_t      http_body           = { http_body_str.c_str() };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

        const http_server_resp_t resp = http_server_handle_req_post_auth(
            true,
            http_header,
            &remote_ip,
            http_body,
            &auth_info,
            &hostinfo,
            &extra_header_fields);
        const string exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"d75d22fbdff14ed940298f251b8ed41e4b94947a4cb8c5e029a99b6ea6fe2ab3\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"OFWNEVMDULCTKBSJ\"\r\nSet-Cookie: "
                   "RUUVISESSION=OFWNEVMDULCTKBSJ\r\n"),
            string(extra_header_fields.buf));
    }
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_ruuvi_fail_bad_body_missing_quote) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 128> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        "user1",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);

    // ------ GET /auth -------------------------------------------------------------
    {
        const http_req_header_t    http_header         = { "" };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

        const http_server_handle_req_auth_param_t param = {
            .flag_access_from_lan                   = true,
            .flag_check_rw_access_with_bearer_token = false,
            .http_header                            = http_header,
            .p_remote_ip                            = &remote_ip,
            .p_auth_info                            = &auth_info,
            .p_hostinfo                             = &hostinfo,
        };
        const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
        const string             exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"EVMDULCTKBSJARIZ\"\r\nSet-Cookie: "
                   "RUUVISESSION=EVMDULCTKBSJARIZ\r\n"),
            string(extra_header_fields.buf));
    }

    // ------ POST /auth -------------------------------------------------------------
    {
        const string            http_header_str = string(R"(Cookie: RUUVISESSION=EVMDULCTKBSJARIZ)");
        const http_req_header_t http_header     = { http_header_str.c_str() };

        const string incorrect_raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
        const wifiman_md5_digest_hex_str_t incorrect_user_pass_md5 = wifiman_md5_calc_hex_str(
            incorrect_raw_user_pass.c_str(),
            incorrect_raw_user_pass.length());
        const string password_with_challenge = string(
                                                   "0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2")
                                               + string(":") + string(incorrect_user_pass_md5.buf);
        const wifiman_sha256_digest_hex_str_t password_sha256 = wifiman_sha256_calc_hex_str(
            password_with_challenge.c_str(),
            password_with_challenge.length());

        const string http_body_str = string(R"({"login": "user1", "password": ")") + string(password_sha256.buf)
                                     + string("}");
        const http_req_body_t      http_body           = { http_body_str.c_str() };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

        const http_server_resp_t resp = http_server_handle_req_post_auth(
            true,
            http_header,
            &remote_ip,
            http_body,
            &auth_info,
            &hostinfo,
            &extra_header_fields);
        const string exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"d75d22fbdff14ed940298f251b8ed41e4b94947a4cb8c5e029a99b6ea6fe2ab3\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"OFWNEVMDULCTKBSJ\"\r\nSet-Cookie: "
                   "RUUVISESSION=OFWNEVMDULCTKBSJ\r\n"),
            string(extra_header_fields.buf));
    }
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_ruuvi_fail_bad_body_no_username) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 128> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        "user1",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);

    // ------ GET /auth -------------------------------------------------------------
    {
        const http_req_header_t    http_header         = { "" };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

        const http_server_handle_req_auth_param_t param = {
            .flag_access_from_lan                   = true,
            .flag_check_rw_access_with_bearer_token = false,
            .http_header                            = http_header,
            .p_remote_ip                            = &remote_ip,
            .p_auth_info                            = &auth_info,
            .p_hostinfo                             = &hostinfo,
        };
        const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
        const string             exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"EVMDULCTKBSJARIZ\"\r\nSet-Cookie: "
                   "RUUVISESSION=EVMDULCTKBSJARIZ\r\n"),
            string(extra_header_fields.buf));
    }

    // ------ POST /auth -------------------------------------------------------------
    {
        const string            http_header_str = string(R"(Cookie: RUUVISESSION=EVMDULCTKBSJARIZ)");
        const http_req_header_t http_header     = { http_header_str.c_str() };

        const string incorrect_raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
        const wifiman_md5_digest_hex_str_t incorrect_user_pass_md5 = wifiman_md5_calc_hex_str(
            incorrect_raw_user_pass.c_str(),
            incorrect_raw_user_pass.length());
        const string password_with_challenge = string(
                                                   "0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2")
                                               + string(":") + string(incorrect_user_pass_md5.buf);
        const wifiman_sha256_digest_hex_str_t password_sha256 = wifiman_sha256_calc_hex_str(
            password_with_challenge.c_str(),
            password_with_challenge.length());

        const string          http_body_str = string(R"({"password": ")") + string(password_sha256.buf) + string("\"}");
        const http_req_body_t http_body     = { http_body_str.c_str() };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

        const http_server_resp_t resp = http_server_handle_req_post_auth(
            true,
            http_header,
            &remote_ip,
            http_body,
            &auth_info,
            &hostinfo,
            &extra_header_fields);
        const string exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"d75d22fbdff14ed940298f251b8ed41e4b94947a4cb8c5e029a99b6ea6fe2ab3\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"OFWNEVMDULCTKBSJ\"\r\nSet-Cookie: "
                   "RUUVISESSION=OFWNEVMDULCTKBSJ\r\n"),
            string(extra_header_fields.buf));
    }
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_ruuvi_fail_bad_body_no_password) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 128> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        "user1",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);

    // ------ GET /auth -------------------------------------------------------------
    {
        const http_req_header_t    http_header         = { "" };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

        const http_server_handle_req_auth_param_t param = {
            .flag_access_from_lan                   = true,
            .flag_check_rw_access_with_bearer_token = false,
            .http_header                            = http_header,
            .p_remote_ip                            = &remote_ip,
            .p_auth_info                            = &auth_info,
            .p_hostinfo                             = &hostinfo,
        };
        const http_server_resp_t resp = http_server_handle_req_get_auth(&param, &extra_header_fields);
        const string             exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"0c27bbaa345e7915ea7873bc293bc2938d5390af1bb4b2ded388477702c58ed2\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"EVMDULCTKBSJARIZ\"\r\nSet-Cookie: "
                   "RUUVISESSION=EVMDULCTKBSJARIZ\r\n"),
            string(extra_header_fields.buf));
    }

    // ------ POST /auth -------------------------------------------------------------
    {
        const string            http_header_str = string(R"(Cookie: RUUVISESSION=EVMDULCTKBSJARIZ)");
        const http_req_header_t http_header     = { http_header_str.c_str() };

        const string               http_body_str       = string(R"({"login": "user1"})");
        const http_req_body_t      http_body           = { http_body_str.c_str() };
        http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
        const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

        const http_server_resp_t resp = http_server_handle_req_post_auth(
            true,
            http_header,
            &remote_ip,
            http_body,
            &auth_info,
            &hostinfo,
            &extra_header_fields);
        const string exp_json_resp
            = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
        ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
        ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
        ASSERT_TRUE(resp.flag_no_cache);
        ASSERT_TRUE(resp.flag_add_header_date);
        ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
        ASSERT_EQ(nullptr, resp.p_content_type_param);
        ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
        ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
        ASSERT_EQ(exp_json_resp.length(), resp.content_len);
        ASSERT_EQ(
            string("WWW-Authenticate: x-ruuvi-interactive realm=\"RuuviGatewayEEFF\" "
                   "challenge=\"d75d22fbdff14ed940298f251b8ed41e4b94947a4cb8c5e029a99b6ea6fe2ab3\" "
                   "session_cookie=\"RUUVISESSION\" session_id=\"OFWNEVMDULCTKBSJ\"\r\nSet-Cookie: "
                   "RUUVISESSION=OFWNEVMDULCTKBSJ\r\n"),
            string(extra_header_fields.buf));
    }
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_bearer_non_empty_success) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 128> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    const string            bearer_auth_api_key("wH3F9SIiAA3rhG32aJki2Z7ekdFc0vtxuDhxl39zFvw=");
    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        "user1",
        "",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);
    snprintf(auth_info.auth_api_key.buf, sizeof(auth_info.auth_api_key.buf), "%s", bearer_auth_api_key.c_str());

    const string               auth_header = string("Authorization: Bearer ") + bearer_auth_api_key + string("\r\n");
    const http_req_header_t    http_header = { auth_header.c_str() };
    http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
    const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

    bool flag_access_by_bearer_token = false;

    const http_server_handle_req_auth_param_t param = {
        .flag_access_from_lan                   = true,
        .flag_check_rw_access_with_bearer_token = false,
        .http_header                            = http_header,
        .p_remote_ip                            = &remote_ip,
        .p_auth_info                            = &auth_info,
        .p_hostinfo                             = &hostinfo,
    };
    const http_server_resp_t resp = http_server_handle_req_check_auth(
        &param,
        &extra_header_fields,
        &flag_access_by_bearer_token);
    const string exp_json_resp
        = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_bearer", "lan": true})";
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(exp_json_resp.length(), resp.content_len);
    ASSERT_EQ(string(""), string(extra_header_fields.buf));

    ASSERT_TRUE(flag_access_by_bearer_token);
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_bearer_non_empty_rw_access_to_ro_success) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 128> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    const string            bearer_auth_api_key("wH3F9SIiAA3rhG32aJki2Z7ekdFc0vtxuDhxl39zFvw=");
    const string            bearer_auth_api_key_rw("KAv9oAT0c1XzbCF9N/Bnj2mgVR7R4QbBn/L3Wq5/zuI=");
    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        "user1",
        "",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);
    snprintf(auth_info.auth_api_key.buf, sizeof(auth_info.auth_api_key.buf), "%s", bearer_auth_api_key.c_str());
    snprintf(
        auth_info.auth_api_key_rw.buf,
        sizeof(auth_info.auth_api_key_rw.buf),
        "%s",
        bearer_auth_api_key_rw.c_str());

    const string               auth_header = string("Authorization: Bearer ") + bearer_auth_api_key_rw + string("\r\n");
    const http_req_header_t    http_header = { auth_header.c_str() };
    http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
    const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

    bool flag_access_by_bearer_token = false;

    const http_server_handle_req_auth_param_t param = {
        .flag_access_from_lan                   = true,
        .flag_check_rw_access_with_bearer_token = false,
        .http_header                            = http_header,
        .p_remote_ip                            = &remote_ip,
        .p_auth_info                            = &auth_info,
        .p_hostinfo                             = &hostinfo,
    };
    const http_server_resp_t resp = http_server_handle_req_check_auth(
        &param,
        &extra_header_fields,
        &flag_access_by_bearer_token);
    const string exp_json_resp
        = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_bearer", "lan": true})";
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(exp_json_resp.length(), resp.content_len);
    ASSERT_EQ(string(""), string(extra_header_fields.buf));

    ASSERT_TRUE(flag_access_by_bearer_token);
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_bearer_non_empty_rw_access_to_rw_success) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 128> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    const string            bearer_auth_api_key("wH3F9SIiAA3rhG32aJki2Z7ekdFc0vtxuDhxl39zFvw=");
    const string            bearer_auth_api_key_rw("KAv9oAT0c1XzbCF9N/Bnj2mgVR7R4QbBn/L3Wq5/zuI=");
    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        "user1",
        "",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);
    snprintf(auth_info.auth_api_key.buf, sizeof(auth_info.auth_api_key.buf), "%s", bearer_auth_api_key.c_str());
    snprintf(
        auth_info.auth_api_key_rw.buf,
        sizeof(auth_info.auth_api_key_rw.buf),
        "%s",
        bearer_auth_api_key_rw.c_str());

    const string               auth_header = string("Authorization: Bearer ") + bearer_auth_api_key_rw + string("\r\n");
    const http_req_header_t    http_header = { auth_header.c_str() };
    http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
    const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

    bool flag_access_by_bearer_token = false;

    const http_server_handle_req_auth_param_t param = {
        .flag_access_from_lan                   = true,
        .flag_check_rw_access_with_bearer_token = true,
        .http_header                            = http_header,
        .p_remote_ip                            = &remote_ip,
        .p_auth_info                            = &auth_info,
        .p_hostinfo                             = &hostinfo,
    };
    const http_server_resp_t resp = http_server_handle_req_check_auth(
        &param,
        &extra_header_fields,
        &flag_access_by_bearer_token);
    const string exp_json_resp
        = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_bearer", "lan": true})";
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(exp_json_resp.length(), resp.content_len);
    ASSERT_EQ(string(""), string(extra_header_fields.buf));

    ASSERT_TRUE(flag_access_by_bearer_token);
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_bearer_non_empty_ro_access_to_rw_fail) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 128> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    const string            bearer_auth_api_key("wH3F9SIiAA3rhG32aJki2Z7ekdFc0vtxuDhxl39zFvw=");
    const string            bearer_auth_api_key_rw("KAv9oAT0c1XzbCF9N/Bnj2mgVR7R4QbBn/L3Wq5/zuI=");
    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        "user1",
        "",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);
    snprintf(auth_info.auth_api_key.buf, sizeof(auth_info.auth_api_key.buf), "%s", bearer_auth_api_key.c_str());
    snprintf(
        auth_info.auth_api_key_rw.buf,
        sizeof(auth_info.auth_api_key_rw.buf),
        "%s",
        bearer_auth_api_key_rw.c_str());

    const string               auth_header = string("Authorization: Bearer ") + bearer_auth_api_key + string("\r\n");
    const http_req_header_t    http_header = { auth_header.c_str() };
    http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
    const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

    bool flag_access_by_bearer_token = false;

    const http_server_handle_req_auth_param_t param = {
        .flag_access_from_lan                   = true,
        .flag_check_rw_access_with_bearer_token = true,
        .http_header                            = http_header,
        .p_remote_ip                            = &remote_ip,
        .p_auth_info                            = &auth_info,
        .p_hostinfo                             = &hostinfo,
    };
    const http_server_resp_t resp = http_server_handle_req_check_auth(
        &param,
        &extra_header_fields,
        &flag_access_by_bearer_token);
    const string exp_json_resp
        = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_bearer", "lan": true})";
    ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(exp_json_resp.length(), resp.content_len);
    ASSERT_EQ(string(""), string(extra_header_fields.buf));

    ASSERT_TRUE(flag_access_by_bearer_token);
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_bearer_non_empty_failed_different_api_key) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 128> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    const string            bearer_auth_api_key("wH3F9SIiAA3rhG32aJki2Z7ekdFc0vtxuDhxl39zFvw=");
    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        "user1",
        "",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);
    snprintf(auth_info.auth_api_key.buf, sizeof(auth_info.auth_api_key.buf), "%s", bearer_auth_api_key.c_str());

    string wrong_api_key = bearer_auth_api_key;
    for (auto& c : wrong_api_key)
        c = toupper(c);
    const string               auth_header         = string("Authorization: Bearer ") + wrong_api_key + string("\r\n");
    const http_req_header_t    http_header         = { auth_header.c_str() };
    http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
    const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

    bool flag_access_by_bearer_token = false;

    const http_server_handle_req_auth_param_t param = {
        .flag_access_from_lan                   = true,
        .flag_check_rw_access_with_bearer_token = false,
        .http_header                            = http_header,
        .p_remote_ip                            = &remote_ip,
        .p_auth_info                            = &auth_info,
        .p_hostinfo                             = &hostinfo,
    };
    const http_server_resp_t resp = http_server_handle_req_check_auth(
        &param,
        &extra_header_fields,
        &flag_access_by_bearer_token);
    ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    const string exp_json_resp
        = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_bearer", "lan": true})";
    ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(exp_json_resp.length(), resp.content_len);
    ASSERT_EQ(string(""), string(extra_header_fields.buf));

    ASSERT_TRUE(flag_access_by_bearer_token);
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_bearer_non_empty_failed_wrong_api_key_len) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 128> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    const string            bearer_auth_api_key("wH3F9SIiAA3rhG32aJki2Z7ekdFc0vtxuDhxl39zFvw=");
    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        "user1",
        "",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);
    snprintf(auth_info.auth_api_key.buf, sizeof(auth_info.auth_api_key.buf), "%s", bearer_auth_api_key.c_str());

    const string            auth_header = string("Authorization: Bearer ") + string("wrong_key_len") + string("\r\n");
    const http_req_header_t http_header = { auth_header.c_str() };
    http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
    const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

    bool flag_access_by_bearer_token = false;

    const http_server_handle_req_auth_param_t param = {
        .flag_access_from_lan                   = true,
        .flag_check_rw_access_with_bearer_token = false,
        .http_header                            = http_header,
        .p_remote_ip                            = &remote_ip,
        .p_auth_info                            = &auth_info,
        .p_hostinfo                             = &hostinfo,
    };
    const http_server_resp_t resp = http_server_handle_req_check_auth(
        &param,
        &extra_header_fields,
        &flag_access_by_bearer_token);
    ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    const string exp_json_resp
        = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_bearer", "lan": true})";
    ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(exp_json_resp.length(), resp.content_len);
    ASSERT_EQ(string(""), string(extra_header_fields.buf));

    ASSERT_TRUE(flag_access_by_bearer_token);
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_bearer_empty_1) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 128> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    const string            bearer_auth_api_key("wH3F9SIiAA3rhG32aJki2Z7ekdFc0vtxuDhxl39zFvw=");
    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        "user1",
        "",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);

    const string               auth_header = string("Authorization: Bearer ") + bearer_auth_api_key + string("\r\n");
    const http_req_header_t    http_header = { auth_header.c_str() };
    http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
    const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

    bool flag_access_by_bearer_token = false;

    const http_server_handle_req_auth_param_t param = {
        .flag_access_from_lan                   = true,
        .flag_check_rw_access_with_bearer_token = false,
        .http_header                            = http_header,
        .p_remote_ip                            = &remote_ip,
        .p_auth_info                            = &auth_info,
        .p_hostinfo                             = &hostinfo,
    };
    const http_server_resp_t resp = http_server_handle_req_check_auth(
        &param,
        &extra_header_fields,
        &flag_access_by_bearer_token);
    ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    const string exp_json_resp
        = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_bearer", "lan": true})";
    ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(exp_json_resp.length(), resp.content_len);
    ASSERT_EQ(string(""), string(extra_header_fields.buf));

    ASSERT_TRUE(flag_access_by_bearer_token);
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_bearer_empty_2) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 128> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        "user1",
        "",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);

    const string               auth_header         = string("Authorization: Bearer ") + string("\r\n");
    const http_req_header_t    http_header         = { auth_header.c_str() };
    http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
    const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

    bool flag_access_by_bearer_token = false;

    const http_server_handle_req_auth_param_t param = {
        .flag_access_from_lan                   = true,
        .flag_check_rw_access_with_bearer_token = false,
        .http_header                            = http_header,
        .p_remote_ip                            = &remote_ip,
        .p_auth_info                            = &auth_info,
        .p_hostinfo                             = &hostinfo,
    };
    const http_server_resp_t resp = http_server_handle_req_check_auth(
        &param,
        &extra_header_fields,
        &flag_access_by_bearer_token);
    ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    const string exp_json_resp
        = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_bearer", "lan": true})";
    ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(exp_json_resp.length(), resp.content_len);
    ASSERT_EQ(string(""), string(extra_header_fields.buf));

    ASSERT_TRUE(flag_access_by_bearer_token);
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_bearer_no_auth_not_used) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 128> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        "user1",
        "",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);

    const http_req_header_t    http_header         = { "" };
    http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
    const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

    bool flag_access_by_bearer_token = false;

    const http_server_handle_req_auth_param_t param = {
        .flag_access_from_lan                   = true,
        .flag_check_rw_access_with_bearer_token = false,
        .http_header                            = http_header,
        .p_remote_ip                            = &remote_ip,
        .p_auth_info                            = &auth_info,
        .p_hostinfo                             = &hostinfo,
    };
    const http_server_resp_t resp = http_server_handle_req_check_auth(
        &param,
        &extra_header_fields,
        &flag_access_by_bearer_token);
    ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    const string exp_json_resp
        = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
    ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(exp_json_resp.length(), resp.content_len);
    ASSERT_EQ(string(""), string(extra_header_fields.buf));

    ASSERT_FALSE(flag_access_by_bearer_token);
}

TEST_F(TestHttpServerHandleReqGetAuth, test_auth_bearer_wrong_auth_not_used) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { "RuuviGatewayEEFF" },
        .fw_ver       = { "1.13.0" },
        .nrf52_fw_ver = { "1.0.0" },
    };
    const string raw_user_pass = string("user1:") + string(hostinfo.hostname.buf) + string(":qwe");
    const wifiman_md5_digest_hex_str_t user_pass_md5 = wifiman_md5_calc_hex_str(
        raw_user_pass.c_str(),
        raw_user_pass.length());

    std::array<uint32_t, 128> random_values = { 0 };
    for (uint32_t i = 0; i < random_values.size(); ++i)
    {
        random_values[i] = 0xAABBCC00U + i * 17;
    }
    this->set_random_values(random_values.begin(), random_values.size());

    http_server_auth_info_t auth_info = {
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        "user1",
        "",
        "",
    };
    snprintf(auth_info.auth_pass.buf, sizeof(auth_info.auth_pass.buf), "%s", user_pass_md5.buf);

    const string               auth_header         = string("Authorization: BeareQ qwe") + string("\r\n");
    const http_req_header_t    http_header         = { auth_header.c_str() };
    http_header_extra_fields_t extra_header_fields = { .buf = { '\0' } };
    const sta_ip_string_t      remote_ip           = { "192.168.1.10" };

    bool flag_access_by_bearer_token = false;

    const http_server_handle_req_auth_param_t param = {
        .flag_access_from_lan                   = true,
        .flag_check_rw_access_with_bearer_token = false,
        .http_header                            = http_header,
        .p_remote_ip                            = &remote_ip,
        .p_auth_info                            = &auth_info,
        .p_hostinfo                             = &hostinfo,
    };
    const http_server_resp_t resp = http_server_handle_req_check_auth(
        &param,
        &extra_header_fields,
        &flag_access_by_bearer_token);
    ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    const string exp_json_resp
        = R"({"gateway_name": "RuuviGatewayEEFF", "fw_ver": "1.13.0", "nrf52_fw_ver": "1.0.0", "lan_auth_type": "lan_auth_ruuvi", "lan": true})";
    ASSERT_EQ(exp_json_resp, string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
    ASSERT_EQ(exp_json_resp.length(), resp.content_len);
    ASSERT_EQ(string(""), string(extra_header_fields.buf));

    ASSERT_FALSE(flag_access_by_bearer_token);
}
