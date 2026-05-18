/**
 * @file test_http_server_handle_req.cpp
 * @author TheSomeMan
 * @date 2026-05-07
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gtest/gtest.h"
#include "http_server_handle_req.h"
#include <string>
#include <vector>
#include <cstring>
#include <cassert>
#include "http_server_handle_req_get_auth.h"
#include "http_server_handle_req_post_auth.h"
#include "http_server_handle_req_delete_auth.h"
#include "http_server_ecdh.h"
#include "http_server_internal.h"
#include "wifi_manager_internal.h"
#include "wifiman_config.h"
#include "json_network_info.h"
#include "str_buf.h"
#include "esp_log_wrapper.hpp"
#include "os_malloc.h"
#include "os_task.h"

using namespace std;

/*** Types for stub configuration *********************************************************************************/

enum decrypt_mode_e
{
    DECRYPT_FAIL = 0,
    DECRYPT_OK_NULL_BUF,
    DECRYPT_OK_SMALL,
    DECRYPT_OK_TOO_LARGE,
};

/*** Google-test class implementation *********************************************************************************/

static http_server_resp_t
make_resp(const http_resp_code_e code)
{
    http_server_resp_t resp = {};
    resp.http_resp_code     = code;
    resp.content_location   = HTTP_CONTENT_LOCATION_NO_CONTENT;
    resp.content_type       = HTTP_CONTENT_TYPE_APPLICATION_JSON;
    return resp;
}

class TestHttpServerHandleReq;
static TestHttpServerHandleReq* g_pTestClass;

class TestHttpServerHandleReq : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        os_malloc_trace_init();
        esp_log_wrapper_init();
        g_pTestClass = this;

        this->m_mutex_try_lock_result = true;
        this->m_decrypt_mode          = DECRYPT_FAIL;
        this->m_post_called           = false;
        this->m_post_path.clear();
        this->m_post_body_len     = 0;
        this->m_post_resp         = make_resp(HTTP_RESP_CODE_200);
        this->m_vTaskDelay_called = false;
        this->m_vTaskDelay_ticks  = 0;
        this->m_tick_values.clear();
        this->m_tick_idx                    = 0;
        this->m_alloc_call_cnt              = 0;
        this->m_alloc_fail_on_call_idx      = -1;
        this->m_alloc_free_call_count       = 0;
        this->m_flag_alloc_counting_enabled = false;

        esp_log_wrapper_clear();
        this->m_flag_alloc_counting_enabled = true;
    }

    void
    TearDown() override
    {
        this->m_flag_alloc_counting_enabled = false;
        this->m_alloc_free_call_count       = 0;
        os_malloc_trace_deinit();
        g_pTestClass = nullptr;
        esp_log_wrapper_deinit();
    }

    http_server_resp_t
    call_req(
        const string& method,
        const string& path,
        const string& headers,
        const string& body,
        const bool    flag_access_from_lan = false)
    {
        const http_req_info_t req_info = {
            .is_success      = true,
            .http_cmd        = { .ptr = method.c_str() },
            .http_uri        = { .ptr = path.c_str() },
            .http_uri_params = { .ptr = NULL },
            .http_ver        = { .ptr = "HTTP/1.1" },
            .http_header     = { .ptr = headers.c_str() },
            .http_body       = { .ptr = body.c_str() },
        };
        const sta_ip_string_t         remote_ip = { .buf = "10.0.0.2" };
        const http_server_auth_info_t auth_info = {
            .auth_type = HTTP_SERVER_AUTH_TYPE_ALLOW,
        };
        const http_server_handle_req_param_t param = {
            .p_req_info           = &req_info,
            .p_remote_ip          = &remote_ip,
            .p_auth_info          = &auth_info,
            .flag_access_from_lan = flag_access_from_lan,
        };
        http_header_extra_fields_t extra = { 0 };
        return http_server_handle_req(&param, &extra);
    }

    http_server_resp_t
    call_post(const string& path, const string& headers, const string& body, const bool flag_access_from_lan = false)
    {
        return this->call_req("POST", path, headers, body, flag_access_from_lan);
    }

    http_server_resp_t
    call_get(const string& path, const string& headers = "", const bool flag_access_from_lan = false)
    {
        return this->call_req("GET", path, headers, "", flag_access_from_lan);
    }

    http_server_resp_t
    call_delete(const string& path, const string& headers = "", const bool flag_access_from_lan = false)
    {
        return this->call_req("DELETE", path, headers, "", flag_access_from_lan);
    }

public:
    // Mutex stub config
    bool m_mutex_try_lock_result;

    // Decrypt stub config
    decrypt_mode_e m_decrypt_mode;

    // POST callback captures
    bool               m_post_called;
    string             m_post_path;
    size_t             m_post_body_len;
    http_server_resp_t m_post_resp;

    // vTaskDelay tracking
    bool       m_vTaskDelay_called;
    TickType_t m_vTaskDelay_ticks;

    // Tick count stub config
    vector<TickType_t> m_tick_values;
    size_t             m_tick_idx;

    bool m_flag_alloc_counting_enabled;
    int  m_alloc_free_call_count;
    int  m_alloc_call_cnt;
    int  m_alloc_fail_on_call_idx;

    TestHttpServerHandleReq();

    ~TestHttpServerHandleReq() override;
};

TestHttpServerHandleReq::TestHttpServerHandleReq()
    : Test()
    , m_mutex_try_lock_result(true)
    , m_decrypt_mode(DECRYPT_FAIL)
    , m_post_called(false)
    , m_post_body_len(0)
    , m_post_resp({})
    , m_vTaskDelay_called(false)
    , m_vTaskDelay_ticks(0)
    , m_tick_idx(0)
    , m_flag_alloc_counting_enabled(false)
    , m_alloc_free_call_count(0)
    , m_alloc_call_cnt(0)
    , m_alloc_fail_on_call_idx(-1)
{
}

TestHttpServerHandleReq::~TestHttpServerHandleReq() = default;

#ifdef __cplusplus
extern "C" {
#endif

void*
__wrap_malloc(size_t size)
{
    extern void* __real_malloc(size_t _size) __THROW __attribute_malloc__ __attribute_alloc_size__((1)) __wur;
    if (g_pTestClass && g_pTestClass->m_alloc_fail_on_call_idx >= 0)
    {
        g_pTestClass->m_alloc_call_cnt += 1;
        if (g_pTestClass->m_alloc_call_cnt == g_pTestClass->m_alloc_fail_on_call_idx)
        {
            return nullptr;
        }
    }
    if (g_pTestClass && g_pTestClass->m_flag_alloc_counting_enabled)
    {
        g_pTestClass->m_alloc_free_call_count += 1;
    }
    return __real_malloc(size);
}

void*
__wrap_calloc(size_t nmemb, size_t size)
{
    extern void*                     __real_calloc(size_t _nmemb, size_t _size)
        __THROW __attribute_malloc__ __attribute_alloc_size__((1, 2)) __wur;
    if (g_pTestClass && g_pTestClass->m_alloc_fail_on_call_idx >= 0)
    {
        g_pTestClass->m_alloc_call_cnt += 1;
        if (g_pTestClass->m_alloc_call_cnt == g_pTestClass->m_alloc_fail_on_call_idx)
        {
            return nullptr;
        }
    }
    if (g_pTestClass && g_pTestClass->m_flag_alloc_counting_enabled)
    {
        g_pTestClass->m_alloc_free_call_count += 1;
    }
    return __real_calloc(nmemb, size);
}

void
__wrap_free(void* ptr)
{
    extern void __real_free(void* _ptr) __THROW;
    if (g_pTestClass && g_pTestClass->m_flag_alloc_counting_enabled)
    {
        g_pTestClass->m_alloc_free_call_count -= 1;
    }
    __real_free(ptr);
}

const char*
os_task_get_name(void)
{
    static const char g_task_name[] = "main";
    return const_cast<char*>(g_task_name);
}

os_task_priority_t
os_task_get_priority(void)
{
    return 0;
}

os_mutex_t
os_mutex_create_static(os_mutex_static_t* const p_mutex_static)
{
    return reinterpret_cast<os_mutex_t>(p_mutex_static);
}

void
os_mutex_delete(os_mutex_t* const ph_mutex)
{
    *ph_mutex = nullptr;
}

void
os_mutex_lock(os_mutex_t const h_mutex)
{
    (void)h_mutex;
}

bool
os_mutex_try_lock(os_mutex_t const h_mutex)
{
    (void)h_mutex;
    if (nullptr != g_pTestClass)
    {
        return g_pTestClass->m_mutex_try_lock_result;
    }
    return true;
}

bool
os_mutex_lock_with_timeout(os_mutex_t const h_mutex, const os_delta_ticks_t ticks_to_wait)
{
    (void)h_mutex;
    (void)ticks_to_wait;
    return true;
}

void
os_mutex_unlock(os_mutex_t const h_mutex)
{
    (void)h_mutex;
}

TickType_t
xTaskGetTickCount(void)
{
    if (nullptr != g_pTestClass)
    {
        if (!g_pTestClass->m_tick_values.empty())
        {
            if (g_pTestClass->m_tick_idx >= g_pTestClass->m_tick_values.size())
            {
                assert(g_pTestClass->m_tick_idx < g_pTestClass->m_tick_values.size());
            }
            return g_pTestClass->m_tick_values[g_pTestClass->m_tick_idx++];
        }
    }
    return 0;
}

void
vTaskDelay(const TickType_t xTicksToDelay)
{
    if (nullptr != g_pTestClass)
    {
        g_pTestClass->m_vTaskDelay_called = true;
        g_pTestClass->m_vTaskDelay_ticks  = xTicksToDelay;
    }
}

http_server_resp_t
http_server_handle_req_check_auth(
    const http_server_handle_req_auth_param_t* const p_param,
    http_header_extra_fields_t* const                p_extra_header_fields,
    bool* const                                      p_flag_access_by_bearer_token)
{
    (void)p_param;
    (void)p_extra_header_fields;
    if (NULL != p_flag_access_by_bearer_token)
    {
        *p_flag_access_by_bearer_token = false;
    }
    return make_resp(HTTP_RESP_CODE_200);
}

http_server_resp_t
http_server_handle_req_get_auth(
    const http_server_handle_req_auth_param_t* const p_param,
    http_header_extra_fields_t* const                p_extra_header_fields)
{
    (void)p_param;
    (void)p_extra_header_fields;
    return make_resp(HTTP_RESP_CODE_200);
}

http_server_resp_t
http_server_handle_req_post_auth(
    const bool                           flag_access_from_lan,
    const http_req_header_t              http_header,
    const sta_ip_string_t* const         p_remote_ip,
    const http_req_body_t                http_body,
    const http_server_auth_info_t* const p_auth_info,
    const wifiman_hostinfo_t* const      p_hostinfo,
    http_header_extra_fields_t* const    p_extra_header_fields)
{
    (void)flag_access_from_lan;
    (void)http_header;
    (void)p_remote_ip;
    (void)http_body;
    (void)p_auth_info;
    (void)p_hostinfo;
    (void)p_extra_header_fields;
    return make_resp(HTTP_RESP_CODE_200);
}

http_server_resp_t
http_server_handle_req_delete_auth(
    const http_req_header_t              http_header,
    const sta_ip_string_t* const         p_remote_ip,
    const http_server_auth_info_t* const p_auth_info,
    const wifiman_hostinfo_t* const      p_hostinfo)
{
    (void)http_header;
    (void)p_remote_ip;
    (void)p_auth_info;
    (void)p_hostinfo;
    return make_resp(HTTP_RESP_CODE_200);
}

http_server_resp_t
wifi_manager_cb_on_http_get(
    const char* const               p_path,
    const char* const               p_uri_params,
    const bool                      flag_access_from_lan,
    const http_server_resp_t* const p_resp_auth)
{
    (void)p_path;
    (void)p_uri_params;
    (void)flag_access_from_lan;
    (void)p_resp_auth;
    return make_resp(HTTP_RESP_CODE_404);
}

http_server_resp_t
wifi_manager_cb_on_http_post(
    const char* const     p_path,
    const char* const     p_uri_params,
    const http_req_body_t http_body,
    const bool            flag_access_from_lan)
{
    (void)p_uri_params;
    (void)flag_access_from_lan;
    if (nullptr != g_pTestClass)
    {
        g_pTestClass->m_post_called   = true;
        g_pTestClass->m_post_path     = (NULL != p_path) ? p_path : "";
        g_pTestClass->m_post_body_len = (NULL != http_body.ptr) ? strlen(http_body.ptr) : 0;
        return g_pTestClass->m_post_resp;
    }
    return make_resp(HTTP_RESP_CODE_500);
}

http_server_resp_t
wifi_manager_cb_on_http_delete(
    const char* const               p_path,
    const char* const               p_uri_params,
    const bool                      flag_access_from_lan,
    const http_server_resp_t* const p_resp_auth)
{
    (void)p_path;
    (void)p_uri_params;
    (void)flag_access_from_lan;
    (void)p_resp_auth;
    return make_resp(HTTP_RESP_CODE_404);
}

wifiman_hostinfo_t
wifiman_config_sta_get_hostinfo(void)
{
    const wifiman_hostinfo_t hostinfo = {
        .hostname     = { .buf = "RuuviGateway" },
        .fw_ver       = { .buf = "1.0.0" },
        .nrf52_fw_ver = { .buf = "1.0.0" },
    };
    return hostinfo;
}

wifiman_wifi_ssid_t
wifiman_config_sta_get_ssid(void)
{
    const wifiman_wifi_ssid_t ssid = {
        .ssid_buf = "ssid",
    };
    return ssid;
}

void
wifiman_config_sta_set_ssid_and_password(
    const wifiman_wifi_ssid_t* const     p_ssid,
    const wifiman_wifi_password_t* const p_password)
{
    (void)p_ssid;
    (void)p_password;
}

void
json_network_info_do_const_action_with_timeout(
    json_network_info_do_const_action_callback_t cb_func,
    void* const                                  p_param,
    const os_delta_ticks_t                       ticks_to_wait)
{
    (void)cb_func;
    (void)p_param;
    (void)ticks_to_wait;
}

void
json_network_info_do_generate_internal(
    const json_network_info_t* const      p_info,
    http_server_resp_status_json_t* const p_resp_status_json)
{
    (void)p_info;
    (void)p_resp_status_json;
}

void
json_network_info_set_reason_user_disconnect(void)
{
}
void
json_network_info_clear(void)
{
}
void
wifi_manager_cb_on_request_status_json(void)
{
}
void
wifi_manager_lock(void)
{
}
void
wifi_manager_unlock(void)
{
}
bool
wifi_manager_is_connected_to_ethernet(void)
{
    return false;
}
void
wifi_manager_disconnect_eth(void)
{
}
void
wifi_manager_disconnect_wifi(void)
{
}
void
wifi_manager_connect_async(void)
{
}
void
wifi_manager_enable_wps(void)
{
}
void
wifi_manager_disable_wps(void)
{
}
const char*
wifi_manager_scan_sync(void)
{
    return NULL;
}
void
dns_server_stop(void)
{
}
bool
wifiman_msg_send_cmd_connect_eth(void)
{
    return true;
}

http_server_resp_t
http_server_resp_302(void)
{
    return make_resp(HTTP_RESP_CODE_302);
}

http_server_resp_t
http_server_resp_503(void)
{
    return make_resp(HTTP_RESP_CODE_503);
}

http_server_resp_t
http_server_resp_200_json(const char* p_json_content)
{
    (void)p_json_content;
    return make_resp(HTTP_RESP_CODE_200);
}

http_server_resp_t
http_server_resp_200_json_in_heap(const char* const p_json_content)
{
    (void)p_json_content;
    return make_resp(HTTP_RESP_CODE_200);
}

http_server_resp_t
http_server_resp_400(void)
{
    return make_resp(HTTP_RESP_CODE_400);
}

http_server_resp_t
http_server_resp_500(void)
{
    return make_resp(HTTP_RESP_CODE_500);
}

http_server_resp_t
http_server_resp_403_forbidden(void)
{
    return make_resp(HTTP_RESP_CODE_403);
}

bool
http_server_ecdh_handshake(
    const http_server_ecdh_pub_key_b64_t* const p_pub_key_b64_cli,
    http_server_ecdh_pub_key_b64_t* const       p_pub_key_b64_srv)
{
    (void)p_pub_key_b64_cli;
    (void)p_pub_key_b64_srv;
    return true;
}

bool
http_server_ecdh_decrypt(const http_server_ecdh_encrypted_req_t* const p_enc_req, str_buf_t* const p_str_buf)
{
    (void)p_enc_req;
    const decrypt_mode_e mode = (nullptr != g_pTestClass) ? g_pTestClass->m_decrypt_mode : DECRYPT_FAIL;
    switch (mode)
    {
        case DECRYPT_FAIL:
            return false;
        case DECRYPT_OK_NULL_BUF:
            p_str_buf->buf  = NULL;
            p_str_buf->size = 0;
            p_str_buf->idx  = 0;
            return true;
        case DECRYPT_OK_SMALL:
        {
            const size_t n = 32;
            p_str_buf->buf = (char*)os_malloc(n + 1);
            memset(p_str_buf->buf, 'a', n);
            p_str_buf->buf[n] = '\0';
            p_str_buf->size   = n + 1;
            p_str_buf->idx    = n;
            return true;
        }
        case DECRYPT_OK_TOO_LARGE:
        {
            const size_t n = HTTP_SERVER_MAX_UNENCRYPTED_CONTENT_SIZE + 1;
            p_str_buf->buf = (char*)os_malloc(n + 1);
            memset(p_str_buf->buf, 'a', n);
            p_str_buf->buf[n] = '\0';
            p_str_buf->size   = n + 1;
            p_str_buf->idx    = n;
            return true;
        }
    }
    return false;
}

#ifdef __cplusplus
}
#endif

#define TEST_CHECK_LOG_RECORD(level_, msg_) ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("http_server", level_, msg_)

/*** Unit-Tests *******************************************************************************************************/

// ===== POST: unencrypted content size checks =====

TEST_F(TestHttpServerHandleReq, post_unencrypted_too_large_returns_400) // NOLINT
{
    const string             body(HTTP_SERVER_MAX_UNENCRYPTED_CONTENT_SIZE + 1, 'x');
    const http_server_resp_t resp = this->call_post("/custom.json", "", body);

    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    ASSERT_FALSE(this->m_post_called);
    ASSERT_FALSE(this->m_vTaskDelay_called);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "Content size 8193 exceeds maximum allowed 8192");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerHandleReq, post_unencrypted_at_limit_routes_to_callback) // NOLINT
{
    this->m_post_resp = make_resp(HTTP_RESP_CODE_200);
    const string body(HTTP_SERVER_MAX_UNENCRYPTED_CONTENT_SIZE, 'x');

    const http_server_resp_t resp = this->call_post("/custom.json", "", body);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_TRUE(this->m_post_called);
    ASSERT_EQ("custom.json", this->m_post_path);
    ASSERT_EQ(body.size(), this->m_post_body_len);
    ASSERT_TRUE(this->m_vTaskDelay_called);
    ASSERT_EQ(pdMS_TO_TICKS(1000), this->m_vTaskDelay_ticks);
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "POST /custom.json, params=");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== POST: encrypted content =====

TEST_F(TestHttpServerHandleReq, post_encrypted_decrypt_fails_returns_400) // NOLINT
{
    this->m_decrypt_mode = DECRYPT_FAIL;
    const string headers = "Ruuvi-Ecdh-Encrypted: true\r\n";
    const string body    = "{\"encrypted\":\"x\",\"iv\":\"x\",\"hash\":\"x\"}";

    const http_server_resp_t resp = this->call_post("/custom.json", headers, body);

    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    ASSERT_FALSE(this->m_post_called);
    ASSERT_FALSE(this->m_vTaskDelay_called);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "Failed to decrypt request");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "http_server_decrypt failed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerHandleReq, post_encrypted_decrypts_to_null_buf_returns_400) // NOLINT
{
    this->m_decrypt_mode = DECRYPT_OK_NULL_BUF;
    const string headers = "Ruuvi-Ecdh-Encrypted: true\r\n";
    const string body    = "{\"encrypted\":\"x\",\"iv\":\"x\",\"hash\":\"x\"}";

    const http_server_resp_t resp = this->call_post("/custom.json", headers, body);

    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    ASSERT_FALSE(this->m_post_called);
    ASSERT_FALSE(this->m_vTaskDelay_called);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "http_server_decrypt failed (no mem)");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerHandleReq, post_encrypted_decrypts_to_too_large_returns_400) // NOLINT
{
    this->m_decrypt_mode = DECRYPT_OK_TOO_LARGE;
    const string headers = "Ruuvi-Ecdh-Encrypted: true\r\n";
    const string body    = "{\"encrypted\":\"x\",\"iv\":\"x\",\"hash\":\"x\"}";

    const http_server_resp_t resp = this->call_post("/custom.json", headers, body);

    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    ASSERT_FALSE(this->m_post_called);
    ASSERT_FALSE(this->m_vTaskDelay_called);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "Decrypted content size 8193 exceeds maximum allowed 8192");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerHandleReq, post_encrypted_decrypts_ok_routes_to_callback) // NOLINT
{
    this->m_decrypt_mode = DECRYPT_OK_SMALL;
    this->m_post_resp    = make_resp(HTTP_RESP_CODE_200);
    const string headers = "Ruuvi-Ecdh-Encrypted: true\r\n";
    const string body    = "{\"encrypted\":\"x\",\"iv\":\"x\",\"hash\":\"x\"}";

    const http_server_resp_t resp = this->call_post("/custom.json", headers, body);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_TRUE(this->m_post_called);
    ASSERT_EQ("custom.json", this->m_post_path);
    ASSERT_EQ(32U, this->m_post_body_len);
    ASSERT_TRUE(this->m_vTaskDelay_called);
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "POST /custom.json, params=");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== POST: connect.json =====

TEST_F(TestHttpServerHandleReq, post_connect_json_with_ssid_and_password_calls_connect) // NOLINT
{
    const string body = "{\"ssid\":\"mySSID\",\"password\":\"myPass\"}";

    const http_server_resp_t resp = this->call_post("/connect.json", "", body);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_TRUE(this->m_vTaskDelay_called);
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "POST /connect.json, params=");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "http_server_netconn_serve: POST /connect.json");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "POST /connect.json: SSID:mySSID, PWD: ******** - connect to WiFi");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerHandleReq, post_connect_json_with_null_ssid_and_null_password_connects_ethernet) // NOLINT
{
    const string body = "{\"ssid\":null,\"password\":null}";

    const http_server_resp_t resp = this->call_post("/connect.json", "", body);

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_TRUE(this->m_vTaskDelay_called);
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "POST /connect.json, params=");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "http_server_netconn_serve: POST /connect.json");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "POST /connect.json: SSID:NULL, PWD: NULL - connect to Ethernet");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerHandleReq, post_connect_json_with_null_ssid_only_returns_400) // NOLINT
{
    const string body = "{\"ssid\":null,\"password\":\"myPass\"}";

    const http_server_resp_t resp = this->call_post("/connect.json", "", body);

    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    ASSERT_TRUE(this->m_vTaskDelay_called);
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "POST /connect.json, params=");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "http_server_netconn_serve: POST /connect.json");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerHandleReq, post_connect_json_with_invalid_json_returns_400) // NOLINT
{
    const string body = "not-valid-json";

    const http_server_resp_t resp = this->call_post("/connect.json", "", body);

    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    ASSERT_TRUE(this->m_vTaskDelay_called);
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "POST /connect.json, params=");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "http_server_netconn_serve: POST /connect.json");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "connect.json: Failed to parse decrypted content or no memory");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerHandleReq, post_connect_json_from_lan_returns_403) // NOLINT
{
    const string body = "{\"ssid\":\"mySSID\",\"password\":\"myPass\"}";

    const http_server_resp_t resp = this->call_post("/connect.json", "", body, true);

    ASSERT_EQ(HTTP_RESP_CODE_403, resp.http_resp_code);
    ASSERT_TRUE(this->m_vTaskDelay_called);
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "POST /connect.json, params=");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "POST /connect.json - access from LAN is not allowed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== POST: connect_wps =====

TEST_F(TestHttpServerHandleReq, post_connect_wps_returns_200) // NOLINT
{
    const http_server_resp_t resp = this->call_post("/connect_wps", "", "");

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    ASSERT_TRUE(this->m_vTaskDelay_called);
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "POST /connect_wps, params=");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "http_server_netconn_serve: POST /connect_wps");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerHandleReq, post_connect_wps_from_lan_returns_403) // NOLINT
{
    const http_server_resp_t resp = this->call_post("/connect_wps", "", "", true);

    ASSERT_EQ(HTTP_RESP_CODE_403, resp.http_resp_code);
    ASSERT_TRUE(this->m_vTaskDelay_called);
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "POST /connect_wps, params=");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "POST /connect_wps - access from LAN is not allowed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== GET =====

TEST_F(TestHttpServerHandleReq, get_unknown_path_routes_to_callback_returning_404) // NOLINT
{
    const http_server_resp_t resp = this->call_get("/unknown_file.html");

    ASSERT_EQ(HTTP_RESP_CODE_404, resp.http_resp_code);
    ASSERT_FALSE(this->m_vTaskDelay_called);
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== DELETE =====

TEST_F(TestHttpServerHandleReq, delete_connect_json_triggers_disconnect) // NOLINT
{
    const http_server_resp_t resp = this->call_delete("/connect.json");

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "DELETE /connect.json, params=");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "http_server_netconn_serve: DELETE /connect.json");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerHandleReq, delete_connect_json_from_lan_returns_403) // NOLINT
{
    const http_server_resp_t resp = this->call_delete("/connect.json", "", true);

    ASSERT_EQ(HTTP_RESP_CODE_403, resp.http_resp_code);
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "DELETE /connect.json, params=");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "http_server_netconn_serve: DELETE /connect.json");
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_ERROR,
        "http_server_netconn_serve: DELETE /connect.json - access from LAN is not allowed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerHandleReq, delete_auth_routes_to_delete_auth_handler) // NOLINT
{
    const http_server_resp_t resp = this->call_delete("/auth");

    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "DELETE /auth, params=");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== Unknown HTTP method =====

TEST_F(TestHttpServerHandleReq, unknown_method_returns_400) // NOLINT
{
    const http_server_resp_t resp = this->call_req("PUT", "/custom.json", "", "{}");

    ASSERT_EQ(HTTP_RESP_CODE_400, resp.http_resp_code);
    ASSERT_FALSE(this->m_vTaskDelay_called);
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}
