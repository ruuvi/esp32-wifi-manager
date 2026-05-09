/**
 * @file test_http_server_netconn_resp.cpp
 * @author TheSomeMan
 * @date 2026-05-08
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gtest/gtest.h"
#include "http_server_netconn_resp.h"
#include <string>
#include <vector>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include "esp_err.h"
#include "os_task.h"
#include "wifi_manager_defs.h"
#include "http_server_auth.h"
#include "esp_log_wrapper.hpp"
#include "lwip/tcp.h"
#include "os_malloc.h"
#include "json_stream_gen.h"

using namespace std;

class TestHttpServerNetconnResp;
static TestHttpServerNetconnResp* g_pTestClass;

/*** Google-test class implementation *********************************************************************************/

struct NetconnWriteCall
{
    string  data;
    uint8_t flags;
};

class TestHttpServerNetconnResp : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        os_malloc_trace_init();
        esp_log_wrapper_init();
        g_pTestClass = this;

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

        memset(&g_http_server_extra_header_fields, 0, sizeof(g_http_server_extra_header_fields));

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

    // Allocation tracking
    bool m_flag_alloc_counting_enabled;
    int  m_alloc_free_call_count;
    int  m_alloc_call_cnt;
    int  m_alloc_fail_on_call_idx;

    TestHttpServerNetconnResp();

    ~TestHttpServerNetconnResp() override;

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

TestHttpServerNetconnResp::TestHttpServerNetconnResp()
    : Test()
    , m_p_conn(nullptr)
    , m_mutex_try_lock_result(true)
    , m_tick_idx(0)
    , m_vTaskDelay_called(false)
    , m_vTaskDelay_ticks(0)
    , m_netconn_write_call_idx(0)
    , m_wdt_reset_count(0)
    , m_flag_alloc_counting_enabled(false)
    , m_alloc_free_call_count(0)
    , m_alloc_call_cnt(0)
    , m_alloc_fail_on_call_idx(-1)
{
}

TestHttpServerNetconnResp::~TestHttpServerNetconnResp() = default;

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

#ifdef __cplusplus
}
#endif

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

#define TEST_CHECK_LOG_RECORD(level_, msg_) ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("http_server", level_, msg_)

/*** Unit-Tests *******************************************************************************************************/

// Group 1: conv_lwip_err_to_str

TEST_F(TestHttpServerNetconnResp, test_conv_lwip_err_to_str_all_codes) // NOLINT
{
    ASSERT_STREQ("No error", conv_lwip_err_to_str(ERR_OK));
    ASSERT_STREQ("Out of memory error", conv_lwip_err_to_str(ERR_MEM));
    ASSERT_STREQ("Buffer error", conv_lwip_err_to_str(ERR_BUF));
    ASSERT_STREQ("Timeout", conv_lwip_err_to_str(ERR_TIMEOUT));
    ASSERT_STREQ("Routing problem", conv_lwip_err_to_str(ERR_RTE));
    ASSERT_STREQ("Operation in progress", conv_lwip_err_to_str(ERR_INPROGRESS));
    ASSERT_STREQ("Illegal value", conv_lwip_err_to_str(ERR_VAL));
    ASSERT_STREQ("Operation would block", conv_lwip_err_to_str(ERR_WOULDBLOCK));
    ASSERT_STREQ("Address in use", conv_lwip_err_to_str(ERR_USE));
    ASSERT_STREQ("Already connecting", conv_lwip_err_to_str(ERR_ALREADY));
    ASSERT_STREQ("Conn already established", conv_lwip_err_to_str(ERR_ISCONN));
    ASSERT_STREQ("Not connected", conv_lwip_err_to_str(ERR_CONN));
    ASSERT_STREQ("Low-level netif error", conv_lwip_err_to_str(ERR_IF));
    ASSERT_STREQ("Connection aborted", conv_lwip_err_to_str(ERR_ABRT));
    ASSERT_STREQ("Connection reset", conv_lwip_err_to_str(ERR_RST));
    ASSERT_STREQ("Connection closed", conv_lwip_err_to_str(ERR_CLSD));
    ASSERT_STREQ("Illegal argument", conv_lwip_err_to_str(ERR_ARG));
    ASSERT_STREQ("Unknown error", conv_lwip_err_to_str((err_enum_t)99));

    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

// Group 2: http_server_netconn_write

TEST_F(TestHttpServerNetconnResp, test_netconn_write_success) // NOLINT
{
    this->m_p_conn->send_timeout = 0;
    const char   data[]          = "Hello";
    const size_t data_len        = strlen(data);

    // Two tick calls: one for tick_start, one for the check after write
    this->m_tick_values.push_back(0);
    this->m_tick_values.push_back(0);

    const bool res = http_server_netconn_write(this->m_p_conn, data, data_len, NETCONN_COPY);
    ASSERT_TRUE(res);
    ASSERT_EQ(1, this->m_netconn_writes.size());
    ASSERT_EQ(string("Hello"), this->m_netconn_writes[0].data);
    ASSERT_EQ((uint8_t)(NETCONN_COPY | NETCONN_DONTBLOCK), this->m_netconn_writes[0].flags);
    ASSERT_EQ(1, this->m_wdt_reset_count);
    ASSERT_FALSE(this->m_vTaskDelay_called);

    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerNetconnResp, test_netconn_write_wouldblock_then_success) // NOLINT
{
    this->m_p_conn->send_timeout = 0;
    const char   data[]          = "Test";
    const size_t data_len        = strlen(data);

    // First call: ERR_WOULDBLOCK, second call: ERR_OK
    this->m_netconn_write_errors.push_back(ERR_WOULDBLOCK);
    // ERR_OK for second call (default since vector is exhausted)

    // tick_start + check after first call (no timeout since send_timeout=0) + check after second call
    this->m_tick_values.push_back(0);
    this->m_tick_values.push_back(0);
    this->m_tick_values.push_back(0);

    const bool res = http_server_netconn_write(this->m_p_conn, data, data_len, NETCONN_COPY);
    ASSERT_TRUE(res);
    ASSERT_TRUE(this->m_vTaskDelay_called);
    // Only one successful write (the WOULDBLOCK one didn't write)
    ASSERT_EQ(1, this->m_netconn_writes.size());
    ASSERT_EQ(string("Test"), this->m_netconn_writes[0].data);
    ASSERT_EQ(2, this->m_wdt_reset_count);

    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerNetconnResp, test_netconn_write_error) // NOLINT
{
    this->m_p_conn->send_timeout = 0;
    const char   data[]          = "Data";
    const size_t data_len        = strlen(data);

    this->m_netconn_write_errors.push_back(ERR_CONN);

    this->m_tick_values.push_back(0);

    const bool res = http_server_netconn_write(this->m_p_conn, data, data_len, NETCONN_COPY);
    ASSERT_FALSE(res);
    ASSERT_EQ(0, this->m_netconn_writes.size());

    {
        const auto log_record = esp_log_wrapper_pop();
        ASSERT_EQ(ESP_LOG_ERROR, log_record.level);
        ASSERT_NE(
            string::npos,
            string(log_record.parsed.msg).find("netconn_write_partly failed (Not connected), offset=0, size=4"));
    }
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerNetconnResp, test_netconn_write_send_timeout) // NOLINT
{
    this->m_p_conn->send_timeout = 1000; // 1000 ms
    const char   data[]          = "Data";
    const size_t data_len        = strlen(data);

    // ERR_WOULDBLOCK to not finish in one call, then we'll timeout
    this->m_netconn_write_errors.push_back(ERR_WOULDBLOCK);

    // tick_start = 0, check after wouldblock = 1001 (exceeds timeout of 1000 ticks)
    this->m_tick_values.push_back(0);
    this->m_tick_values.push_back(pdMS_TO_TICKS(1000) + 1);

    const bool res = http_server_netconn_write(this->m_p_conn, data, data_len, NETCONN_COPY);
    ASSERT_FALSE(res);

    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "netconn_write_partly failed: send timeout (1000 ms)");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

// Group 3: http_server_netconn_resp_302

TEST_F(TestHttpServerNetconnResp, test_resp_302_captive_portal) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    // netconn_write needs tick values: tick_start + check after write
    this->m_tick_values.push_back(0);
    this->m_tick_values.push_back(0);

    http_server_netconn_resp_302(this->m_p_conn);

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 302 Found\r\n"));
    ASSERT_NE(string::npos, written.find("Location: http://192.168.1.114/\r\n"));
    ASSERT_NE(string::npos, written.find("Server: Ruuvi Gateway\r\n"));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: status 302 (Found), URL=http://192.168.1.114/");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnResp, test_resp_302_printf_alloc_failure) // NOLINT
{
    this->m_p_conn->send_timeout   = 0;
    this->m_alloc_fail_on_call_idx = 1; // fail the first calloc (str_buf_vprintf_with_alloc)

    http_server_netconn_resp_302(this->m_p_conn);

    ASSERT_EQ(0, this->m_netconn_writes.size());

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: status 302 (Found), URL=http://192.168.1.114/");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "Can't allocate memory for buffer");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "http_server_netconn_printf failed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

// Group 4: http_server_netconn_resp dispatcher

TEST_F(TestHttpServerNetconnResp, test_resp_200_static_mem) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    static const char  body[]         = "{\"status\":\"ok\"}";
    http_server_resp_t resp           = {};
    resp.http_resp_code               = HTTP_RESP_CODE_200;
    resp.content_location             = HTTP_CONTENT_LOCATION_STATIC_MEM;
    resp.content_type                 = HTTP_CONTENT_TYPE_APPLICATION_JSON;
    resp.p_content_type_param         = "";
    resp.content_len                  = strlen(body);
    resp.content_encoding             = HTTP_CONTENT_ENCODING_NONE;
    resp.flag_no_cache                = false;
    resp.flag_add_header_date         = true;
    resp.select_location.memory.p_buf = reinterpret_cast<const uint8_t*>(body);

    // Need tick values for each netconn_write_partly call: header write + body write
    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 200 OK\r\n"));
    ASSERT_NE(string::npos, written.find("Content-type: application/json; charset=utf-8\r\n"));
    ASSERT_NE(string::npos, written.find("Content-Length: 15\r\n"));
    ASSERT_NE(string::npos, written.find("Date:"));
    ASSERT_NE(string::npos, written.find(body));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnResp, test_resp_200_heap) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    const char   src_body[]  = "{\"heap\":true}";
    const size_t body_len    = strlen(src_body);
    char*        p_heap_body = static_cast<char*>(os_malloc(body_len + 1));
    assert(nullptr != p_heap_body);
    memcpy(p_heap_body, src_body, body_len + 1);

    http_server_resp_t resp           = {};
    resp.http_resp_code               = HTTP_RESP_CODE_200;
    resp.content_location             = HTTP_CONTENT_LOCATION_HEAP;
    resp.content_type                 = HTTP_CONTENT_TYPE_APPLICATION_JSON;
    resp.p_content_type_param         = "";
    resp.content_len                  = body_len;
    resp.content_encoding             = HTTP_CONTENT_ENCODING_NONE;
    resp.flag_no_cache                = false;
    resp.flag_add_header_date         = true;
    resp.select_location.memory.p_buf = reinterpret_cast<const uint8_t*>(p_heap_body);

    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 200 OK\r\n"));
    ASSERT_NE(string::npos, written.find(src_body));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    // Heap buffer should have been freed by write_content_from_heap
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnResp, test_resp_200_no_content) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    http_server_resp_t resp   = {};
    resp.http_resp_code       = HTTP_RESP_CODE_200;
    resp.content_location     = HTTP_CONTENT_LOCATION_NO_CONTENT;
    resp.content_type         = HTTP_CONTENT_TYPE_APPLICATION_JSON;
    resp.p_content_type_param = "";
    resp.content_len          = 0;
    resp.content_encoding     = HTTP_CONTENT_ENCODING_NONE;
    resp.flag_no_cache        = false;
    resp.flag_add_header_date = true;

    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 200 OK\r\n"));
    ASSERT_NE(string::npos, written.find("Content-Length: 0\r\n"));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnResp, test_resp_200_content_len_size_max) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    http_server_resp_t resp   = {};
    resp.http_resp_code       = HTTP_RESP_CODE_200;
    resp.content_location     = HTTP_CONTENT_LOCATION_NO_CONTENT;
    resp.content_type         = HTTP_CONTENT_TYPE_APPLICATION_JSON;
    resp.p_content_type_param = "";
    resp.content_len          = SIZE_MAX;
    resp.content_encoding     = HTTP_CONTENT_ENCODING_NONE;
    resp.flag_no_cache        = false;
    resp.flag_add_header_date = true;

    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 200 OK\r\n"));
    // No Content-Length header when content_len == SIZE_MAX
    ASSERT_EQ(string::npos, written.find("Content-Length:"));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerNetconnResp, test_resp_200_gzip_no_cache_content_type_param) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    static const char  body[]         = "{}";
    http_server_resp_t resp           = {};
    resp.http_resp_code               = HTTP_RESP_CODE_200;
    resp.content_location             = HTTP_CONTENT_LOCATION_STATIC_MEM;
    resp.content_type                 = HTTP_CONTENT_TYPE_APPLICATION_JSON;
    resp.p_content_type_param         = "boundary=something";
    resp.content_len                  = strlen(body);
    resp.content_encoding             = HTTP_CONTENT_ENCODING_GZIP;
    resp.flag_no_cache                = true;
    resp.flag_add_header_date         = true;
    resp.select_location.memory.p_buf = reinterpret_cast<const uint8_t*>(body);

    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("Content-Encoding: gzip\r\n"));
    ASSERT_NE(string::npos, written.find("Cache-Control: no-store, no-cache, must-revalidate, max-age=0\r\n"));
    ASSERT_NE(string::npos, written.find("Pragma: no-cache\r\n"));
    ASSERT_NE(string::npos, written.find("; boundary=something\r\n"));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerNetconnResp, test_resp_200_with_extra_header_fields) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    snprintf(
        g_http_server_extra_header_fields.buf,
        sizeof(g_http_server_extra_header_fields.buf),
        "X-Custom: value\r\n");

    static const char  body[]         = "ok";
    http_server_resp_t resp           = {};
    resp.http_resp_code               = HTTP_RESP_CODE_200;
    resp.content_location             = HTTP_CONTENT_LOCATION_STATIC_MEM;
    resp.content_type                 = HTTP_CONTENT_TYPE_TEXT_PLAIN;
    resp.p_content_type_param         = "";
    resp.content_len                  = strlen(body);
    resp.content_encoding             = HTTP_CONTENT_ENCODING_NONE;
    resp.flag_no_cache                = false;
    resp.flag_add_header_date         = true;
    resp.select_location.memory.p_buf = reinterpret_cast<const uint8_t*>(body);

    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("X-Custom: value\r\n"));
    ASSERT_NE(string::npos, written.find("Content-type: text/plain; charset=utf-8\r\n"));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerNetconnResp, test_resp_206_fallback_to_200) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    static const char  body[]         = "partial";
    http_server_resp_t resp           = {};
    resp.http_resp_code               = HTTP_RESP_CODE_206;
    resp.content_location             = HTTP_CONTENT_LOCATION_STATIC_MEM;
    resp.content_type                 = HTTP_CONTENT_TYPE_APPLICATION_OCTET_STREAM;
    resp.p_content_type_param         = "";
    resp.content_len                  = strlen(body);
    resp.content_encoding             = HTTP_CONTENT_ENCODING_NONE;
    resp.flag_no_cache                = false;
    resp.flag_add_header_date         = true;
    resp.select_location.memory.p_buf = reinterpret_cast<const uint8_t*>(body);

    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    // 206 should be remapped to 200
    ASSERT_EQ(HTTP_RESP_CODE_200, resp.http_resp_code);
    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 200 OK\r\n"));

    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Falling back to HTTP/1.0 status code 200 for partial content");
    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerNetconnResp, test_resp_301_auth_redirect) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    http_server_resp_t resp = {};
    resp.http_resp_code     = HTTP_RESP_CODE_301;

    snprintf(g_http_server_extra_header_fields.buf, sizeof(g_http_server_extra_header_fields.buf), "X-Auth: token\r\n");

    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "gw.local");

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 301 Moved Permanently\r\n"));
    ASSERT_NE(string::npos, written.find("Location: http://gw.local/#auth\r\n"));
    ASSERT_NE(string::npos, written.find("X-Auth: token\r\n"));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: status 301 (Moved Permanently), URL=http://gw.local/#auth");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerNetconnResp, test_resp_302_auth_redirect) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    http_server_resp_t resp = {};
    resp.http_resp_code     = HTTP_RESP_CODE_302;

    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "gw.local");

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 302 Found\r\n"));
    ASSERT_NE(string::npos, written.find("Location: http://gw.local/#auth\r\n"));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: status 302 (Found), URL=http://gw.local/#auth");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerNetconnResp, test_resp_400_without_content) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    http_server_resp_t resp   = {};
    resp.http_resp_code       = HTTP_RESP_CODE_400;
    resp.content_location     = HTTP_CONTENT_LOCATION_NO_CONTENT;
    resp.content_type         = HTTP_CONTENT_TYPE_APPLICATION_JSON;
    resp.p_content_type_param = "";
    resp.content_len          = 0;
    resp.content_encoding     = HTTP_CONTENT_ENCODING_NONE;

    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 400 Bad Request\r\n"));
    // resp_without_content sends "{}" as body
    ASSERT_NE(string::npos, written.find("{}"));

    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Response: status 400 (Bad Request)");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerNetconnResp, test_resp_400_with_content) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    static const char  body[]         = "{\"error\":\"bad\"}";
    http_server_resp_t resp           = {};
    resp.http_resp_code               = HTTP_RESP_CODE_400;
    resp.content_location             = HTTP_CONTENT_LOCATION_STATIC_MEM;
    resp.content_type                 = HTTP_CONTENT_TYPE_APPLICATION_JSON;
    resp.p_content_type_param         = "";
    resp.content_len                  = strlen(body);
    resp.content_encoding             = HTTP_CONTENT_ENCODING_NONE;
    resp.flag_no_cache                = false;
    resp.flag_add_header_date         = true;
    resp.select_location.memory.p_buf = reinterpret_cast<const uint8_t*>(body);

    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 400 Bad Request\r\n"));
    ASSERT_NE(string::npos, written.find(body));

    // resp_with_content for non-200 logs WARN
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Response: status 400 (Bad Request), extra header fields:\n");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerNetconnResp, test_resp_401_403) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    snprintf(
        g_http_server_extra_header_fields.buf,
        sizeof(g_http_server_extra_header_fields.buf),
        "WWW-Authenticate: Basic\r\n");

    static const char  body[]         = "{\"auth\":false}";
    http_server_resp_t resp           = {};
    resp.http_resp_code               = HTTP_RESP_CODE_401;
    resp.content_location             = HTTP_CONTENT_LOCATION_STATIC_MEM;
    resp.content_type                 = HTTP_CONTENT_TYPE_APPLICATION_JSON;
    resp.p_content_type_param         = "";
    resp.content_len                  = strlen(body);
    resp.content_encoding             = HTTP_CONTENT_ENCODING_NONE;
    resp.flag_no_cache                = false;
    resp.flag_add_header_date         = true;
    resp.select_location.memory.p_buf = reinterpret_cast<const uint8_t*>(body);

    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 401 Unauthorized\r\n"));
    ASSERT_NE(string::npos, written.find("WWW-Authenticate: Basic\r\n"));

    TEST_CHECK_LOG_RECORD(
        ESP_LOG_WARN,
        "Response: status 401 (Unauthorized), extra header fields:\nWWW-Authenticate: Basic\r\n");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    // Now test 403
    this->m_netconn_writes.clear();
    this->m_netconn_write_call_idx = 0;
    this->m_tick_idx               = 0;
    esp_log_wrapper_clear();

    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    resp.http_resp_code = HTTP_RESP_CODE_403;

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 403 Forbidden\r\n"));

    TEST_CHECK_LOG_RECORD(
        ESP_LOG_WARN,
        "Response: status 403 (Forbidden), extra header fields:\nWWW-Authenticate: Basic\r\n");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerNetconnResp, test_resp_404_409_429_500_502_503_504) // NOLINT
{
    struct
    {
        http_resp_code_e code;
        const char*      status_msg;
    } codes[] = {
        { HTTP_RESP_CODE_404, "Not Found" },         { HTTP_RESP_CODE_409, "Conflict" },
        { HTTP_RESP_CODE_429, "Too Many Requests" }, { HTTP_RESP_CODE_500, "Internal Server Error" },
        { HTTP_RESP_CODE_502, "Bad Gateway" },       { HTTP_RESP_CODE_503, "Service Unavailable" },
        { HTTP_RESP_CODE_504, "Gateway timeout" },
    };

    for (const auto& tc : codes)
    {
        this->m_netconn_writes.clear();
        this->m_netconn_write_call_idx = 0;
        this->m_tick_idx               = 0;
        this->m_tick_values.clear();
        esp_log_wrapper_clear();

        this->m_p_conn->send_timeout = 0;

        http_server_resp_t resp   = {};
        resp.http_resp_code       = tc.code;
        resp.content_location     = HTTP_CONTENT_LOCATION_NO_CONTENT;
        resp.content_type         = HTTP_CONTENT_TYPE_APPLICATION_JSON;
        resp.p_content_type_param = "";
        resp.content_len          = 0;
        resp.content_encoding     = HTTP_CONTENT_ENCODING_NONE;

        for (int i = 0; i < 10; i++)
        {
            this->m_tick_values.push_back(0);
        }

        http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

        const string written = this->get_all_written_data();
        char         expected_status[128];
        snprintf(expected_status, sizeof(expected_status), "HTTP/1.0 %u %s\r\n", (unsigned)tc.code, tc.status_msg);
        ASSERT_NE(string::npos, written.find(expected_status)) << "Failed for code " << (unsigned)tc.code;

        char expected_log[128];
        snprintf(expected_log, sizeof(expected_log), "Response: status %u (%s)", (unsigned)tc.code, tc.status_msg);
        TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, expected_log);
        ASSERT_TRUE(esp_log_wrapper_is_empty()) << "Failed for code " << (unsigned)tc.code;
    }

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerNetconnResp, test_resp_299_treated_as_200) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    static const char  body[]         = "ok";
    http_server_resp_t resp           = {};
    resp.http_resp_code               = HTTP_RESP_CODE_299;
    resp.content_location             = HTTP_CONTENT_LOCATION_STATIC_MEM;
    resp.content_type                 = HTTP_CONTENT_TYPE_TEXT_PLAIN;
    resp.p_content_type_param         = "";
    resp.content_len                  = strlen(body);
    resp.content_encoding             = HTTP_CONTENT_ENCODING_NONE;
    resp.flag_no_cache                = false;
    resp.flag_add_header_date         = true;
    resp.select_location.memory.p_buf = reinterpret_cast<const uint8_t*>(body);

    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    const string written = this->get_all_written_data();
    // 299 is handled by http_server_netconn_resp_200 which hardcodes HTTP_RESP_CODE_200
    ASSERT_NE(string::npos, written.find("HTTP/1.0 200 OK\r\n"));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

// Group 5: write_content_from_fatfs

TEST_F(TestHttpServerNetconnResp, test_resp_200_fatfs_single_chunk) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    const char   file_data[] = "Hello from file";
    const size_t data_len    = strlen(file_data);

    int pipefd[2];
    ASSERT_EQ(0, pipe(pipefd));
    ssize_t wr = write(pipefd[1], file_data, data_len);
    ASSERT_EQ((ssize_t)data_len, wr);
    close(pipefd[1]);

    http_server_resp_t resp       = {};
    resp.http_resp_code           = HTTP_RESP_CODE_200;
    resp.content_location         = HTTP_CONTENT_LOCATION_FATFS;
    resp.content_type             = HTTP_CONTENT_TYPE_TEXT_HTML;
    resp.p_content_type_param     = "";
    resp.content_len              = data_len;
    resp.content_encoding         = HTTP_CONTENT_ENCODING_NONE;
    resp.flag_no_cache            = false;
    resp.flag_add_header_date     = true;
    resp.select_location.fatfs.fd = pipefd[0];

    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 200 OK\r\n"));
    ASSERT_NE(string::npos, written.find(file_data));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerNetconnResp, test_resp_200_fatfs_multi_chunk) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    // Create data larger than HTTP_SERVER_TX_CHUNK_SIZE (1536)
    const size_t data_len = 2000;
    char*        p_data   = static_cast<char*>(malloc(data_len));
    assert(nullptr != p_data);
    memset(p_data, 'A', data_len);

    int pipefd[2];
    ASSERT_EQ(0, pipe(pipefd));
    ssize_t wr = write(pipefd[1], p_data, data_len);
    ASSERT_EQ((ssize_t)data_len, wr);
    close(pipefd[1]);
    free(p_data);

    http_server_resp_t resp       = {};
    resp.http_resp_code           = HTTP_RESP_CODE_200;
    resp.content_location         = HTTP_CONTENT_LOCATION_FATFS;
    resp.content_type             = HTTP_CONTENT_TYPE_APPLICATION_OCTET_STREAM;
    resp.p_content_type_param     = "";
    resp.content_len              = data_len;
    resp.content_encoding         = HTTP_CONTENT_ENCODING_NONE;
    resp.flag_no_cache            = false;
    resp.flag_add_header_date     = true;
    resp.select_location.fatfs.fd = pipefd[0];

    for (int i = 0; i < 20; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 200 OK\r\n"));
    // Count total body bytes written (subtract the header write)
    size_t header_end = written.find("\r\n\r\n");
    ASSERT_NE(string::npos, header_end);
    size_t body_len = written.size() - (header_end + 4);
    ASSERT_EQ(data_len, body_len);

    // Should have multiple content writes (header + at least 2 content chunks)
    ASSERT_GE(this->m_netconn_writes.size(), (size_t)3);

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerNetconnResp, test_resp_200_fatfs_read_failure) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    // Use a closed fd so read() returns -1
    int pipefd[2];
    ASSERT_EQ(0, pipe(pipefd));
    close(pipefd[0]);
    close(pipefd[1]);

    http_server_resp_t resp       = {};
    resp.http_resp_code           = HTTP_RESP_CODE_200;
    resp.content_location         = HTTP_CONTENT_LOCATION_FATFS;
    resp.content_type             = HTTP_CONTENT_TYPE_TEXT_HTML;
    resp.p_content_type_param     = "";
    resp.content_len              = 100;
    resp.content_encoding         = HTTP_CONTENT_ENCODING_NONE;
    resp.flag_no_cache            = false;
    resp.flag_add_header_date     = true;
    resp.select_location.fatfs.fd = pipefd[0]; // closed fd

    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "Failed to read 100 bytes");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerNetconnResp, test_resp_200_fatfs_malloc_failure) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    // Create a valid pipe
    int pipefd[2];
    ASSERT_EQ(0, pipe(pipefd));
    close(pipefd[1]);

    http_server_resp_t resp       = {};
    resp.http_resp_code           = HTTP_RESP_CODE_200;
    resp.content_location         = HTTP_CONTENT_LOCATION_FATFS;
    resp.content_type             = HTTP_CONTENT_TYPE_TEXT_HTML;
    resp.p_content_type_param     = "";
    resp.content_len              = 100;
    resp.content_encoding         = HTTP_CONTENT_ENCODING_NONE;
    resp.flag_no_cache            = false;
    resp.flag_add_header_date     = true;
    resp.select_location.fatfs.fd = pipefd[0];

    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    // The header printf will allocate, then write_content_from_fatfs will try os_malloc for tmp buffer.
    // We need to fail the right allocation. The printf alloc is via str_buf_vprintf_with_alloc (calloc).
    // The fatfs tmp buffer is via os_malloc → os_calloc_internal → calloc.
    // Let's fail on alloc #2 (first alloc is the header printf, second is the fatfs tmp buf).
    this->m_alloc_fail_on_call_idx = 2;

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    close(pipefd[0]);

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "Can't allocate memory for temporary buffer");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

// Group 6: write_content_from_json_generator

static json_stream_gen_callback_result_t
test_json_gen_cb(json_stream_gen_t* const p_gen, const void* const p_user_ctx)
{
    (void)p_user_ctx;
    JSON_STREAM_GEN_START_OBJECT(p_gen, NULL);
    JSON_STREAM_GEN_ADD_STRING(p_gen, "key", "value");
    JSON_STREAM_GEN_END_OBJECT(p_gen);
    JSON_STREAM_GEN_END_GENERATOR_FUNC();
}

TEST_F(TestHttpServerNetconnResp, test_resp_200_json_generator_small) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    json_stream_gen_cfg_t cfg = {};
    cfg.max_chunk_size        = 256;
    cfg.flag_formatted_json   = false;
    cfg.max_nesting_level     = 3;
    cfg.p_malloc              = &test_os_malloc;
    cfg.p_free                = &test_os_free;
    cfg.p_localeconv          = &localeconv;

    void*              p_ctx      = nullptr;
    json_stream_gen_t* p_json_gen = json_stream_gen_create(&cfg, &test_json_gen_cb, 0, &p_ctx);
    ASSERT_NE(nullptr, p_json_gen);

    const json_stream_gen_size_t json_size = json_stream_gen_calc_size(p_json_gen);
    ASSERT_GT(json_size, 0);
    json_stream_gen_reset(p_json_gen);

    http_server_resp_t resp                        = {};
    resp.http_resp_code                            = HTTP_RESP_CODE_200;
    resp.content_location                          = HTTP_CONTENT_LOCATION_JSON_GENERATOR;
    resp.content_type                              = HTTP_CONTENT_TYPE_APPLICATION_JSON;
    resp.p_content_type_param                      = "";
    resp.content_len                               = (size_t)json_size; // small, < 4096
    resp.content_encoding                          = HTTP_CONTENT_ENCODING_NONE;
    resp.flag_no_cache                             = false;
    resp.flag_add_header_date                      = true;
    resp.select_location.json_generator.p_json_gen = p_json_gen;

    for (int i = 0; i < 20; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("HTTP/1.0 200 OK\r\n"));
    ASSERT_NE(string::npos, written.find("{\"key\":\"value\"}"));

    // vTaskDelay should have been called between chunks
    ASSERT_TRUE(this->m_vTaskDelay_called);

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    // Small content → LOG_INFO with content
    // Verify the json_stream_gen log
    {
        const auto log_record = esp_log_wrapper_pop();
        ASSERT_EQ(ESP_LOG_INFO, log_record.level);
        ASSERT_NE(string::npos, string(log_record.parsed.msg).find("json_stream_gen: send"));
        ASSERT_NE(string::npos, string(log_record.parsed.msg).find("key"));
        ASSERT_NE(string::npos, string(log_record.parsed.msg).find("value"));
    }
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerNetconnResp, test_resp_200_json_generator_large) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    json_stream_gen_cfg_t cfg = {};
    cfg.max_chunk_size        = 256;
    cfg.flag_formatted_json   = false;
    cfg.max_nesting_level     = 3;
    cfg.p_malloc              = &test_os_malloc;
    cfg.p_free                = &test_os_free;
    cfg.p_localeconv          = &localeconv;

    void*              p_ctx      = nullptr;
    json_stream_gen_t* p_json_gen = json_stream_gen_create(&cfg, &test_json_gen_cb, 0, &p_ctx);
    ASSERT_NE(nullptr, p_json_gen);

    http_server_resp_t resp   = {};
    resp.http_resp_code       = HTTP_RESP_CODE_200;
    resp.content_location     = HTTP_CONTENT_LOCATION_JSON_GENERATOR;
    resp.content_type         = HTTP_CONTENT_TYPE_APPLICATION_JSON;
    resp.p_content_type_param = "";
    // Set content_len >= 4096 to trigger LOG_DBG path instead of LOG_INFO
    resp.content_len                               = 4 * 1024;
    resp.content_encoding                          = HTTP_CONTENT_ENCODING_NONE;
    resp.flag_no_cache                             = false;
    resp.flag_add_header_date                      = true;
    resp.select_location.json_generator.p_json_gen = p_json_gen;

    for (int i = 0; i < 20; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    const string written = this->get_all_written_data();
    ASSERT_NE(string::npos, written.find("{\"key\":\"value\"}"));

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    // Large content → LOG_DBG (not visible at LOG_LEVEL_INFO), so no json_stream_gen log
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerNetconnResp, test_resp_200_json_generator_write_failure) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    json_stream_gen_cfg_t cfg = {};
    cfg.max_chunk_size        = 256;
    cfg.flag_formatted_json   = false;
    cfg.max_nesting_level     = 3;
    cfg.p_malloc              = &test_os_malloc;
    cfg.p_free                = &test_os_free;
    cfg.p_localeconv          = &localeconv;

    void*              p_ctx      = nullptr;
    json_stream_gen_t* p_json_gen = json_stream_gen_create(&cfg, &test_json_gen_cb, 0, &p_ctx);
    ASSERT_NE(nullptr, p_json_gen);

    http_server_resp_t resp                        = {};
    resp.http_resp_code                            = HTTP_RESP_CODE_200;
    resp.content_location                          = HTTP_CONTENT_LOCATION_JSON_GENERATOR;
    resp.content_type                              = HTTP_CONTENT_TYPE_APPLICATION_JSON;
    resp.p_content_type_param                      = "";
    resp.content_len                               = 100;
    resp.content_encoding                          = HTTP_CONTENT_ENCODING_NONE;
    resp.flag_no_cache                             = false;
    resp.flag_add_header_date                      = true;
    resp.select_location.json_generator.p_json_gen = p_json_gen;

    // Header write succeeds, content write fails
    this->m_netconn_write_errors.push_back(ERR_OK);   // header
    this->m_netconn_write_errors.push_back(ERR_CONN); // json content

    for (int i = 0; i < 20; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    // json_stream_gen chunk logged before write attempt
    {
        const auto log_record = esp_log_wrapper_pop();
        ASSERT_EQ(ESP_LOG_INFO, log_record.level);
        ASSERT_NE(string::npos, string(log_record.parsed.msg).find("json_stream_gen: send"));
    }
    // Write failure
    {
        const auto log_record = esp_log_wrapper_pop();
        ASSERT_EQ(ESP_LOG_ERROR, log_record.level);
        ASSERT_NE(string::npos, string(log_record.parsed.msg).find("netconn_write_partly failed"));
    }
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "http_server_netconn_write failed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

// Group 7: Error in content write

TEST_F(TestHttpServerNetconnResp, test_resp_200_netconn_write_failure_during_content) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    static const char  body[]         = "body data here";
    http_server_resp_t resp           = {};
    resp.http_resp_code               = HTTP_RESP_CODE_200;
    resp.content_location             = HTTP_CONTENT_LOCATION_STATIC_MEM;
    resp.content_type                 = HTTP_CONTENT_TYPE_TEXT_PLAIN;
    resp.p_content_type_param         = "";
    resp.content_len                  = strlen(body);
    resp.content_encoding             = HTTP_CONTENT_ENCODING_NONE;
    resp.flag_no_cache                = false;
    resp.flag_add_header_date         = true;
    resp.select_location.memory.p_buf = reinterpret_cast<const uint8_t*>(body);

    // First write (header) succeeds, second write (body) fails
    this->m_netconn_write_errors.push_back(ERR_OK);   // header
    this->m_netconn_write_errors.push_back(ERR_CONN); // body

    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    // write failure in body → LOG_ERR from http_server_netconn_write
    {
        const auto log_record = esp_log_wrapper_pop();
        ASSERT_EQ(ESP_LOG_ERROR, log_record.level);
        ASSERT_NE(
            string::npos,
            string(log_record.parsed.msg).find("netconn_write_partly failed (Not connected), offset=0, size=14"));
    }
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "http_server_netconn_write failed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

// Additional: resp_without_content write failure

TEST_F(TestHttpServerNetconnResp, test_resp_without_content_write_failure) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    http_server_resp_t resp   = {};
    resp.http_resp_code       = HTTP_RESP_CODE_500;
    resp.content_location     = HTTP_CONTENT_LOCATION_NO_CONTENT;
    resp.content_type         = HTTP_CONTENT_TYPE_APPLICATION_JSON;
    resp.p_content_type_param = "";
    resp.content_len          = 0;
    resp.content_encoding     = HTTP_CONTENT_ENCODING_NONE;

    // Fail the write
    this->m_netconn_write_errors.push_back(ERR_CONN);

    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Response: status 500 (Internal Server Error)");
    {
        const auto log_record = esp_log_wrapper_pop();
        ASSERT_EQ(ESP_LOG_ERROR, log_record.level);
        ASSERT_NE(string::npos, string(log_record.parsed.msg).find("netconn_write_partly failed (Not connected)"));
    }
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "http_server_netconn_write failed");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "http_server_netconn_printf failed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

// Additional: content_with_len header write failure

TEST_F(TestHttpServerNetconnResp, test_resp_200_header_write_failure) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    // Fail the alloc for header printf
    this->m_alloc_fail_on_call_idx = 1;

    static const char  body[]         = "data";
    http_server_resp_t resp           = {};
    resp.http_resp_code               = HTTP_RESP_CODE_200;
    resp.content_location             = HTTP_CONTENT_LOCATION_STATIC_MEM;
    resp.content_type                 = HTTP_CONTENT_TYPE_TEXT_PLAIN;
    resp.p_content_type_param         = "";
    resp.content_len                  = strlen(body);
    resp.content_encoding             = HTTP_CONTENT_ENCODING_NONE;
    resp.flag_no_cache                = false;
    resp.flag_add_header_date         = true;
    resp.select_location.memory.p_buf = reinterpret_cast<const uint8_t*>(body);

    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "Can't allocate memory for buffer");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "http_server_netconn_printf failed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

// Additional coverage tests

TEST_F(TestHttpServerNetconnResp, test_resp_200_heap_write_failure) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    const char   src_body[]  = "{\"heap\":true}";
    const size_t body_len    = strlen(src_body);
    char*        p_heap_body = static_cast<char*>(os_malloc(body_len + 1));
    assert(nullptr != p_heap_body);
    memcpy(p_heap_body, src_body, body_len + 1);

    http_server_resp_t resp           = {};
    resp.http_resp_code               = HTTP_RESP_CODE_200;
    resp.content_location             = HTTP_CONTENT_LOCATION_HEAP;
    resp.content_type                 = HTTP_CONTENT_TYPE_APPLICATION_JSON;
    resp.p_content_type_param         = "";
    resp.content_len                  = body_len;
    resp.content_encoding             = HTTP_CONTENT_ENCODING_NONE;
    resp.flag_no_cache                = false;
    resp.flag_add_header_date         = true;
    resp.select_location.memory.p_buf = reinterpret_cast<const uint8_t*>(p_heap_body);

    // Header write succeeds, body write fails
    this->m_netconn_write_errors.push_back(ERR_OK);   // header
    this->m_netconn_write_errors.push_back(ERR_CONN); // body

    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    {
        const auto log_record = esp_log_wrapper_pop();
        ASSERT_EQ(ESP_LOG_ERROR, log_record.level);
        ASSERT_NE(string::npos, string(log_record.parsed.msg).find("netconn_write_partly failed"));
    }
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "http_server_netconn_write failed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    // Heap buffer must still be freed even on write failure
    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerNetconnResp, test_resp_200_fatfs_partial_read) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    const size_t actual_data_len = 50;
    char         data[50];
    memset(data, 'B', actual_data_len);

    int pipefd[2];
    ASSERT_EQ(0, pipe(pipefd));
    ssize_t wr = write(pipefd[1], data, actual_data_len);
    ASSERT_EQ((ssize_t)actual_data_len, wr);
    close(pipefd[1]);

    http_server_resp_t resp       = {};
    resp.http_resp_code           = HTTP_RESP_CODE_200;
    resp.content_location         = HTTP_CONTENT_LOCATION_FATFS;
    resp.content_type             = HTTP_CONTENT_TYPE_APPLICATION_OCTET_STREAM;
    resp.p_content_type_param     = "";
    resp.content_len              = 100; // more than available
    resp.content_encoding         = HTTP_CONTENT_ENCODING_NONE;
    resp.flag_no_cache            = false;
    resp.flag_add_header_date     = true;
    resp.select_location.fatfs.fd = pipefd[0];

    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    {
        const auto log_record = esp_log_wrapper_pop();
        ASSERT_EQ(ESP_LOG_ERROR, log_record.level);
        ASSERT_NE(string::npos, string(log_record.parsed.msg).find("Read 50 bytes, while requested 100 bytes"));
    }
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerNetconnResp, test_resp_200_fatfs_write_failure) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    const char   file_data[] = "file content";
    const size_t data_len    = strlen(file_data);

    int pipefd[2];
    ASSERT_EQ(0, pipe(pipefd));
    ssize_t wr = write(pipefd[1], file_data, data_len);
    ASSERT_EQ((ssize_t)data_len, wr);
    close(pipefd[1]);

    http_server_resp_t resp       = {};
    resp.http_resp_code           = HTTP_RESP_CODE_200;
    resp.content_location         = HTTP_CONTENT_LOCATION_FATFS;
    resp.content_type             = HTTP_CONTENT_TYPE_TEXT_HTML;
    resp.p_content_type_param     = "";
    resp.content_len              = data_len;
    resp.content_encoding         = HTTP_CONTENT_ENCODING_NONE;
    resp.flag_no_cache            = false;
    resp.flag_add_header_date     = true;
    resp.select_location.fatfs.fd = pipefd[0];

    // Header write succeeds, fatfs content write fails
    this->m_netconn_write_errors.push_back(ERR_OK);   // header
    this->m_netconn_write_errors.push_back(ERR_CONN); // fatfs content

    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    {
        const auto log_record = esp_log_wrapper_pop();
        ASSERT_EQ(ESP_LOG_ERROR, log_record.level);
        ASSERT_NE(string::npos, string(log_record.parsed.msg).find("netconn_write_partly failed"));
    }
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "http_server_netconn_write failed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerNetconnResp, test_resp_200_content_without_len_write_failure) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    http_server_resp_t resp   = {};
    resp.http_resp_code       = HTTP_RESP_CODE_200;
    resp.content_location     = HTTP_CONTENT_LOCATION_NO_CONTENT;
    resp.content_type         = HTTP_CONTENT_TYPE_TEXT_PLAIN;
    resp.p_content_type_param = "";
    resp.content_len          = SIZE_MAX; // triggers content_without_len path
    resp.content_encoding     = HTTP_CONTENT_ENCODING_NONE;
    resp.flag_no_cache        = false;
    resp.flag_add_header_date = true;

    // Fail the write for the header
    this->m_netconn_write_errors.push_back(ERR_CONN);

    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
    {
        const auto log_record = esp_log_wrapper_pop();
        ASSERT_EQ(ESP_LOG_ERROR, log_record.level);
        ASSERT_NE(string::npos, string(log_record.parsed.msg).find("netconn_write_partly failed"));
    }
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "http_server_netconn_write failed");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "http_server_netconn_printf failed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerNetconnResp, test_resp_301_auth_redirect_write_failure) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    http_server_resp_t resp = {};
    resp.http_resp_code     = HTTP_RESP_CODE_301;

    this->m_netconn_write_errors.push_back(ERR_CONN);

    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "gw.local");

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: status 301 (Moved Permanently), URL=http://gw.local/#auth");
    {
        const auto log_record = esp_log_wrapper_pop();
        ASSERT_EQ(ESP_LOG_ERROR, log_record.level);
        ASSERT_NE(string::npos, string(log_record.parsed.msg).find("netconn_write_partly failed"));
    }
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "http_server_netconn_write failed");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "http_server_netconn_printf failed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerNetconnResp, test_resp_302_auth_redirect_write_failure) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    http_server_resp_t resp = {};
    resp.http_resp_code     = HTTP_RESP_CODE_302;

    this->m_netconn_write_errors.push_back(ERR_CONN);

    for (int i = 0; i < 10; i++)
    {
        this->m_tick_values.push_back(0);
    }

    http_server_netconn_resp(this->m_p_conn, &resp, "gw.local");

    TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: status 302 (Found), URL=http://gw.local/#auth");
    {
        const auto log_record = esp_log_wrapper_pop();
        ASSERT_EQ(ESP_LOG_ERROR, log_record.level);
        ASSERT_NE(string::npos, string(log_record.parsed.msg).find("netconn_write_partly failed"));
    }
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "http_server_netconn_write failed");
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "http_server_netconn_printf failed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}

TEST_F(TestHttpServerNetconnResp, test_content_type_css_js_png_svg) // NOLINT
{
    this->m_p_conn->send_timeout = 0;

    static const char body[] = "x";

    struct
    {
        http_content_type_e type;
        const char*         expected_str;
        bool                expect_charset;
    } types[] = {
        { HTTP_CONTENT_TYPE_TEXT_CSS, "text/css", true },
        { HTTP_CONTENT_TYPE_TEXT_JAVASCRIPT, "text/javascript", true },
        { HTTP_CONTENT_TYPE_IMAGE_PNG, "image/png", false },
        { HTTP_CONTENT_TYPE_IMAGE_SVG_XML, "image/svg+xml", false },
    };

    for (const auto& tc : types)
    {
        this->m_netconn_writes.clear();
        this->m_netconn_write_call_idx = 0;
        this->m_tick_idx               = 0;
        this->m_tick_values.clear();
        esp_log_wrapper_clear();

        http_server_resp_t resp           = {};
        resp.http_resp_code               = HTTP_RESP_CODE_200;
        resp.content_location             = HTTP_CONTENT_LOCATION_STATIC_MEM;
        resp.content_type                 = tc.type;
        resp.p_content_type_param         = "";
        resp.content_len                  = strlen(body);
        resp.content_encoding             = HTTP_CONTENT_ENCODING_NONE;
        resp.flag_no_cache                = false;
        resp.flag_add_header_date         = true;
        resp.select_location.memory.p_buf = reinterpret_cast<const uint8_t*>(body);

        for (int i = 0; i < 10; i++)
        {
            this->m_tick_values.push_back(0);
        }

        http_server_netconn_resp(this->m_p_conn, &resp, "myhost.local");

        const string written = this->get_all_written_data();
        char         expected[128];
        if (tc.expect_charset)
        {
            snprintf(expected, sizeof(expected), "Content-type: %s; charset=utf-8\r\n", tc.expected_str);
        }
        else
        {
            snprintf(expected, sizeof(expected), "Content-type: %s\r\n", tc.expected_str);
        }
        ASSERT_NE(string::npos, written.find(expected)) << "Failed for content type: " << tc.expected_str;

        TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Response: OK");
        ASSERT_TRUE(esp_log_wrapper_is_empty()) << "Failed for: " << tc.expected_str;
    }

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}
