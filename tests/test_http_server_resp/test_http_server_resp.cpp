/**
 * @file test_http_server_resp.cpp
 * @author TheSomeMan
 * @date 2020-11-23
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gtest/gtest.h"
#include "http_server_resp.h"
#include <string>

using namespace std;

/*** Google-test class implementation *********************************************************************************/

class TestHttpServerResp : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        this->m_idx_random_value = 0;
        std::fill(arr_of_random_values.begin(), arr_of_random_values.end(), 0);
    }

    void
    TearDown() override
    {
        this->m_p_random_values   = nullptr;
        this->m_num_random_values = 0;
    }

public:
    const uint32_t*          m_p_random_values;
    size_t                   m_num_random_values;
    size_t                   m_idx_random_value;
    std::array<uint32_t, 50> arr_of_random_values;

    TestHttpServerResp();

    ~TestHttpServerResp() override;

    void
    set_random_values(const uint32_t* const p_random_values, const size_t num_random_values)
    {
        this->m_p_random_values   = p_random_values;
        this->m_num_random_values = num_random_values;
        this->m_idx_random_value  = 0;
    }
};

static TestHttpServerResp* g_pTestObj;

TestHttpServerResp::TestHttpServerResp()
    : Test()
    , m_p_random_values(nullptr)
    , m_num_random_values(0)
    , m_idx_random_value(0)
{
}

TestHttpServerResp::~TestHttpServerResp()
{
    g_pTestObj = this;
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

TEST_F(TestHttpServerResp, resp_400) // NOLINT
{
    const http_server_resp_t resp = http_server_resp_400();
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(nullptr, resp.select_location.memory.p_buf);
}

TEST_F(TestHttpServerResp, resp_404) // NOLINT
{
    const http_server_resp_t resp = http_server_resp_404();
    ASSERT_EQ(HTTP_RESP_CODE_404, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(nullptr, resp.select_location.memory.p_buf);
}

TEST_F(TestHttpServerResp, resp_503) // NOLINT
{
    const http_server_resp_t resp = http_server_resp_503();
    ASSERT_EQ(HTTP_RESP_CODE_503, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(nullptr, resp.select_location.memory.p_buf);
}

TEST_F(TestHttpServerResp, resp_data_in_flash_html) // NOLINT
{
    const char* html_content = "qwe";

    const http_server_resp_t resp = http_server_resp_data_in_flash(
        HTTP_CONTENT_TYPE_TEXT_HTML,
        nullptr,
        strlen(html_content),
        HTTP_CONTENT_ENCODING_NONE,
        reinterpret_cast<const uint8_t*>(html_content),
        true);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FLASH_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(3, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(reinterpret_cast<const uint8_t*>(html_content), resp.select_location.memory.p_buf);
}

TEST_F(TestHttpServerResp, resp_data_in_flash_js_gzipped_with_param) // NOLINT
{
    const char* js_content = "qwe";
    const char* param_str  = "param1=val1";

    const http_server_resp_t resp = http_server_resp_data_in_flash(
        HTTP_CONTENT_TYPE_TEXT_JAVASCRIPT,
        param_str,
        strlen(js_content),
        HTTP_CONTENT_ENCODING_GZIP,
        reinterpret_cast<const uint8_t*>(js_content),
        true);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FLASH_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_JAVASCRIPT, resp.content_type);
    ASSERT_EQ(param_str, resp.p_content_type_param);
    ASSERT_EQ(3, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_GZIP, resp.content_encoding);
    ASSERT_EQ(reinterpret_cast<const uint8_t*>(js_content), resp.select_location.memory.p_buf);
}

TEST_F(TestHttpServerResp, resp_data_in_static_mem_plain_text_with_caching) // NOLINT
{
    const char* p_content     = "qwer";
    const bool  flag_no_cache = false;
    const bool  flag_add_date = false;

    const http_server_resp_t resp = http_server_resp_data_in_static_mem(
        HTTP_CONTENT_TYPE_TEXT_PLAIN,
        nullptr,
        strlen(p_content),
        HTTP_CONTENT_ENCODING_NONE,
        reinterpret_cast<const uint8_t*>(p_content),
        flag_no_cache,
        flag_add_date);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_EQ(flag_no_cache, resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_PLAIN, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(4, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(reinterpret_cast<const uint8_t*>(p_content), resp.select_location.memory.p_buf);
}

TEST_F(TestHttpServerResp, resp_data_in_static_mem_plain_text_without_caching) // NOLINT
{
    const char* p_content     = "qwer";
    const bool  flag_no_cache = true;
    const bool  flag_add_date = false;

    const http_server_resp_t resp = http_server_resp_data_in_static_mem(
        HTTP_CONTENT_TYPE_TEXT_PLAIN,
        nullptr,
        strlen(p_content),
        HTTP_CONTENT_ENCODING_NONE,
        reinterpret_cast<const uint8_t*>(p_content),
        flag_no_cache,
        flag_add_date);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_EQ(flag_no_cache, resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_PLAIN, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(4, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(reinterpret_cast<const uint8_t*>(p_content), resp.select_location.memory.p_buf);
}

TEST_F(TestHttpServerResp, resp_data_in_heap_json_with_caching) // NOLINT
{
    const char* p_content     = "qwer";
    const bool  flag_no_cache = false;
    const bool  flag_add_date = false;

    const http_server_resp_t resp = http_server_resp_200_data_in_heap(
        HTTP_CONTENT_TYPE_APPLICATION_JSON,
        nullptr,
        strlen(p_content),
        HTTP_CONTENT_ENCODING_NONE,
        reinterpret_cast<const uint8_t*>(p_content),
        flag_no_cache,
        flag_add_date);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_EQ(flag_no_cache, resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(4, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(reinterpret_cast<const uint8_t*>(p_content), resp.select_location.memory.p_buf);
}

TEST_F(TestHttpServerResp, resp_data_in_heap_json_without_caching) // NOLINT
{
    const char* p_content     = "qwer";
    const bool  flag_no_cache = true;
    const bool  flag_add_date = false;

    const http_server_resp_t resp = http_server_resp_200_data_in_heap(
        HTTP_CONTENT_TYPE_APPLICATION_JSON,
        nullptr,
        strlen(p_content),
        HTTP_CONTENT_ENCODING_NONE,
        reinterpret_cast<const uint8_t*>(p_content),
        flag_no_cache,
        flag_add_date);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_EQ(flag_no_cache, resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(4, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(reinterpret_cast<const uint8_t*>(p_content), resp.select_location.memory.p_buf);
}

TEST_F(TestHttpServerResp, resp_data_from_file_css_gzipped) // NOLINT
{
    const char*    p_content = "qwer";
    const socket_t sock      = 5;

    const http_server_resp_t resp = http_server_resp_data_from_file(
        HTTP_RESP_CODE_200,
        HTTP_CONTENT_TYPE_TEXT_CSS,
        nullptr,
        strlen(p_content),
        HTTP_CONTENT_ENCODING_GZIP,
        sock,
        true);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FATFS, resp.content_location);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_CSS, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(4, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_GZIP, resp.content_encoding);
    ASSERT_EQ(sock, resp.select_location.fatfs.fd);
}

TEST_F(TestHttpServerResp, resp_data_from_file_png) // NOLINT
{
    const char*    p_content = "qwer";
    const socket_t sock      = 6;

    const http_server_resp_t resp = http_server_resp_data_from_file(
        HTTP_RESP_CODE_200,
        HTTP_CONTENT_TYPE_IMAGE_PNG,
        nullptr,
        strlen(p_content),
        HTTP_CONTENT_ENCODING_NONE,
        sock,
        true);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FATFS, resp.content_location);
    ASSERT_EQ(HTTP_CONTENT_TYPE_IMAGE_PNG, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(4, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(sock, resp.select_location.fatfs.fd);
}

TEST_F(TestHttpServerResp, resp_data_from_file_svg) // NOLINT
{
    const char*    p_content = "qwere";
    const socket_t sock      = 7;

    const http_server_resp_t resp = http_server_resp_data_from_file(
        HTTP_RESP_CODE_200,
        HTTP_CONTENT_TYPE_IMAGE_SVG_XML,
        nullptr,
        strlen(p_content),
        HTTP_CONTENT_ENCODING_NONE,
        sock,
        true);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FATFS, resp.content_location);
    ASSERT_EQ(HTTP_CONTENT_TYPE_IMAGE_SVG_XML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(5, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(sock, resp.select_location.fatfs.fd);
}

TEST_F(TestHttpServerResp, resp_data_from_file_octet_stream) // NOLINT
{
    const char*    p_content = "qwere";
    const socket_t sock      = 7;

    const http_server_resp_t resp = http_server_resp_data_from_file(
        HTTP_RESP_CODE_200,
        HTTP_CONTENT_TYPE_APPLICATION_OCTET_STREAM,
        nullptr,
        strlen(p_content),
        HTTP_CONTENT_ENCODING_NONE,
        sock,
        true);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_FATFS, resp.content_location);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_OCTET_STREAM, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(5, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(sock, resp.select_location.fatfs.fd);
}

TEST_F(TestHttpServerResp, test_http_server_resp_200_auth_allow_with_new_session_id) // NOLINT
{
    const bool                 flag_no_cache       = true;
    const sta_ip_string_t      remote_ip           = { "192.168.1.110" };
    const wifiman_hostinfo_t   hostinfo            = { .hostname     = { "hostname" },
                                                       .fw_ver       = { "v1.15.0" },
                                                       .nrf52_fw_ver = { "v1.0.0" } };
    http_header_extra_fields_t extra_header_fields = { '\0' };

    std::fill(arr_of_random_values.begin(), arr_of_random_values.end(), 0);
    set_random_values(this->arr_of_random_values.data(), this->arr_of_random_values.size());

    const http_server_resp_t resp = http_server_resp_200_auth_allow_with_new_session_id(
        &remote_ip,
        &hostinfo,
        &extra_header_fields);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_EQ(flag_no_cache, resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(123, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        "{\"gateway_name\": \"hostname\", \"fw_ver\": \"v1.15.0\", \"nrf52_fw_ver\": \"v1.0.0\", \"lan_auth_type\": "
        "\"lan_auth_allow\", \"lan\": true}",
        string(reinterpret_cast<const char*>(resp.select_location.memory.p_buf)));
}
