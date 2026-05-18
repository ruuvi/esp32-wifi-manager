/**
 * @file test_http_server_netconn_serve_handle_req_with_resp.cpp
 * @author TheSomeMan
 * @date 2026-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gtest/gtest.h"
#include "http_server_netconn_serve_handle_req.h"
#include <string>
#include <vector>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include "esp_err.h"
#include "os_task.h"
#include "os_mutex.h"
#include "os_malloc.h"
#include "esp_log_wrapper.hpp"
#include "http_server_auth.h"
#include "http_server_handle_req.h"
#include "wifi_manager_defs.h"
#include "http_server_netconn_resp.h"
#include "json_stream_gen.h"

using namespace std;

class TestHttpServerNetconnServeHandleReqWithResp;
static TestHttpServerNetconnServeHandleReqWithResp* g_pTestClass;

/*** Google-test class implementation *********************************************************************************/

struct NetconnWriteCall
{
    string  data;
    uint8_t flags;
};

class TestHttpServerNetconnServeHandleReqWithResp : public ::testing::Test
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

        this->m_tick_values.clear();
        this->m_tick_idx = 0;

        this->m_vTaskDelay_called = false;
        this->m_vTaskDelay_ticks  = 0;

        this->m_netconn_writes.clear();
        this->m_netconn_write_errors.clear();
        this->m_netconn_write_call_idx = 0;

        this->m_wdt_reset_count = 0;

        this->m_lan_blocked = false;

        memset(&this->m_handle_req_resp, 0, sizeof(this->m_handle_req_resp));
        this->m_handle_req_called        = false;
        this->m_handle_req_flag_from_lan = false;
        this->m_extra_header_to_set      = "";

        esp_log_wrapper_clear();

        this->m_alloc_free_call_count       = 0;
        this->m_flag_alloc_counting_enabled = true;
        this->m_alloc_call_cnt              = 0;
        this->m_alloc_fail_on_call_idx      = -1;
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

    // Tick count stub config
    vector<TickType_t> m_tick_values;
    size_t             m_tick_idx;

    // vTaskDelay tracking
    bool       m_vTaskDelay_called;
    TickType_t m_vTaskDelay_ticks;

    // netconn_write_partly tracking
    vector<NetconnWriteCall> m_netconn_writes;
    vector<err_t>            m_netconn_write_errors;
    size_t                   m_netconn_write_call_idx;

    // watchdog reset tracking
    int m_wdt_reset_count;

    http_server_auth_info_t m_auth_info;

    // wifi_manager stub config
    bool m_lan_blocked;

    // http_server_handle_req stub config
    http_server_resp_t                 m_handle_req_resp;
    bool                               m_handle_req_called;
    bool                               m_handle_req_flag_from_lan;
    string                             m_extra_header_to_set;
    string                             m_content_in_heap;
    string                             m_file_data;
    json_stream_gen_cb_generate_next_t m_json_gen_cb;

    bool m_flag_alloc_counting_enabled;
    int  m_alloc_free_call_count;
    int  m_alloc_call_cnt;
    int  m_alloc_fail_on_call_idx;

    TestHttpServerNetconnServeHandleReqWithResp();

    ~TestHttpServerNetconnServeHandleReqWithResp() override;

    string
    get_all_written_data() const
    {
        string result;
        for (const auto& w : this->m_netconn_writes)
        {
            result += w.data;
        }
        return result;
    }
};

TestHttpServerNetconnServeHandleReqWithResp::TestHttpServerNetconnServeHandleReqWithResp()
    : Test()
    , m_p_conn(nullptr)
    , m_mutex_try_lock_result(true)
    , m_tick_idx(0)
    , m_vTaskDelay_called(false)
    , m_vTaskDelay_ticks(0)
    , m_netconn_write_call_idx(0)
    , m_wdt_reset_count(0)
    , m_auth_info()
    , m_lan_blocked(false)
    , m_handle_req_resp()
    , m_handle_req_called(false)
    , m_handle_req_flag_from_lan(false)
    , m_content_in_heap("")
    , m_file_data("")
    , m_json_gen_cb(nullptr)
    , m_flag_alloc_counting_enabled(false)
    , m_alloc_free_call_count(0)
    , m_alloc_call_cnt(0)
    , m_alloc_fail_on_call_idx(-1)
{
}

TestHttpServerNetconnServeHandleReqWithResp::~TestHttpServerNetconnServeHandleReqWithResp() = default;

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

void
http_server_netconn_resp_free(http_server_resp_t* const p_resp)
{
    switch (p_resp->content_location)
    {
        case HTTP_CONTENT_LOCATION_NO_CONTENT:
            break;
        case HTTP_CONTENT_LOCATION_FLASH_MEM:
            break;
        case HTTP_CONTENT_LOCATION_STATIC_MEM:
            break;
        case HTTP_CONTENT_LOCATION_HEAP:
            os_free(p_resp->select_location.memory.p_buf);
            break;
        case HTTP_CONTENT_LOCATION_FATFS:
            close(p_resp->select_location.fatfs.fd);
            break;
        case HTTP_CONTENT_LOCATION_JSON_GENERATOR:
            json_stream_gen_delete(&p_resp->select_location.json_generator.p_json_gen);
            break;
    }
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

void
http_server_task_wdt_reset(void)
{
    if (nullptr != g_pTestClass)
    {
        g_pTestClass->m_wdt_reset_count += 1;
    }
}

const char*
wrap_esp_err_to_name_r(const esp_err_t code, char* const p_buf, const size_t buf_len)
{
    (void)snprintf(p_buf, buf_len, "Unknows");
    return p_buf;
}

void
http_server_sema_send_wait_immediate(void)
{
}

err_t
netconn_write_partly(struct netconn* conn, const void* dataptr, size_t size, u8_t apiflags, size_t* bytes_written)
{
    if (nullptr == g_pTestClass)
    {
        *bytes_written = size;
        return ERR_OK;
    }
    const size_t idx = g_pTestClass->m_netconn_write_call_idx;
    g_pTestClass->m_netconn_write_call_idx += 1;

    err_t err = ERR_OK;
    if (idx < g_pTestClass->m_netconn_write_errors.size())
    {
        err = g_pTestClass->m_netconn_write_errors[idx];
    }
    if (ERR_OK == err)
    {
        NetconnWriteCall call;
        call.data.assign(static_cast<const char*>(dataptr), size);
        call.flags = apiflags;
        g_pTestClass->m_netconn_writes.push_back(call);
        *bytes_written = size;
    }
    else
    {
        *bytes_written = 0;
    }
    return err;
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

static void*
test_os_malloc(size_t size)
{
    return os_malloc(size);
}

static void
test_os_free(void* ptr)
{
    os_free(ptr);
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
        if (HTTP_CONTENT_LOCATION_HEAP == g_pTestClass->m_handle_req_resp.content_location)
        {
            // For heap content, we need to allocate memory and copy the content, because http_server_netconn_resp will
            // free it after use
            if (g_pTestClass->m_content_in_heap.length() != g_pTestClass->m_handle_req_resp.content_len)
            {
                assert(g_pTestClass->m_content_in_heap.length() == g_pTestClass->m_handle_req_resp.content_len);
            }
            char* const p_heap_content = static_cast<char*>(os_malloc(g_pTestClass->m_content_in_heap.length()));
            if (!p_heap_content)
            {
                assert(p_heap_content);
            }
            memcpy(p_heap_content, g_pTestClass->m_content_in_heap.c_str(), g_pTestClass->m_content_in_heap.length());
            g_pTestClass->m_handle_req_resp.select_location.memory.p_buf = reinterpret_cast<const uint8_t*>(
                p_heap_content);
        }
        else if (HTTP_CONTENT_LOCATION_FATFS == g_pTestClass->m_handle_req_resp.content_location)
        {
            if (g_pTestClass->m_file_data.length() != g_pTestClass->m_handle_req_resp.content_len)
            {
                assert(g_pTestClass->m_file_data.length() == g_pTestClass->m_handle_req_resp.content_len);
            }
            // Create a pipe to simulate a FATFS file descriptor
            int       pipe_fds[2];
            const int res = pipe(pipe_fds);
            if (0 != res)
            {
                assert(0 == res);
            }
            g_pTestClass->m_handle_req_resp.select_location.fatfs.fd = pipe_fds[0];
            write(pipe_fds[1], g_pTestClass->m_file_data.c_str(), g_pTestClass->m_file_data.length());
            close(pipe_fds[1]);
        }
        else if (HTTP_CONTENT_LOCATION_JSON_GENERATOR == g_pTestClass->m_handle_req_resp.content_location)
        {
            json_stream_gen_cfg_t cfg = {};
            cfg.max_chunk_size        = 256;
            cfg.flag_formatted_json   = false;
            cfg.max_nesting_level     = 3;
            cfg.p_malloc              = &test_os_malloc;
            cfg.p_free                = &test_os_free;
            cfg.p_localeconv          = &localeconv;

            void*              p_ctx      = nullptr;
            json_stream_gen_t* p_json_gen = json_stream_gen_create(&cfg, g_pTestClass->m_json_gen_cb, 0, &p_ctx);
            if (nullptr == p_json_gen)
            {
                assert(nullptr != p_json_gen);
            }
            g_pTestClass->m_handle_req_resp.select_location.json_generator.p_json_gen = p_json_gen;
        }
        return g_pTestClass->m_handle_req_resp;
    }
    http_server_resp_t resp = {};
    return resp;
}

#ifdef __cplusplus
}
#endif

#define TEST_CHECK_LOG_RECORD(level_, msg_) ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("http_server", level_, msg_)

/*** Unit-Tests *******************************************************************************************************/

// ===== Parse failure =====

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_parse_failure_returns_400) // NOLINT
{
    // Request without \r\n\r\n → http_req_parse returns is_success=false
    char            req_buf[]     = "malformed request";
    sta_ip_string_t local_ip_str  = { .buf = "10.0.0.1" };
    sta_ip_string_t remote_ip_str = { .buf = "10.0.0.2" };

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    // Real resp_400 sends HTTP response via netconn_write
    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 400 Bad Request"));
    ASSERT_FALSE(this->m_handle_req_called);
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_ERROR,
        "Request from 10.0.0.2 to 10.0.0.1: failed to parse request: malformed request");
    // resp_without_content logs WARN
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Response: status 400 (Bad Request)");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== LAN access paths =====

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_lan_access_blocked_returns_503) // NOLINT
{
    // local_ip != AP IP → LAN access; wifi_manager says blocked
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: 10.0.0.1\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "10.0.0.1" };
    sta_ip_string_t remote_ip_str = { .buf = "10.0.0.2" };

    this->m_lan_blocked = true;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 503 Service Unavailable"));
    ASSERT_FALSE(this->m_handle_req_called);
    // INFO log for the request
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 10.0.0.2 to 10.0.0.1 (Host: 10.0.0.1): GET /");
    // WARN log about blocked LAN
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Request from LAN while WiFi hotspot is active - return HTTP error 503");
    // resp_without_content logs WARN
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Response: status 503 (Service Unavailable)");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_lan_access_not_blocked_normal_handling) // NOLINT
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
    // Real resp function sends HTTP response
    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 200 OK"));
    // INFO log for the request
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 10.0.0.2 to 10.0.0.1 (Host: 10.0.0.1): GET /status");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== AP access / captive portal paths =====

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_ap_access_captive_portal_redirect) // NOLINT
{
    // local_ip == AP IP → AP access; Host doesn't contain AP IP → redirect
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: some.host.com\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 302 Found"));
    ASSERT_NE(string::npos, written.find("Location: http://192.168.1.114/"));
    ASSERT_FALSE(this->m_handle_req_called);
    // INFO log for the request
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: some.host.com): GET /");
    // resp_302 logs INFO
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: status 302 (Found), URL=http://192.168.1.114/");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_ap_access_no_host_header_redirect) // NOLINT
{
    // local_ip == AP IP; no Host header → host_len=0 → is_request_to_ap_ip=false → redirect
    char            req_buf[]     = "GET / HTTP/1.1\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 302 Found"));
    ASSERT_NE(string::npos, written.find("Location: http://192.168.1.114/"));
    ASSERT_FALSE(this->m_handle_req_called);
    // INFO log: Host is empty since no Host header
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: ): GET /");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: status 302 (Found), URL=http://192.168.1.114/");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_ap_access_host_matches_ap_ip_normal) // NOLINT
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
    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 200 OK"));
    // INFO log for the request
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== JSON response logging =====

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_json_resp_static_mem_short_content) // NOLINT
{
    // JSON response from static memory, content_len <= 256 → log with content
    char            req_buf[]     = "GET /api HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    static const char json_content[]                     = "{\"status\":\"ok\"}";
    this->m_handle_req_resp.http_resp_code               = HTTP_RESP_CODE_200;
    this->m_handle_req_resp.content_type                 = HTTP_CONTENT_TYPE_APPLICATION_JSON;
    this->m_handle_req_resp.content_location             = HTTP_CONTENT_LOCATION_STATIC_MEM;
    this->m_handle_req_resp.content_len                  = strlen(json_content);
    this->m_handle_req_resp.select_location.memory.p_buf = (const uint8_t*)json_content;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    ASSERT_TRUE(this->m_handle_req_called);
    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 200 OK"));
    ASSERT_NE(string::npos, written.find(json_content));
    // INFO log for the request
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /api");
    // INFO log for JSON response with content
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, string("Json resp: code=200, content:\n") + json_content);
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_json_resp_heap_long_content) // NOLINT
{
    // JSON response from heap, content_len > 256 → log with length only
    char            req_buf[]     = "GET /api HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    // Create a string longer than 256 chars on the heap (so http_server_netconn_resp_free frees it)
    this->m_content_in_heap      = string(299, 'x');
    this->m_content_in_heap[0]   = '{';
    this->m_content_in_heap[298] = '}';

    this->m_handle_req_resp.http_resp_code   = HTTP_RESP_CODE_200;
    this->m_handle_req_resp.content_type     = HTTP_CONTENT_TYPE_APPLICATION_JSON;
    this->m_handle_req_resp.content_location = HTTP_CONTENT_LOCATION_HEAP;
    this->m_handle_req_resp.content_len      = this->m_content_in_heap.length();
    this->m_handle_req_resp.select_location.memory.p_buf
        = nullptr; // heap content will be allocated in http_server_handle_req

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    ASSERT_TRUE(this->m_handle_req_called);
    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 200 OK"));
    // INFO log for the request
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /api");
    // INFO log for JSON response with length only (content_len = 299 > 256)
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Json resp: code=200, content_len=299");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_non_json_resp_no_json_log) // NOLINT
{
    // Non-JSON response → no JSON log line
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    static const char content[]                          = "<html>Hello</html>";
    this->m_handle_req_resp.http_resp_code               = HTTP_RESP_CODE_200;
    this->m_handle_req_resp.content_type                 = HTTP_CONTENT_TYPE_TEXT_HTML;
    this->m_handle_req_resp.content_location             = HTTP_CONTENT_LOCATION_FLASH_MEM;
    this->m_handle_req_resp.content_len                  = strlen(content);
    this->m_handle_req_resp.select_location.memory.p_buf = (const uint8_t*)content;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    ASSERT_TRUE(this->m_handle_req_called);
    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 200 OK"));
    ASSERT_NE(string::npos, written.find(content));
    // Only the request INFO log + response log, no JSON log
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== Extra header fields =====

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_extra_header_fields_logged) // NOLINT
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
    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 200 OK"));
    ASSERT_NE(string::npos, written.find("X-Custom: value"));
    // INFO log for request
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /");
    // INFO log for extra header fields
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Extra HTTP-header resp: X-Custom: value\r\n");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== Hostname allocation =====

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_hostname_from_host_header) // NOLINT
{
    // When Host header is present, hostname should be from Host header value
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: myhost.example.com\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "10.0.0.1" };
    sta_ip_string_t remote_ip_str = { .buf = "10.0.0.2" };

    this->m_handle_req_resp.http_resp_code   = HTTP_RESP_CODE_301;
    this->m_handle_req_resp.content_type     = HTTP_CONTENT_TYPE_TEXT_HTML;
    this->m_handle_req_resp.content_location = HTTP_CONTENT_LOCATION_NO_CONTENT;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    // 301 response uses hostname in Location header
    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 301 Moved Permanently"));
    ASSERT_NE(string::npos, written.find("Location: http://myhost.example.com/#auth"));
    // INFO log
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 10.0.0.2 to 10.0.0.1 (Host: myhost.example.com): GET /");
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_INFO,
        "Response: status 301 (Moved Permanently), URL=http://myhost.example.com/#auth");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_hostname_from_local_ip_when_no_host) // NOLINT
{
    // No Host header → hostname should be from local_ip
    char            req_buf[]     = "GET / HTTP/1.1\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "10.0.0.1" };
    sta_ip_string_t remote_ip_str = { .buf = "10.0.0.2" };

    this->m_handle_req_resp.http_resp_code   = HTTP_RESP_CODE_302;
    this->m_handle_req_resp.content_type     = HTTP_CONTENT_TYPE_TEXT_HTML;
    this->m_handle_req_resp.content_location = HTTP_CONTENT_LOCATION_NO_CONTENT;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    // 302 auth redirect uses hostname in Location header
    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 302 Found"));
    ASSERT_NE(string::npos, written.find("Location: http://10.0.0.1/#auth"));
    // INFO log with empty Host
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 10.0.0.2 to 10.0.0.1 (Host: ): GET /");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: status 302 (Found), URL=http://10.0.0.1/#auth");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_hostname_alloc_failure_no_content) // NOLINT
{
    // str_buf_printf_with_alloc fails → LOG_ERR + resp_500 + resp_free (NO_CONTENT: nothing to free)
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
    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 500 Internal Server Error"));
    // INFO log for request
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /");
    // ERROR log for allocation failure
    {
        const auto log_record = esp_log_wrapper_pop();
        ASSERT_EQ(ESP_LOG_ERROR, log_record.level);
        ASSERT_NE(string::npos, string(log_record.parsed.msg).find("Failed to allocate memory for hostname string"));
    }
    // resp_500 → resp_without_content logs WARN
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Response: status 500 (Internal Server Error)");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== Hostname alloc failure with resource leak scenarios (tests http_server_netconn_resp_free) =====

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_hostname_alloc_failure_frees_heap_content) // NOLINT
{
    // When hostname alloc fails and resp has HEAP content, resp_free must os_free the buffer
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    this->m_content_in_heap = string("{\"key\":\"val\"}");

    this->m_handle_req_resp.http_resp_code   = HTTP_RESP_CODE_200;
    this->m_handle_req_resp.content_type     = HTTP_CONTENT_TYPE_APPLICATION_JSON;
    this->m_handle_req_resp.content_location = HTTP_CONTENT_LOCATION_HEAP;
    this->m_handle_req_resp.content_len      = this->m_content_in_heap.length();
    this->m_handle_req_resp.select_location.memory.p_buf
        = nullptr; // heap content will be allocated in http_server_handle_req

    // Fail the hostname alloc (first calloc after handle_req)
    this->m_alloc_fail_on_call_idx = 1;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    // Should get 500 response
    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 500 Internal Server Error"));

    // Verify no memory leak - the heap buffer should have been freed by resp_free
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /");
    {
        const auto log_record = esp_log_wrapper_pop();
        ASSERT_EQ(ESP_LOG_ERROR, log_record.level);
        ASSERT_NE(string::npos, string(log_record.parsed.msg).find("Failed to allocate memory for hostname string"));
    }
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Response: status 500 (Internal Server Error)");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    // ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_hostname_alloc_failure_closes_fatfs_fd) // NOLINT
{
    // When hostname alloc fails and resp has FATFS content, resp_free must close the fd
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    // Create a pipe to simulate a FATFS file descriptor
    this->m_file_data = string("file content");

    this->m_handle_req_resp.http_resp_code           = HTTP_RESP_CODE_200;
    this->m_handle_req_resp.content_type             = HTTP_CONTENT_TYPE_APPLICATION_OCTET_STREAM;
    this->m_handle_req_resp.content_location         = HTTP_CONTENT_LOCATION_FATFS;
    this->m_handle_req_resp.content_len              = this->m_file_data.length();
    this->m_handle_req_resp.select_location.fatfs.fd = -1;

    // Fail the hostname alloc
    this->m_alloc_fail_on_call_idx = 1;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    // Verify that file was not opened since hostname alloc failed,
    // so fd should still be -1 (if it was opened, it would be a non-negative integer)
    ASSERT_EQ(-1, this->m_handle_req_resp.select_location.fatfs.fd);

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 500 Internal Server Error"));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /");
    {
        const auto log_record = esp_log_wrapper_pop();
        ASSERT_EQ(ESP_LOG_ERROR, log_record.level);
        ASSERT_NE(string::npos, string(log_record.parsed.msg).find("Failed to allocate memory for hostname string"));
    }
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Response: status 500 (Internal Server Error)");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

static json_stream_gen_callback_result_t
test_json_gen_cb(json_stream_gen_t* const p_gen, const void* const p_user_ctx)
{
    (void)p_user_ctx;
    JSON_STREAM_GEN_BEGIN_GENERATOR_FUNC(p_gen);
    JSON_STREAM_GEN_ADD_STRING(p_gen, "key111", "value222");
    JSON_STREAM_GEN_END_GENERATOR_FUNC();
}

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_hostname_alloc_failure_deletes_json_gen) // NOLINT
{
    // When hostname alloc fails and resp has JSON_GENERATOR content, resp_free must delete the generator
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    this->m_json_gen_cb = &test_json_gen_cb;

    this->m_handle_req_resp.http_resp_code                            = HTTP_RESP_CODE_200;
    this->m_handle_req_resp.content_type                              = HTTP_CONTENT_TYPE_APPLICATION_JSON;
    this->m_handle_req_resp.content_location                          = HTTP_CONTENT_LOCATION_JSON_GENERATOR;
    this->m_handle_req_resp.content_len                               = 0;
    this->m_handle_req_resp.select_location.json_generator.p_json_gen = nullptr;

    // Fail the hostname alloc
    this->m_alloc_fail_on_call_idx = 1;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    ASSERT_EQ(nullptr, this->m_handle_req_resp.select_location.json_generator.p_json_gen);

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 500 Internal Server Error"));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /");
    {
        const auto log_record = esp_log_wrapper_pop();
        ASSERT_EQ(ESP_LOG_ERROR, log_record.level);
        ASSERT_NE(string::npos, string(log_record.parsed.msg).find("Failed to allocate memory for hostname string"));
    }
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Response: status 500 (Internal Server Error)");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    // json_stream_gen should have been deleted by resp_free - no leak
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== Response codes through full path =====

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_resp_200_with_static_mem_content) // NOLINT
{
    // Normal 200 response with HEAP content, verify content is written and freed
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    char content[] = "static_mem data here";

    this->m_handle_req_resp.http_resp_code               = HTTP_RESP_CODE_200;
    this->m_handle_req_resp.content_type                 = HTTP_CONTENT_TYPE_TEXT_PLAIN;
    this->m_handle_req_resp.content_location             = HTTP_CONTENT_LOCATION_STATIC_MEM;
    this->m_handle_req_resp.content_len                  = strlen(content);
    this->m_handle_req_resp.select_location.memory.p_buf = reinterpret_cast<uint8_t*>(content);

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 200 OK"));
    ASSERT_NE(string::npos, written.find("static_mem data here"));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_resp_200_with_heap_content) // NOLINT
{
    // Normal 200 response with HEAP content, verify content is written and freed
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    this->m_content_in_heap = string("heap data here");

    this->m_handle_req_resp.http_resp_code   = HTTP_RESP_CODE_200;
    this->m_handle_req_resp.content_type     = HTTP_CONTENT_TYPE_TEXT_PLAIN;
    this->m_handle_req_resp.content_location = HTTP_CONTENT_LOCATION_HEAP;
    this->m_handle_req_resp.content_len      = this->m_content_in_heap.length();
    this->m_handle_req_resp.select_location.memory.p_buf
        = nullptr; // heap content will be allocated in http_server_handle_req

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 200 OK"));
    ASSERT_NE(string::npos, written.find("heap data here"));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_resp_200_with_fatfs_content) // NOLINT
{
    // Normal 200 response with FATFS content, verify content is read and fd is closed
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    // Simulation of file opening will be performed in http_server_handle_req
    this->m_file_data = string("fatfs file content");

    this->m_handle_req_resp.http_resp_code           = HTTP_RESP_CODE_200;
    this->m_handle_req_resp.content_type             = HTTP_CONTENT_TYPE_APPLICATION_OCTET_STREAM;
    this->m_handle_req_resp.content_location         = HTTP_CONTENT_LOCATION_FATFS;
    this->m_handle_req_resp.content_len              = this->m_file_data.length();
    this->m_handle_req_resp.select_location.fatfs.fd = -1;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    ASSERT_NE(-1, this->m_handle_req_resp.select_location.fatfs.fd);

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 200 OK"));
    ASSERT_NE(string::npos, written.find(this->m_file_data.c_str()));

    // Verify fd closed
    char tmp;
    ASSERT_EQ(-1, read(this->m_handle_req_resp.select_location.fatfs.fd, &tmp, 1));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_resp_200_with_json_content) // NOLINT
{
    // Normal 200 response with FATFS content, verify content is read and fd is closed
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    this->m_json_gen_cb = &test_json_gen_cb;

    this->m_handle_req_resp.http_resp_code                            = HTTP_RESP_CODE_200;
    this->m_handle_req_resp.content_type                              = HTTP_CONTENT_TYPE_APPLICATION_JSON;
    this->m_handle_req_resp.content_location                          = HTTP_CONTENT_LOCATION_JSON_GENERATOR;
    this->m_handle_req_resp.content_len                               = 0;
    this->m_handle_req_resp.select_location.json_generator.p_json_gen = nullptr;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    ASSERT_NE(nullptr, this->m_handle_req_resp.select_location.json_generator.p_json_gen);

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 200 OK"));
    ASSERT_NE(string::npos, written.find("{\"key111\":\"value222\"}"));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "json_stream_gen: send 21 bytes:\n{\"key111\":\"value222\"}");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_resp_200_with_flash_mem_content) // NOLINT
{
    // Normal 200 response with FLASH_MEM content
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    static const char content[]                          = "<html>flash</html>";
    this->m_handle_req_resp.http_resp_code               = HTTP_RESP_CODE_200;
    this->m_handle_req_resp.content_type                 = HTTP_CONTENT_TYPE_TEXT_HTML;
    this->m_handle_req_resp.content_location             = HTTP_CONTENT_LOCATION_FLASH_MEM;
    this->m_handle_req_resp.content_len                  = strlen(content);
    this->m_handle_req_resp.select_location.memory.p_buf = (const uint8_t*)content;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 200 OK"));
    ASSERT_NE(string::npos, written.find(content));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== Various response codes =====

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_resp_400_with_content) // NOLINT
{
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    static const char err_content[]                      = "Bad request details";
    this->m_handle_req_resp.http_resp_code               = HTTP_RESP_CODE_400;
    this->m_handle_req_resp.content_type                 = HTTP_CONTENT_TYPE_TEXT_PLAIN;
    this->m_handle_req_resp.content_location             = HTTP_CONTENT_LOCATION_STATIC_MEM;
    this->m_handle_req_resp.content_len                  = strlen(err_content);
    this->m_handle_req_resp.select_location.memory.p_buf = (const uint8_t*)err_content;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 400 Bad Request"));
    ASSERT_NE(string::npos, written.find(err_content));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /");
    // 400 with content → resp_with_content → WARN log
    {
        const auto log_record = esp_log_wrapper_pop();
        ASSERT_EQ(ESP_LOG_WARN, log_record.level);
        ASSERT_NE(string::npos, string(log_record.parsed.msg).find("status 400"));
    }
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_resp_400_without_content) // NOLINT
{
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    this->m_handle_req_resp.http_resp_code   = HTTP_RESP_CODE_400;
    this->m_handle_req_resp.content_type     = HTTP_CONTENT_TYPE_TEXT_PLAIN;
    this->m_handle_req_resp.content_location = HTTP_CONTENT_LOCATION_NO_CONTENT;
    this->m_handle_req_resp.content_len      = 0;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 400 Bad Request"));
    // resp_without_content sends empty JSON "{}"
    ASSERT_NE(string::npos, written.find("{}"));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /");
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Response: status 400 (Bad Request)");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_resp_401_with_extra_headers) // NOLINT
{
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    this->m_handle_req_resp.http_resp_code   = HTTP_RESP_CODE_401;
    this->m_handle_req_resp.content_type     = HTTP_CONTENT_TYPE_TEXT_HTML;
    this->m_handle_req_resp.content_location = HTTP_CONTENT_LOCATION_NO_CONTENT;
    this->m_handle_req_resp.content_len      = 0;
    this->m_extra_header_to_set              = "WWW-Authenticate: Basic realm=\"test\"\r\n";

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 401 Unauthorized"));
    ASSERT_NE(string::npos, written.find("WWW-Authenticate: Basic"));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Extra HTTP-header resp: WWW-Authenticate: Basic realm=\"test\"\r\n");
    // 401 with content_len=0 uses resp_with_content (since p_resp != NULL), but content_len=0 → no content write
    {
        const auto log_record = esp_log_wrapper_pop();
        ASSERT_EQ(ESP_LOG_WARN, log_record.level);
        ASSERT_NE(string::npos, string(log_record.parsed.msg).find("status 401"));
    }
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_resp_404) // NOLINT
{
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    this->m_handle_req_resp.http_resp_code   = HTTP_RESP_CODE_404;
    this->m_handle_req_resp.content_type     = HTTP_CONTENT_TYPE_TEXT_HTML;
    this->m_handle_req_resp.content_location = HTTP_CONTENT_LOCATION_NO_CONTENT;
    this->m_handle_req_resp.content_len      = 0;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 404 Not Found"));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /");
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Response: status 404 (Not Found)");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_resp_500) // NOLINT
{
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    this->m_handle_req_resp.http_resp_code   = HTTP_RESP_CODE_500;
    this->m_handle_req_resp.content_type     = HTTP_CONTENT_TYPE_TEXT_HTML;
    this->m_handle_req_resp.content_location = HTTP_CONTENT_LOCATION_NO_CONTENT;
    this->m_handle_req_resp.content_len      = 0;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 500 Internal Server Error"));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /");
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Response: status 500 (Internal Server Error)");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_resp_503) // NOLINT
{
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    this->m_handle_req_resp.http_resp_code   = HTTP_RESP_CODE_503;
    this->m_handle_req_resp.content_type     = HTTP_CONTENT_TYPE_TEXT_HTML;
    this->m_handle_req_resp.content_location = HTTP_CONTENT_LOCATION_NO_CONTENT;
    this->m_handle_req_resp.content_len      = 0;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 503 Service Unavailable"));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /");
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Response: status 503 (Service Unavailable)");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_resp_206_falls_back_to_200) // NOLINT
{
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    static const char content[]                          = "partial";
    this->m_handle_req_resp.http_resp_code               = HTTP_RESP_CODE_206;
    this->m_handle_req_resp.content_type                 = HTTP_CONTENT_TYPE_TEXT_PLAIN;
    this->m_handle_req_resp.content_location             = HTTP_CONTENT_LOCATION_STATIC_MEM;
    this->m_handle_req_resp.content_len                  = strlen(content);
    this->m_handle_req_resp.select_location.memory.p_buf = (const uint8_t*)content;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    const string written = this->get_all_written_data();
    // 206 falls back to 200
    ASSERT_NE(string::npos, written.find("HTTP/1.0 200 OK"));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /");
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Falling back to HTTP/1.0 status code 200 for partial content");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_resp_200_with_gzip_no_cache) // NOLINT
{
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    static const char content[]                          = "compressed data";
    this->m_handle_req_resp.http_resp_code               = HTTP_RESP_CODE_200;
    this->m_handle_req_resp.content_type                 = HTTP_CONTENT_TYPE_TEXT_HTML;
    this->m_handle_req_resp.content_location             = HTTP_CONTENT_LOCATION_FLASH_MEM;
    this->m_handle_req_resp.content_len                  = strlen(content);
    this->m_handle_req_resp.content_encoding             = HTTP_CONTENT_ENCODING_GZIP;
    this->m_handle_req_resp.flag_no_cache                = true;
    this->m_handle_req_resp.select_location.memory.p_buf = (const uint8_t*)content;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 200 OK"));
    ASSERT_NE(string::npos, written.find("Content-Encoding: gzip"));
    ASSERT_NE(string::npos, written.find("Cache-Control: no-store"));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnServeHandleReqWithResp, test_hostname_alloc_failure_frees_static_mem) // NOLINT
{
    // STATIC_MEM content → resp_free does nothing (no resource to free), but no crash either
    char            req_buf[]     = "GET / HTTP/1.1\r\nHost: 192.168.1.114\r\n\r\n";
    sta_ip_string_t local_ip_str  = { .buf = "192.168.1.114" };
    sta_ip_string_t remote_ip_str = { .buf = "192.168.1.100" };

    static const char content[]                          = "static content";
    this->m_handle_req_resp.http_resp_code               = HTTP_RESP_CODE_200;
    this->m_handle_req_resp.content_type                 = HTTP_CONTENT_TYPE_TEXT_PLAIN;
    this->m_handle_req_resp.content_location             = HTTP_CONTENT_LOCATION_STATIC_MEM;
    this->m_handle_req_resp.content_len                  = strlen(content);
    this->m_handle_req_resp.select_location.memory.p_buf = (const uint8_t*)content;

    this->m_alloc_fail_on_call_idx = 1;

    http_server_netconn_serve_handle_req(this->m_p_conn, req_buf, &local_ip_str, &remote_ip_str);

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 500 Internal Server Error"));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Request from 192.168.1.100 to 192.168.1.114 (Host: 192.168.1.114): GET /");
    {
        const auto log_record = esp_log_wrapper_pop();
        ASSERT_EQ(ESP_LOG_ERROR, log_record.level);
        ASSERT_NE(string::npos, string(log_record.parsed.msg).find("Failed to allocate memory for hostname string"));
    }
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Response: status 500 (Internal Server Error)");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}
