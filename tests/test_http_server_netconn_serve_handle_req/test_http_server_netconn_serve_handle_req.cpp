/**
 * @file test_http_server_netconn_serve_handle_req.cpp
 * @author TheSomeMan
 * @date 2026-05-08
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gtest/gtest.h"
#include "http_server_netconn_serve_handle_req.h"
#include <string>
#include <vector>
#include <cstring>
#include <cassert>
#include "esp_err.h"
#include "os_task.h"
#include "os_mutex.h"
#include "os_malloc.h"
#include "esp_log_wrapper.hpp"
#include "http_server_auth.h"
#include "http_server_handle_req.h"
#include "wifi_manager_defs.h"

using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

http_header_extra_fields_t g_http_server_extra_header_fields;

#ifdef __cplusplus
}
#endif

class TestHttpServerNetconnServeHandleReq;
static TestHttpServerNetconnServeHandleReq* g_pTestClass;

/*** Google-test class implementation *********************************************************************************/

class TestHttpServerNetconnServeHandleReq : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        os_malloc_trace_init();
        esp_log_wrapper_init();
        g_pTestClass = this;

        memset(&g_http_server_extra_header_fields, 0, sizeof(g_http_server_extra_header_fields));
        memset(&this->m_auth_info, 0, sizeof(this->m_auth_info));

        this->m_p_conn = static_cast<struct netconn*>(calloc(1, sizeof(struct netconn)));
        assert(nullptr != this->m_p_conn);

        this->m_mutex_try_lock_result = true;

        this->m_lan_blocked = false;

        memset(&this->m_handle_req_resp, 0, sizeof(this->m_handle_req_resp));
        this->m_handle_req_called        = false;
        this->m_handle_req_flag_from_lan = false;
        this->m_extra_header_to_set      = "";

        this->m_resp_called     = false;
        this->m_resp_hostname   = "";
        this->m_resp_302_called = false;
        this->m_resp_400_called = false;
        this->m_resp_500_called = false;
        this->m_resp_503_called = false;

        esp_log_wrapper_clear();

        this->m_alloc_free_call_count       = 0;
        this->m_flag_alloc_counting_enabled = true;
    }

    void
    TearDown() override
    {
        this->m_flag_alloc_counting_enabled = false;
        this->m_alloc_free_call_count       = 0;
        os_malloc_trace_deinit();
        free(this->m_p_conn);
        this->m_p_conn = nullptr;
        g_pTestClass   = nullptr;
        esp_log_wrapper_deinit();
    }

public:
    struct netconn* m_p_conn;

    // Mutex stub config
    bool m_mutex_try_lock_result;

    http_server_auth_info_t m_auth_info;

    // wifi_manager stub config
    bool m_lan_blocked;

    // http_server_handle_req stub config
    http_server_resp_t m_handle_req_resp;
    bool               m_handle_req_called;
    bool               m_handle_req_flag_from_lan;
    string             m_extra_header_to_set;

    // Response tracking
    bool   m_resp_called;
    string m_resp_hostname;
    bool   m_resp_302_called;
    bool   m_resp_400_called;
    bool   m_resp_500_called;
    bool   m_resp_503_called;

    bool m_flag_alloc_counting_enabled;
    int  m_alloc_free_call_count;
    int  m_alloc_call_cnt;
    int  m_alloc_fail_on_call_idx;

    TestHttpServerNetconnServeHandleReq();

    ~TestHttpServerNetconnServeHandleReq() override;
};

TestHttpServerNetconnServeHandleReq::TestHttpServerNetconnServeHandleReq()
    : Test()
    , m_p_conn(nullptr)
    , m_mutex_try_lock_result(true)
    , m_auth_info()
    , m_lan_blocked(false)
    , m_handle_req_resp()
    , m_handle_req_called(false)
    , m_handle_req_flag_from_lan(false)
    , m_resp_called(false)
    , m_resp_302_called(false)
    , m_resp_400_called(false)
    , m_resp_500_called(false)
    , m_resp_503_called(false)
    , m_flag_alloc_counting_enabled(false)
    , m_alloc_free_call_count(0)
    , m_alloc_call_cnt(0)
    , m_alloc_fail_on_call_idx(-1)
{
}

TestHttpServerNetconnServeHandleReq::~TestHttpServerNetconnServeHandleReq() = default;

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
}

bool
os_mutex_try_lock(os_mutex_t const h_mutex)
{
    if (nullptr != g_pTestClass)
    {
        return g_pTestClass->m_mutex_try_lock_result;
    }
    return true;
}

bool
os_mutex_lock_with_timeout(os_mutex_t const h_mutex, const os_delta_ticks_t ticks_to_wait)
{
    return true;
}

void
os_mutex_unlock(os_mutex_t const h_mutex)
{
}

const char*
wrap_esp_err_to_name_r(const esp_err_t code, char* const p_buf, const size_t buf_len)
{
    (void)snprintf(p_buf, buf_len, "Unknows");
    return p_buf;
}

wifiman_ip4_addr_str_t
wifiman_config_ap_get_ip_str(void)
{
    wifiman_ip4_addr_str_t ip_str = {
        .buf = "192.168.1.114",
    };
    return ip_str;
}

bool
wifi_manager_is_req_from_lan_blocked_while_ap_is_active(void)
{
    if (nullptr != g_pTestClass)
    {
        return g_pTestClass->m_lan_blocked;
    }
    return false;
}

http_server_auth_info_t*
http_server_get_auth(void)
{
    return &g_pTestClass->m_auth_info;
}

http_server_resp_t
http_server_handle_req(
    const http_server_handle_req_param_t* const p_param,
    http_header_extra_fields_t* const           p_extra_header_fields)
{
    if (nullptr != g_pTestClass)
    {
        g_pTestClass->m_handle_req_called        = true;
        g_pTestClass->m_handle_req_flag_from_lan = p_param->flag_access_from_lan;
        if (!g_pTestClass->m_extra_header_to_set.empty())
        {
            snprintf(
                p_extra_header_fields->buf,
                sizeof(p_extra_header_fields->buf),
                "%s",
                g_pTestClass->m_extra_header_to_set.c_str());
        }
        return g_pTestClass->m_handle_req_resp;
    }
    http_server_resp_t resp = {};
    return resp;
}

void
http_server_netconn_resp(struct netconn* const p_conn, http_server_resp_t* const p_resp, const char* const p_hostname)
{
    if (nullptr != g_pTestClass)
    {
        g_pTestClass->m_resp_called   = true;
        g_pTestClass->m_resp_hostname = (nullptr != p_hostname) ? string(p_hostname) : "";
    }
    if (HTTP_CONTENT_LOCATION_HEAP == p_resp->content_location)
    {
        os_free(p_resp->select_location.heap.p_buf);
        p_resp->select_location.heap.p_buf = nullptr;
    }
}

void
http_server_netconn_resp_302(struct netconn* const p_conn)
{
    if (nullptr != g_pTestClass)
    {
        g_pTestClass->m_resp_302_called = true;
    }
}

void
http_server_netconn_resp_400(struct netconn* const p_conn, http_server_resp_t* const p_resp)
{
    if (nullptr != g_pTestClass)
    {
        g_pTestClass->m_resp_400_called = true;
    }
}

void
http_server_netconn_resp_500(struct netconn* const p_conn, http_server_resp_t* const p_resp)
{
    if (nullptr != g_pTestClass)
    {
        g_pTestClass->m_resp_500_called = true;
    }
}

void
http_server_netconn_resp_503(struct netconn* const p_conn, http_server_resp_t* const p_resp)
{
    if (nullptr != g_pTestClass)
    {
        g_pTestClass->m_resp_503_called = true;
    }
}

#ifdef __cplusplus
}
#endif

#define TEST_CHECK_LOG_RECORD(level_, msg_) ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("http_server", level_, msg_)

/*** Unit-Tests *******************************************************************************************************/

// ===== Parse failure =====

TEST_F(TestHttpServerNetconnServeHandleReq, test_parse_failure_returns_400) // NOLINT
{
    // Request without \r\n\r\n → http_req_parse returns is_success=false
    char            req_buf[]     = "malformed request";
    sta_ip_string_t local_ip_str  = { .buf = "10.0.0.1" };
    sta_ip_string_t remote_ip_str = { .buf = "10.0.0.2" };

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    ASSERT_TRUE(this->m_resp_400_called);
    ASSERT_FALSE(this->m_handle_req_called);
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_ERROR,
        "Request from 10.0.0.2 to 10.0.0.1: failed to parse request: malformed request");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== LAN access paths =====

TEST_F(TestHttpServerNetconnServeHandleReq, test_lan_access_blocked_returns_503) // NOLINT
{
    // local_ip != AP IP → LAN access; wifi_manager says blocked
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: 10.0.0.1\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "10.0.0.1" };
    sta_ip_string_t remote_ip_str = { .buf = "10.0.0.2" };

    this->m_lan_blocked = true;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    ASSERT_TRUE(this->m_resp_503_called);
    ASSERT_FALSE(this->m_handle_req_called);
    // INFO log for the request
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 10.0.0.2 to 10.0.0.1 (Host: 10.0.0.1): GET /");
    // WARN log about blocked LAN
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Request from LAN while WiFi hotspot is active - return HTTP error 503");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReq, test_lan_access_not_blocked_normal_handling) // NOLINT
{
    // local_ip != AP IP → LAN access; not blocked → normal handling
    char            req_buf[]     = "GET /status HTTP/1.1\r\nHost: 10.0.0.1\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "10.0.0.1" };
    sta_ip_string_t remote_ip_str = { .buf = "10.0.0.2" };

    this->m_lan_blocked                      = false;
    this->m_handle_req_resp.http_resp_code   = HTTP_RESP_CODE_200;
    this->m_handle_req_resp.content_type     = HTTP_CONTENT_TYPE_TEXT_HTML;
    this->m_handle_req_resp.content_location = HTTP_CONTENT_LOCATION_NO_CONTENT;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    ASSERT_TRUE(this->m_handle_req_called);
    ASSERT_TRUE(this->m_handle_req_flag_from_lan);
    ASSERT_TRUE(this->m_resp_called);
    ASSERT_EQ("10.0.0.1", this->m_resp_hostname);
    // INFO log for the request
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 10.0.0.2 to 10.0.0.1 (Host: 10.0.0.1): GET /status");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== AP access / captive portal paths =====

TEST_F(TestHttpServerNetconnServeHandleReq, test_ap_access_captive_portal_redirect) // NOLINT
{
    // local_ip == AP IP → AP access; Host doesn't contain AP IP → redirect
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: some.host.com\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    ASSERT_TRUE(this->m_resp_302_called);
    ASSERT_FALSE(this->m_handle_req_called);
    // INFO log for the request
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: some.host.com): GET /");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReq, test_ap_access_no_host_header_redirect) // NOLINT
{
    // local_ip == AP IP; no Host header → host_len=0 → is_request_to_ap_ip=false → redirect
    char            req_buf[]     = "GET / HTTP/1.1\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    ASSERT_TRUE(this->m_resp_302_called);
    ASSERT_FALSE(this->m_handle_req_called);
    // INFO log: Host is empty since no Host header
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: ): GET /");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReq, test_ap_access_host_matches_ap_ip_normal) // NOLINT
{
    // local_ip == AP IP; Host contains AP IP → normal handling
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    this->m_handle_req_resp.http_resp_code   = HTTP_RESP_CODE_200;
    this->m_handle_req_resp.content_type     = HTTP_CONTENT_TYPE_TEXT_HTML;
    this->m_handle_req_resp.content_location = HTTP_CONTENT_LOCATION_NO_CONTENT;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    ASSERT_TRUE(this->m_handle_req_called);
    ASSERT_FALSE(this->m_handle_req_flag_from_lan);
    ASSERT_TRUE(this->m_resp_called);
    ASSERT_EQ("192.168.1.114", this->m_resp_hostname);
    ASSERT_FALSE(this->m_resp_302_called);
    // INFO log for the request
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== JSON response logging =====

TEST_F(TestHttpServerNetconnServeHandleReq, test_json_resp_static_mem_short_content) // NOLINT
{
    // JSON response from static memory, content_len <= 256 → log with content
    char            req_buf[]     = "GET /api HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    static char json_content[]                               = "{\"status\":\"ok\"}";
    this->m_handle_req_resp.http_resp_code                   = HTTP_RESP_CODE_200;
    this->m_handle_req_resp.content_type                     = HTTP_CONTENT_TYPE_APPLICATION_JSON;
    this->m_handle_req_resp.content_location                 = HTTP_CONTENT_LOCATION_STATIC_MEM;
    this->m_handle_req_resp.select_location.static_mem.p_buf = reinterpret_cast<const uint8_t*>(json_content);
    this->m_handle_req_resp.content_len                      = strlen(json_content);

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    ASSERT_TRUE(this->m_handle_req_called);
    ASSERT_TRUE(this->m_resp_called);
    // INFO log for the request
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /api");
    // INFO log for JSON response with content
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("Json resp: code=200, content:\n") + json_content);
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReq, test_json_resp_heap_long_content) // NOLINT
{
    // JSON response from heap, content_len > 256 → log with length only
    char            req_buf[]     = "GET /api HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    // Create a string longer than 256 chars
    constexpr size_t long_json_size = 300;
    char*            p_long_json    = static_cast<char*>(os_malloc(long_json_size));
    assert(nullptr != p_long_json);
    memset(p_long_json, 'x', long_json_size - 1);
    p_long_json[0]                  = '{';
    p_long_json[long_json_size - 2] = '}';
    p_long_json[long_json_size - 1] = '\0';

    this->m_handle_req_resp.http_resp_code             = HTTP_RESP_CODE_200;
    this->m_handle_req_resp.content_type               = HTTP_CONTENT_TYPE_APPLICATION_JSON;
    this->m_handle_req_resp.content_location           = HTTP_CONTENT_LOCATION_HEAP;
    this->m_handle_req_resp.select_location.heap.p_buf = reinterpret_cast<uint8_t*>(p_long_json);
    this->m_handle_req_resp.content_len                = strlen(p_long_json);

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    ASSERT_TRUE(this->m_handle_req_called);
    ASSERT_TRUE(this->m_resp_called);
    // INFO log for the request
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /api");
    // INFO log for JSON response with length only (content_len = 299 > 256)
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Json resp: code=200, content_len=" + to_string(long_json_size - 1));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReq, test_json_resp_flash_mem_short_content) // NOLINT
{
    // JSON response from flash memory, content_len <= 256 → log with content
    char            req_buf[]     = "GET /api HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    static const char json_content[]                    = "{\"version\":\"1.0\"}";
    this->m_handle_req_resp.http_resp_code              = HTTP_RESP_CODE_200;
    this->m_handle_req_resp.content_type                = HTTP_CONTENT_TYPE_APPLICATION_JSON;
    this->m_handle_req_resp.content_location            = HTTP_CONTENT_LOCATION_FLASH_MEM;
    this->m_handle_req_resp.select_location.flash.p_buf = reinterpret_cast<const uint8_t*>(json_content);
    this->m_handle_req_resp.content_len                 = strlen(json_content);

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    ASSERT_TRUE(this->m_handle_req_called);
    ASSERT_TRUE(this->m_resp_called);
    // INFO log for the request
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /api");
    // INFO log for JSON response with content (flash mem)
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("Json resp: code=200, content:\n") + json_content);
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReq, test_json_resp_no_content) // NOLINT
{
    // JSON response with NO_CONTENT location → log warning
    char            req_buf[]     = "GET /api HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    this->m_handle_req_resp.http_resp_code   = HTTP_RESP_CODE_200;
    this->m_handle_req_resp.content_type     = HTTP_CONTENT_TYPE_APPLICATION_JSON;
    this->m_handle_req_resp.content_location = HTTP_CONTENT_LOCATION_NO_CONTENT;
    this->m_handle_req_resp.content_len      = 0;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    ASSERT_TRUE(this->m_handle_req_called);
    ASSERT_TRUE(this->m_resp_called);
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /api");
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Json resp: code=200, content (len 0): NO_CONTENT");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReq, test_json_resp_fatfs) // NOLINT
{
    // JSON response with FATFS location → log info with FATFS
    char            req_buf[]     = "GET /api HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    this->m_handle_req_resp.http_resp_code   = HTTP_RESP_CODE_200;
    this->m_handle_req_resp.content_type     = HTTP_CONTENT_TYPE_APPLICATION_JSON;
    this->m_handle_req_resp.content_location = HTTP_CONTENT_LOCATION_FATFS;
    this->m_handle_req_resp.content_len      = 1024;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    ASSERT_TRUE(this->m_handle_req_called);
    ASSERT_TRUE(this->m_resp_called);
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /api");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Json resp: code=200, content (len 1024): FATFS");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReq, test_non_json_resp_no_json_log) // NOLINT
{
    // Non-JSON response → no JSON log line
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    this->m_handle_req_resp.http_resp_code   = HTTP_RESP_CODE_200;
    this->m_handle_req_resp.content_type     = HTTP_CONTENT_TYPE_TEXT_HTML;
    this->m_handle_req_resp.content_location = HTTP_CONTENT_LOCATION_FLASH_MEM;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    ASSERT_TRUE(this->m_handle_req_called);
    ASSERT_TRUE(this->m_resp_called);
    // Only the request INFO log, no JSON log
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== Extra header fields =====

TEST_F(TestHttpServerNetconnServeHandleReq, test_extra_header_fields_logged) // NOLINT
{
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    this->m_handle_req_resp.http_resp_code   = HTTP_RESP_CODE_200;
    this->m_handle_req_resp.content_type     = HTTP_CONTENT_TYPE_TEXT_HTML;
    this->m_handle_req_resp.content_location = HTTP_CONTENT_LOCATION_NO_CONTENT;
    this->m_extra_header_to_set              = "X-Custom: value\r\n";

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    ASSERT_TRUE(this->m_handle_req_called);
    ASSERT_TRUE(this->m_resp_called);
    // INFO log for request
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /");
    // INFO log for extra header fields
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Extra HTTP-header resp: X-Custom: value\r\n");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== Hostname allocation =====

TEST_F(TestHttpServerNetconnServeHandleReq, test_hostname_from_host_header) // NOLINT
{
    // When Host header is present, hostname should be from Host header value
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: myhost.example.com\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "10.0.0.1" };
    sta_ip_string_t remote_ip_str = { .buf = "10.0.0.2" };

    this->m_handle_req_resp.http_resp_code   = HTTP_RESP_CODE_200;
    this->m_handle_req_resp.content_type     = HTTP_CONTENT_TYPE_TEXT_HTML;
    this->m_handle_req_resp.content_location = HTTP_CONTENT_LOCATION_NO_CONTENT;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    ASSERT_TRUE(this->m_resp_called);
    ASSERT_EQ("myhost.example.com", this->m_resp_hostname);
    // INFO log
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 10.0.0.2 to 10.0.0.1 (Host: myhost.example.com): GET /");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReq, test_hostname_from_local_ip_when_no_host) // NOLINT
{
    // No Host header → hostname should be from local_ip
    char            req_buf[]     = "GET / HTTP/1.1\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "10.0.0.1" };
    sta_ip_string_t remote_ip_str = { .buf = "10.0.0.2" };

    this->m_handle_req_resp.http_resp_code   = HTTP_RESP_CODE_200;
    this->m_handle_req_resp.content_type     = HTTP_CONTENT_TYPE_TEXT_HTML;
    this->m_handle_req_resp.content_location = HTTP_CONTENT_LOCATION_NO_CONTENT;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    ASSERT_TRUE(this->m_resp_called);
    ASSERT_EQ("10.0.0.1", this->m_resp_hostname);
    // INFO log with empty Host
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 10.0.0.2 to 10.0.0.1 (Host: ): GET /");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReq, test_hostname_alloc_failure_returns_500) // NOLINT
{
    // str_buf_printf_with_alloc fails → LOG_ERR + resp_500
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    this->m_handle_req_resp.http_resp_code   = HTTP_RESP_CODE_200;
    this->m_handle_req_resp.content_type     = HTTP_CONTENT_TYPE_TEXT_HTML;
    this->m_handle_req_resp.content_location = HTTP_CONTENT_LOCATION_NO_CONTENT;

    // Fail the first calloc call (str_buf_printf_with_alloc → os_malloc → os_calloc_internal → calloc)
    this->m_alloc_fail_on_call_idx = 1;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    ASSERT_FALSE(this->m_handle_req_called);
    ASSERT_TRUE(this->m_resp_500_called);
    ASSERT_FALSE(this->m_resp_called);
    // INFO log for request
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /");
    // ERROR log for allocation failure
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "Failed to allocate memory for hostname string");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}
