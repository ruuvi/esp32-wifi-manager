/**
 * @file test_http_server_resp.cpp
 * @author TheSomeMan
 * @date 2020-11-23
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gtest/gtest.h"
#include "http_server_resp.h"
#include "http_server_auth.h"
#include "json_stream_gen.h"
#include <string>
#include <cstring>
#include <cstdlib>

using namespace std;

/*** Google-test class implementation *********************************************************************************/

static bool g_flag_force_empty_sha256_calc_hex_str = false;
static bool g_flag_force_empty_sha256_hex_str      = false;

class TestHttpServerResp;
static TestHttpServerResp* g_pTestObj;

class TestHttpServerResp : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        g_pTestObj               = this;
        this->m_idx_random_value = 0;
        std::fill(arr_of_random_values.begin(), arr_of_random_values.end(), 0);
        http_server_auth_clear_authorized_sessions();
        g_flag_force_empty_sha256_calc_hex_str = false;
        g_flag_force_empty_sha256_hex_str      = false;
        this->m_alloc_free_call_count          = 0;
        this->m_flag_alloc_counting_enabled    = true;
        this->m_close_call_count               = 0;
        this->m_close_last_fd                  = -1;
    }

    void
    TearDown() override
    {
        this->m_flag_alloc_counting_enabled = false;
        this->m_alloc_free_call_count       = 0;
        this->m_p_random_values             = nullptr;
        this->m_num_random_values           = 0;
        g_pTestObj                          = nullptr;
    }

public:
    const uint32_t*          m_p_random_values;
    size_t                   m_num_random_values;
    size_t                   m_idx_random_value;
    std::array<uint32_t, 50> arr_of_random_values;

    bool m_flag_alloc_counting_enabled;
    int  m_alloc_free_call_count;
    int  m_close_call_count;
    int  m_close_last_fd;

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

TestHttpServerResp::TestHttpServerResp()
    : Test()
    , m_p_random_values(nullptr)
    , m_num_random_values(0)
    , m_idx_random_value(0)
    , m_flag_alloc_counting_enabled(false)
    , m_alloc_free_call_count(0)
    , m_close_call_count(0)
    , m_close_last_fd(-1)
{
}

TestHttpServerResp::~TestHttpServerResp() = default;

#ifdef __cplusplus
extern "C" {
#endif

void*
__wrap_malloc(size_t size)
{
    extern void* __real_malloc(size_t _size) __THROW __attribute_malloc__ __attribute_alloc_size__((1)) __wur;
    if (g_pTestObj && g_pTestObj->m_flag_alloc_counting_enabled)
    {
        g_pTestObj->m_alloc_free_call_count += 1;
    }
    return __real_malloc(size);
}

void*
__wrap_calloc(size_t nmemb, size_t size)
{
    extern void*                     __real_calloc(size_t _nmemb, size_t _size)
        __THROW __attribute_malloc__ __attribute_alloc_size__((1, 2)) __wur;
    if (g_pTestObj && g_pTestObj->m_flag_alloc_counting_enabled)
    {
        g_pTestObj->m_alloc_free_call_count += 1;
    }
    return __real_calloc(nmemb, size);
}

void
__wrap_free(void* ptr)
{
    extern void __real_free(void* _ptr) __THROW;
    if (g_pTestObj && g_pTestObj->m_flag_alloc_counting_enabled)
    {
        g_pTestObj->m_alloc_free_call_count -= 1;
    }
    __real_free(ptr);
}

int
__wrap_close(int fd)
{
    if (g_pTestObj)
    {
        g_pTestObj->m_close_call_count += 1;
        g_pTestObj->m_close_last_fd = fd;
    }
    return 0;
}

wifiman_sha256_digest_hex_str_t
__real_wifiman_sha256_calc_hex_str(const void* const p_buf, const size_t buf_size);

wifiman_sha256_digest_hex_str_t
__wrap_wifiman_sha256_calc_hex_str(const void* const p_buf, const size_t buf_size)
{
    if (g_flag_force_empty_sha256_calc_hex_str)
    {
        return (wifiman_sha256_digest_hex_str_t) { 0 };
    }
    return __real_wifiman_sha256_calc_hex_str(p_buf, buf_size);
}

wifiman_sha256_digest_hex_str_t
__real_wifiman_sha256_hex_str(const wifiman_sha256_digest_t* const p_digest);

wifiman_sha256_digest_hex_str_t
__wrap_wifiman_sha256_hex_str(const wifiman_sha256_digest_t* const p_digest)
{
    if (g_flag_force_empty_sha256_hex_str)
    {
        return (wifiman_sha256_digest_hex_str_t) { 0 };
    }
    return __real_wifiman_sha256_hex_str(p_digest);
}

static json_stream_gen_callback_result_t
test_resp_json_generator_cb(json_stream_gen_t* const p_gen, const void* const p_user_ctx)
{
    (void)p_user_ctx;
    JSON_STREAM_GEN_BEGIN_GENERATOR_FUNC(p_gen);
    JSON_STREAM_GEN_START_OBJECT(p_gen, NULL);
    JSON_STREAM_GEN_ADD_INT32(p_gen, "a", 1);
    JSON_STREAM_GEN_END_OBJECT(p_gen);
    JSON_STREAM_GEN_END_GENERATOR_FUNC();
}

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

/*** Helper to create heap-allocated copy of a string *****************************************************************/

static char*
heap_strdup(const char* const p_str)
{
    const size_t len   = strlen(p_str) + 1;
    char* const  p_buf = static_cast<char*>(malloc(len));
    assert(nullptr != p_buf);
    memcpy(p_buf, p_str, len);
    return p_buf;
}

/*** Unit-Tests *******************************************************************************************************/

TEST_F(TestHttpServerResp, resp_400) // NOLINT
{
    http_server_resp_t resp = http_server_resp_400();
    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_403) // NOLINT
{
    http_server_resp_t resp = http_server_resp_403();
    ASSERT_EQ(HTTP_RESP_CODE_403, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_403_json) // NOLINT
{
    const char*                  p_auth_json_content = "{\"message\":\"forbidden\"}";
    http_server_resp_auth_json_t auth_json           = { '\0' };
    strncpy(auth_json.buf, p_auth_json_content, sizeof(auth_json.buf) - 1);

    http_server_resp_t resp = http_server_resp_403_json(&auth_json);
    ASSERT_EQ(HTTP_RESP_CODE_403, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(p_auth_json_content), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        string(p_auth_json_content),
        string(reinterpret_cast<const char*>(resp.select_location.static_mem.p_buf)));
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_403_json_null_auth_json_ptr_fallback_to_err) // NOLINT
{
    http_server_resp_t resp = http_server_resp_403_json(nullptr);
    ASSERT_EQ(HTTP_RESP_CODE_403, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(0, resp.content_len);
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_404) // NOLINT
{
    http_server_resp_t resp = http_server_resp_404();
    ASSERT_EQ(HTTP_RESP_CODE_404, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_302) // NOLINT
{
    http_server_resp_t resp = http_server_resp_302();
    ASSERT_EQ(HTTP_RESP_CODE_302, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_409) // NOLINT
{
    http_server_resp_t resp = http_server_resp_409();
    ASSERT_EQ(HTTP_RESP_CODE_409, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_503) // NOLINT
{
    http_server_resp_t resp = http_server_resp_503();
    ASSERT_EQ(HTTP_RESP_CODE_503, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(0, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_500) // NOLINT
{
    http_server_resp_t resp = http_server_resp_500();
    ASSERT_EQ(HTTP_RESP_CODE_500, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(0, resp.content_len);
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_502) // NOLINT
{
    http_server_resp_t resp = http_server_resp_502();
    ASSERT_EQ(HTTP_RESP_CODE_502, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(0, resp.content_len);
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_504) // NOLINT
{
    http_server_resp_t resp = http_server_resp_504();
    ASSERT_EQ(HTTP_RESP_CODE_504, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(0, resp.content_len);
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_401_json) // NOLINT
{
    const char*                  p_auth_json_content = "{\"auth\":\"required\"}";
    http_server_resp_auth_json_t auth_json           = { '\0' };
    strncpy(auth_json.buf, p_auth_json_content, sizeof(auth_json.buf) - 1);

    http_server_resp_t resp = http_server_resp_401_json(&auth_json);
    ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(strlen(p_auth_json_content), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        string(p_auth_json_content),
        string(reinterpret_cast<const char*>(resp.select_location.static_mem.p_buf)));
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_401_json_null_auth_json_ptr_fallback_to_err) // NOLINT
{
    http_server_resp_t resp = http_server_resp_401_json(nullptr);
    ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(0, resp.content_len);
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_json_in_heap_and_200_json_in_heap) // NOLINT
{
    const char* p_json_str = "{\"ok\":true}";

    char*              p_json_content_1 = heap_strdup(p_json_str);
    http_server_resp_t resp_504         = http_server_resp_json_in_heap(HTTP_RESP_CODE_504, p_json_content_1);
    ASSERT_EQ(HTTP_RESP_CODE_504, resp_504.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp_504.content_location);
    ASSERT_TRUE(resp_504.flag_no_cache);
    ASSERT_TRUE(resp_504.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp_504.content_type);
    ASSERT_EQ(strlen(p_json_str), resp_504.content_len);
    ASSERT_EQ(string(p_json_str), string(reinterpret_cast<char*>(resp_504.select_location.heap.p_buf)));
    http_server_resp_free(&resp_504);
    ASSERT_EQ(0, this->m_alloc_free_call_count);

    char*              p_json_content_2 = heap_strdup(p_json_str);
    http_server_resp_t resp_200         = http_server_resp_200_json_in_heap(p_json_content_2);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp_200.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp_200.content_location);
    ASSERT_TRUE(resp_200.flag_no_cache);
    ASSERT_TRUE(resp_200.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp_200.content_type);
    ASSERT_EQ(strlen(p_json_str), resp_200.content_len);
    ASSERT_EQ(string(p_json_str), string(reinterpret_cast<char*>(resp_200.select_location.heap.p_buf)));
    http_server_resp_free(&resp_200);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_502_json_in_heap) // NOLINT
{
    const char*        p_json_str     = "{\"error\":\"bad_gateway\"}";
    char*              p_json_content = heap_strdup(p_json_str);
    http_server_resp_t resp           = http_server_resp_502_json_in_heap(p_json_content);
    ASSERT_EQ(HTTP_RESP_CODE_502, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(strlen(p_json_str), resp.content_len);
    ASSERT_EQ(string(p_json_str), string(reinterpret_cast<char*>(resp.select_location.heap.p_buf)));
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_502_json_in_heap_null_fallback_to_err) // NOLINT
{
    http_server_resp_t resp = http_server_resp_502_json_in_heap(nullptr);
    ASSERT_EQ(HTTP_RESP_CODE_502, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(0, resp.content_len);
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_json_generator) // NOLINT
{
    void*                 p_ctx         = nullptr;
    json_stream_gen_cfg_t cfg           = {};
    cfg.max_chunk_size                  = JSON_STREAM_GEN_CFG_DEFAULT_MAX_CHUNK_SIZE;
    cfg.flag_formatted_json             = false;
    cfg.indentation_mark                = JSON_STREAM_GEN_CFG_DEFAULT_INDENTATION_MARK;
    cfg.indentation                     = JSON_STREAM_GEN_CFG_DEFAULT_INDENTATION;
    cfg.max_nesting_level               = JSON_STREAM_GEN_CFG_DEFAULT_MAX_NESTING_LEVEL;
    cfg.p_malloc                        = &malloc;
    cfg.p_free                          = &free;
    cfg.p_localeconv                    = &localeconv;
    json_stream_gen_t* const p_json_gen = json_stream_gen_create(&cfg, test_resp_json_generator_cb, 0, &p_ctx);
    ASSERT_NE(nullptr, p_json_gen);

    http_server_resp_t resp = http_server_resp_json_generator(HTTP_RESP_CODE_409, p_json_gen);
    ASSERT_EQ(HTTP_RESP_CODE_409, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_JSON_GENERATOR, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_GT(resp.content_len, 0);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(
        string("{{\"a\":1}}"),
        string(json_stream_gen_get_next_chunk(resp.select_location.json_generator.p_json_gen)));

    http_server_resp_free(&resp);
    ASSERT_EQ(nullptr, resp.select_location.json_generator.p_json_gen);
}

TEST_F(TestHttpServerResp, resp_200_json_generator) // NOLINT
{
    void*                 p_ctx         = nullptr;
    json_stream_gen_cfg_t cfg           = {};
    cfg.max_chunk_size                  = JSON_STREAM_GEN_CFG_DEFAULT_MAX_CHUNK_SIZE;
    cfg.flag_formatted_json             = false;
    cfg.indentation_mark                = JSON_STREAM_GEN_CFG_DEFAULT_INDENTATION_MARK;
    cfg.indentation                     = JSON_STREAM_GEN_CFG_DEFAULT_INDENTATION;
    cfg.max_nesting_level               = JSON_STREAM_GEN_CFG_DEFAULT_MAX_NESTING_LEVEL;
    cfg.p_malloc                        = &malloc;
    cfg.p_free                          = &free;
    cfg.p_localeconv                    = &localeconv;
    json_stream_gen_t* const p_json_gen = json_stream_gen_create(&cfg, test_resp_json_generator_cb, 0, &p_ctx);
    ASSERT_NE(nullptr, p_json_gen);

    http_server_resp_t resp = http_server_resp_200_json_generator(p_json_gen);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_JSON_GENERATOR, resp.content_location);
    ASSERT_GT(resp.content_len, 0);
    ASSERT_EQ(
        string("{{\"a\":1}}"),
        string(json_stream_gen_get_next_chunk(resp.select_location.json_generator.p_json_gen)));

    http_server_resp_free(&resp);
    ASSERT_EQ(nullptr, resp.select_location.json_generator.p_json_gen);
}

TEST_F(TestHttpServerResp, resp_data_in_flash_html) // NOLINT
{
    const char* html_content = "qwe";

    http_server_resp_t resp = http_server_resp_data_in_flash(
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
    ASSERT_EQ(reinterpret_cast<const uint8_t*>(html_content), resp.select_location.flash.p_buf);
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_data_in_flash_js_gzipped_with_param) // NOLINT
{
    const char* js_content = "qwe";
    const char* param_str  = "param1=val1";

    http_server_resp_t resp = http_server_resp_data_in_flash(
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
    ASSERT_EQ(reinterpret_cast<const uint8_t*>(js_content), resp.select_location.flash.p_buf);
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_data_in_static_mem_plain_text_with_caching) // NOLINT
{
    const char* p_content     = "qwer";
    const bool  flag_no_cache = false;
    const bool  flag_add_date = false;

    http_server_resp_t resp = http_server_resp_data_in_static_mem(
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
    ASSERT_EQ(reinterpret_cast<const uint8_t*>(p_content), resp.select_location.static_mem.p_buf);
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_data_in_static_mem_plain_text_without_caching) // NOLINT
{
    const char* p_content     = "qwer";
    const bool  flag_no_cache = true;
    const bool  flag_add_date = false;

    http_server_resp_t resp = http_server_resp_data_in_static_mem(
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
    ASSERT_EQ(reinterpret_cast<const uint8_t*>(p_content), resp.select_location.static_mem.p_buf);
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_data_in_heap_json_with_caching) // NOLINT
{
    const char* p_content_str = "qwer";
    const bool  flag_no_cache = false;
    const bool  flag_add_date = false;

    char*              p_content = heap_strdup(p_content_str);
    http_server_resp_t resp      = http_server_resp_200_data_in_heap(
        HTTP_CONTENT_TYPE_APPLICATION_JSON,
        nullptr,
        strlen(p_content_str),
        HTTP_CONTENT_ENCODING_NONE,
        reinterpret_cast<uint8_t*>(p_content),
        flag_no_cache,
        flag_add_date);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_EQ(flag_no_cache, resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(4, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(string(p_content_str), string(reinterpret_cast<char*>(resp.select_location.heap.p_buf)));
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_data_in_heap_json_without_caching) // NOLINT
{
    const char* p_content_str = "qwer";
    const bool  flag_no_cache = true;
    const bool  flag_add_date = false;

    char*              p_content = heap_strdup(p_content_str);
    http_server_resp_t resp      = http_server_resp_200_data_in_heap(
        HTTP_CONTENT_TYPE_APPLICATION_JSON,
        nullptr,
        strlen(p_content_str),
        HTTP_CONTENT_ENCODING_NONE,
        reinterpret_cast<uint8_t*>(p_content),
        flag_no_cache,
        flag_add_date);
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_EQ(flag_no_cache, resp.flag_no_cache);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(4, resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(string(p_content_str), string(reinterpret_cast<char*>(resp.select_location.heap.p_buf)));
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_text_in_heap) // NOLINT
{
    const char* p_content_str = "forbidden";

    char*              p_content = heap_strdup(p_content_str);
    http_server_resp_t resp      = http_server_resp_text_in_heap(HTTP_RESP_CODE_403, p_content);
    ASSERT_EQ(HTTP_RESP_CODE_403, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_HEAP, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_PLAIN, resp.content_type);
    ASSERT_EQ(nullptr, resp.p_content_type_param);
    ASSERT_EQ(strlen(p_content_str), resp.content_len);
    ASSERT_EQ(HTTP_CONTENT_ENCODING_NONE, resp.content_encoding);
    ASSERT_EQ(string(p_content_str), string(reinterpret_cast<char*>(resp.select_location.heap.p_buf)));
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_data_from_file_css_gzipped) // NOLINT
{
    const char*    p_content = "qwer";
    const socket_t sock      = 5;

    http_server_resp_t resp = http_server_resp_data_from_file(
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
    http_server_resp_free(&resp);
    ASSERT_EQ(1, this->m_close_call_count);
    ASSERT_EQ(sock, this->m_close_last_fd);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_data_from_file_png) // NOLINT
{
    const char*    p_content = "qwer";
    const socket_t sock      = 6;

    http_server_resp_t resp = http_server_resp_data_from_file(
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
    http_server_resp_free(&resp);
    ASSERT_EQ(1, this->m_close_call_count);
    ASSERT_EQ(sock, this->m_close_last_fd);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_data_from_file_svg) // NOLINT
{
    const char*    p_content = "qwere";
    const socket_t sock      = 7;

    http_server_resp_t resp = http_server_resp_data_from_file(
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
    http_server_resp_free(&resp);
    ASSERT_EQ(1, this->m_close_call_count);
    ASSERT_EQ(sock, this->m_close_last_fd);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_data_from_file_octet_stream) // NOLINT
{
    const char*    p_content = "qwere";
    const socket_t sock      = 7;

    http_server_resp_t resp = http_server_resp_data_from_file(
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
    http_server_resp_free(&resp);
    ASSERT_EQ(1, this->m_close_call_count);
    ASSERT_EQ(sock, this->m_close_last_fd);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, get_content_ptr_if_in_memory_returns_ptr_for_flash_static_heap) // NOLINT
{
    const char* p_flash_content  = "flash_data";
    const char* p_static_content = "static_data";
    const char* p_heap_content   = "heap_data";

    http_server_resp_t resp_flash = http_server_resp_data_in_flash(
        HTTP_CONTENT_TYPE_TEXT_PLAIN,
        nullptr,
        strlen(p_flash_content),
        HTTP_CONTENT_ENCODING_NONE,
        reinterpret_cast<const uint8_t*>(p_flash_content),
        true);
    ASSERT_EQ(p_flash_content, http_server_resp_get_content_ptr_if_in_memory(&resp_flash));
    http_server_resp_free(&resp_flash);

    http_server_resp_t resp_static = http_server_resp_data_in_static_mem(
        HTTP_CONTENT_TYPE_TEXT_PLAIN,
        nullptr,
        strlen(p_static_content),
        HTTP_CONTENT_ENCODING_NONE,
        reinterpret_cast<const uint8_t*>(p_static_content),
        true,
        false);
    ASSERT_EQ(p_static_content, http_server_resp_get_content_ptr_if_in_memory(&resp_static));
    http_server_resp_free(&resp_static);

    char*              p_heap_buf = heap_strdup(p_heap_content);
    http_server_resp_t resp_heap  = http_server_resp_200_data_in_heap(
        HTTP_CONTENT_TYPE_TEXT_PLAIN,
        nullptr,
        strlen(p_heap_content),
        HTTP_CONTENT_ENCODING_NONE,
        reinterpret_cast<uint8_t*>(p_heap_buf),
        true,
        false);
    ASSERT_EQ(p_heap_buf, http_server_resp_get_content_ptr_if_in_memory(&resp_heap));
    http_server_resp_free(&resp_heap);

    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, get_content_ptr_if_in_memory_returns_null_for_non_memory_locations) // NOLINT
{
    http_server_resp_t resp_no_content = http_server_resp_404();
    ASSERT_EQ(nullptr, http_server_resp_get_content_ptr_if_in_memory(&resp_no_content));
    http_server_resp_free(&resp_no_content);

    const socket_t     sock      = 11;
    http_server_resp_t resp_file = http_server_resp_data_from_file(
        HTTP_RESP_CODE_200,
        HTTP_CONTENT_TYPE_TEXT_CSS,
        nullptr,
        3,
        HTTP_CONTENT_ENCODING_NONE,
        sock,
        true);
    ASSERT_EQ(nullptr, http_server_resp_get_content_ptr_if_in_memory(&resp_file));
    http_server_resp_free(&resp_file);
    ASSERT_EQ(1, this->m_close_call_count);
    ASSERT_EQ(sock, this->m_close_last_fd);

    void*                 p_ctx         = nullptr;
    json_stream_gen_cfg_t cfg           = {};
    cfg.max_chunk_size                  = JSON_STREAM_GEN_CFG_DEFAULT_MAX_CHUNK_SIZE;
    cfg.flag_formatted_json             = false;
    cfg.indentation_mark                = JSON_STREAM_GEN_CFG_DEFAULT_INDENTATION_MARK;
    cfg.indentation                     = JSON_STREAM_GEN_CFG_DEFAULT_INDENTATION;
    cfg.max_nesting_level               = JSON_STREAM_GEN_CFG_DEFAULT_MAX_NESTING_LEVEL;
    cfg.p_malloc                        = &malloc;
    cfg.p_free                          = &free;
    cfg.p_localeconv                    = &localeconv;
    json_stream_gen_t* const p_json_gen = json_stream_gen_create(&cfg, test_resp_json_generator_cb, 0, &p_ctx);
    ASSERT_NE(nullptr, p_json_gen);

    http_server_resp_t resp_gen = http_server_resp_200_json_generator(p_json_gen);
    ASSERT_EQ(nullptr, http_server_resp_get_content_ptr_if_in_memory(&resp_gen));
    http_server_resp_free(&resp_gen);

    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, get_content_ptr_if_in_memory_returns_null_for_explicit_non_memory_enums) // NOLINT
{
    http_server_resp_t resp            = {};
    const char*        p_flash_content = "flash_data";
    resp.select_location.flash.p_buf   = reinterpret_cast<const uint8_t*>(p_flash_content);
    resp.content_len                   = strlen(p_flash_content);

    resp.content_location = HTTP_CONTENT_LOCATION_NO_CONTENT;
    ASSERT_EQ(nullptr, http_server_resp_get_content_ptr_if_in_memory(&resp));

    resp.content_location = HTTP_CONTENT_LOCATION_FATFS;
    ASSERT_EQ(nullptr, http_server_resp_get_content_ptr_if_in_memory(&resp));

    resp.content_location = HTTP_CONTENT_LOCATION_JSON_GENERATOR;
    ASSERT_EQ(nullptr, http_server_resp_get_content_ptr_if_in_memory(&resp));

    ASSERT_EQ(0, this->m_alloc_free_call_count);
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

    http_server_resp_t resp = http_server_resp_200_auth_allow_with_new_session_id(
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
        string(reinterpret_cast<const char*>(resp.select_location.static_mem.p_buf)));
    ASSERT_EQ(
        "WWW-Authenticate: x-ruuvi-interactive realm=\"hostname\" "
        "challenge=\"66687aadf862bd776c8fc18b8e9f8e20089714856ee233b3902a591d0d5f2925\" "
        "session_cookie=\"RUUVISESSION\" session_id=\"AAAAAAAAAAAAAAAA\"\r\n"
        "Set-Cookie: RUUVISESSION=AAAAAAAAAAAAAAAA\r\n",
        string(extra_header_fields.buf));

    const http_server_auth_ruuvi_t* const                    p_auth    = http_server_auth_ruuvi_get_info();
    const http_server_auth_ruuvi_authorized_session_t* const p_session = &p_auth->authorized_sessions[0];
    ASSERT_EQ("AAAAAAAAAAAAAAAA", string(p_session->session_id.buf));
    ASSERT_EQ(string(remote_ip.buf), string(p_session->remote_ip.buf));
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_200_auth_allow_with_new_session_id_empty_challenge_returns_503) // NOLINT
{
    const sta_ip_string_t      remote_ip           = { "192.168.1.110" };
    const wifiman_hostinfo_t   hostinfo            = { .hostname     = { "hostname" },
                                                       .fw_ver       = { "v1.15.0" },
                                                       .nrf52_fw_ver = { "v1.0.0" } };
    http_header_extra_fields_t extra_header_fields = { '\0' };

    std::fill(arr_of_random_values.begin(), arr_of_random_values.end(), 0);
    set_random_values(this->arr_of_random_values.data(), this->arr_of_random_values.size());

    g_flag_force_empty_sha256_hex_str = true;

    http_server_resp_t resp = http_server_resp_200_auth_allow_with_new_session_id(
        &remote_ip,
        &hostinfo,
        &extra_header_fields);
    ASSERT_EQ(HTTP_RESP_CODE_503, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_401_auth_ruuvi) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = { .hostname     = { "hostname" },
                                          .fw_ver       = { "v1.15.0" },
                                          .nrf52_fw_ver = { "v1.0.0" } };

    http_server_resp_t resp = http_server_resp_401_auth_ruuvi(&hostinfo, HTTP_SERVER_AUTH_TYPE_RUUVI);
    ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(
        "{\"gateway_name\": \"hostname\", \"fw_ver\": \"v1.15.0\", \"nrf52_fw_ver\": \"v1.0.0\", \"lan_auth_type\": "
        "\"lan_auth_ruuvi\", \"lan\": true}",
        string(reinterpret_cast<const char*>(resp.select_location.static_mem.p_buf)));
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_401_auth_ruuvi_with_new_session_id_with_err_message) // NOLINT
{
    const sta_ip_string_t      remote_ip           = { "192.168.1.110" };
    const wifiman_hostinfo_t   hostinfo            = { .hostname     = { "hostname" },
                                                       .fw_ver       = { "v1.15.0" },
                                                       .nrf52_fw_ver = { "v1.0.0" } };
    http_header_extra_fields_t extra_header_fields = { '\0' };

    std::fill(arr_of_random_values.begin(), arr_of_random_values.end(), 0);
    set_random_values(this->arr_of_random_values.data(), this->arr_of_random_values.size());

    const char*        p_err_message = "wrong password";
    http_server_resp_t resp          = http_server_resp_401_auth_ruuvi_with_new_session_id(
        &remote_ip,
        &hostinfo,
        &extra_header_fields,
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        p_err_message);
    ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_NE(
        string::npos,
        string(reinterpret_cast<const char*>(resp.select_location.static_mem.p_buf))
            .find("\"message\": \"wrong password\""));
    ASSERT_NE(
        string::npos,
        string(extra_header_fields.buf).find("WWW-Authenticate: x-ruuvi-interactive realm=\"hostname\""));
    ASSERT_NE(string::npos, string(extra_header_fields.buf).find("session_id=\"AAAAAAAAAAAAAAAA\""));
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_401_auth_ruuvi_with_new_session_id_empty_challenge_returns_503) // NOLINT
{
    const sta_ip_string_t      remote_ip           = { "192.168.1.110" };
    const wifiman_hostinfo_t   hostinfo            = { .hostname     = { "hostname" },
                                                       .fw_ver       = { "v1.15.0" },
                                                       .nrf52_fw_ver = { "v1.0.0" } };
    http_header_extra_fields_t extra_header_fields = { '\0' };

    std::fill(arr_of_random_values.begin(), arr_of_random_values.end(), 0);
    set_random_values(this->arr_of_random_values.data(), this->arr_of_random_values.size());

    g_flag_force_empty_sha256_hex_str = true;

    http_server_resp_t resp = http_server_resp_401_auth_ruuvi_with_new_session_id(
        &remote_ip,
        &hostinfo,
        &extra_header_fields,
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        "err");
    ASSERT_EQ(HTTP_RESP_CODE_503, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_401_auth_digest) // NOLINT
{
    const wifiman_hostinfo_t   hostinfo            = { .hostname     = { "hostname" },
                                                       .fw_ver       = { "v1.15.0" },
                                                       .nrf52_fw_ver = { "v1.0.0" } };
    http_header_extra_fields_t extra_header_fields = { '\0' };

    std::fill(arr_of_random_values.begin(), arr_of_random_values.end(), 0);
    set_random_values(this->arr_of_random_values.data(), this->arr_of_random_values.size());

    http_server_resp_t resp = http_server_resp_401_auth_digest(&hostinfo, &extra_header_fields);
    ASSERT_EQ(HTTP_RESP_CODE_401, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_NE(
        string::npos,
        string(extra_header_fields.buf).find("WWW-Authenticate: Digest realm=\"hostname\" qop=\"auth\" nonce=\""));
    ASSERT_NE(string::npos, string(extra_header_fields.buf).find("\" opaque=\""));
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_401_auth_digest_sha_failure_returns_503) // NOLINT
{
    const wifiman_hostinfo_t   hostinfo            = { .hostname     = { "hostname" },
                                                       .fw_ver       = { "v1.15.0" },
                                                       .nrf52_fw_ver = { "v1.0.0" } };
    http_header_extra_fields_t extra_header_fields = { '\0' };

    std::fill(arr_of_random_values.begin(), arr_of_random_values.end(), 0);
    set_random_values(this->arr_of_random_values.data(), this->arr_of_random_values.size());

    g_flag_force_empty_sha256_calc_hex_str = true;

    http_server_resp_t resp = http_server_resp_401_auth_digest(&hostinfo, &extra_header_fields);
    ASSERT_EQ(HTTP_RESP_CODE_503, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_403_auth_deny) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = { .hostname     = { "hostname" },
                                          .fw_ver       = { "v1.15.0" },
                                          .nrf52_fw_ver = { "v1.0.0" } };

    http_server_resp_t resp = http_server_resp_403_auth_deny(&hostinfo);
    ASSERT_EQ(HTTP_RESP_CODE_403, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_STATIC_MEM, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_APPLICATION_JSON, resp.content_type);
    ASSERT_EQ(
        "{\"gateway_name\": \"hostname\", \"fw_ver\": \"v1.15.0\", \"nrf52_fw_ver\": \"v1.0.0\", \"lan_auth_type\": "
        "\"lan_auth_deny\", \"lan\": true}",
        string(reinterpret_cast<const char*>(resp.select_location.static_mem.p_buf)));
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, resp_403_forbidden) // NOLINT
{
    http_server_resp_t resp = http_server_resp_403_forbidden();
    ASSERT_EQ(HTTP_RESP_CODE_403, resp.http_resp_code);
    ASSERT_EQ(HTTP_CONTENT_LOCATION_NO_CONTENT, resp.content_location);
    ASSERT_TRUE(resp.flag_no_cache);
    ASSERT_TRUE(resp.flag_add_header_date);
    ASSERT_EQ(HTTP_CONTENT_TYPE_TEXT_HTML, resp.content_type);
    ASSERT_EQ(0, resp.content_len);
    http_server_resp_free(&resp);
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, fill_auth_json_bearer) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = { .hostname     = { "hostname" },
                                          .fw_ver       = { "v1.15.0" },
                                          .nrf52_fw_ver = { "v1.0.0" } };

    const http_server_resp_auth_json_t* const p_auth_json = http_server_fill_auth_json_bearer(&hostinfo);
    ASSERT_NE(nullptr, p_auth_json);
    ASSERT_EQ(
        "{\"gateway_name\": \"hostname\", \"fw_ver\": \"v1.15.0\", \"nrf52_fw_ver\": \"v1.0.0\"}",
        string(p_auth_json->buf));
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, fill_auth_json_without_err_message_lan_true) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = { .hostname     = { "mygw" },
                                          .fw_ver       = { "v1.15.0" },
                                          .nrf52_fw_ver = { "v1.0.0" } };

    const http_server_resp_auth_json_t* const p_auth_json = http_server_fill_auth_json(
        &hostinfo,
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        true,
        NULL);
    ASSERT_NE(nullptr, p_auth_json);
    ASSERT_EQ(
        "{\"gateway_name\": \"mygw\", \"fw_ver\": \"v1.15.0\", \"nrf52_fw_ver\": \"v1.0.0\", "
        "\"lan_auth_type\": \"lan_auth_ruuvi\", \"lan\": true}",
        string(p_auth_json->buf));
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, fill_auth_json_without_err_message_lan_false) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = { .hostname     = { "mygw" },
                                          .fw_ver       = { "v1.15.0" },
                                          .nrf52_fw_ver = { "v1.0.0" } };

    const http_server_resp_auth_json_t* const p_auth_json = http_server_fill_auth_json(
        &hostinfo,
        HTTP_SERVER_AUTH_TYPE_DENY,
        false,
        NULL);
    ASSERT_NE(nullptr, p_auth_json);
    ASSERT_EQ(
        "{\"gateway_name\": \"mygw\", \"fw_ver\": \"v1.15.0\", \"nrf52_fw_ver\": \"v1.0.0\", "
        "\"lan_auth_type\": \"lan_auth_deny\", \"lan\": false}",
        string(p_auth_json->buf));
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, fill_auth_json_with_err_message) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = { .hostname     = { "mygw" },
                                          .fw_ver       = { "v1.15.0" },
                                          .nrf52_fw_ver = { "v1.0.0" } };

    const http_server_resp_auth_json_t* const p_auth_json = http_server_fill_auth_json(
        &hostinfo,
        HTTP_SERVER_AUTH_TYPE_RUUVI,
        true,
        "wrong password");
    ASSERT_NE(nullptr, p_auth_json);
    ASSERT_EQ(
        "{\"gateway_name\": \"mygw\", \"fw_ver\": \"v1.15.0\", \"nrf52_fw_ver\": \"v1.0.0\", "
        "\"lan_auth_type\": \"lan_auth_ruuvi\", \"lan\": true, \"message\": \"wrong password\"}",
        string(p_auth_json->buf));
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, fill_auth_json_auth_type_allow) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = { .hostname = { "gw1" }, .fw_ver = { "v2.0.0" }, .nrf52_fw_ver = { "v1.1.0" } };

    const http_server_resp_auth_json_t* const p_auth_json = http_server_fill_auth_json(
        &hostinfo,
        HTTP_SERVER_AUTH_TYPE_ALLOW,
        true,
        NULL);
    ASSERT_NE(nullptr, p_auth_json);
    ASSERT_EQ(
        "{\"gateway_name\": \"gw1\", \"fw_ver\": \"v2.0.0\", \"nrf52_fw_ver\": \"v1.1.0\", "
        "\"lan_auth_type\": \"lan_auth_allow\", \"lan\": true}",
        string(p_auth_json->buf));
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerResp, fill_auth_json_auth_type_digest) // NOLINT
{
    const wifiman_hostinfo_t hostinfo = { .hostname = { "gw1" }, .fw_ver = { "v2.0.0" }, .nrf52_fw_ver = { "v1.1.0" } };

    const http_server_resp_auth_json_t* const p_auth_json = http_server_fill_auth_json(
        &hostinfo,
        HTTP_SERVER_AUTH_TYPE_DIGEST,
        true,
        NULL);
    ASSERT_NE(nullptr, p_auth_json);
    ASSERT_EQ(
        "{\"gateway_name\": \"gw1\", \"fw_ver\": \"v2.0.0\", \"nrf52_fw_ver\": \"v1.1.0\", "
        "\"lan_auth_type\": \"lan_auth_digest\", \"lan\": true}",
        string(p_auth_json->buf));
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}
