/**
 * @file test_http_server_accept_and_handle_conn.cpp
 * @author TheSomeMan
 * @date 2026-05-07
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gtest/gtest.h"
#include "http_server_accept_and_handle_conn.h"
#include <string>
#include <vector>
#include <cstring>
#include <cassert>
#include "esp_err.h"
#include "os_task.h"
#include "wifi_manager_defs.h"
#include "http_server_auth.h"
#include "esp_log_wrapper.hpp"
#include "lwip/tcp.h"
#include "os_malloc.h"

using namespace std;

class TestHttpServerAcceptAndHandleConn;
static TestHttpServerAcceptAndHandleConn* g_pTestClass;

/*** Types for stub configuration *********************************************************************************/

struct RecvFrame
{
    err_t          err;
    vector<string> fragments;
};

struct ServeHandleReqCapture
{
    bool   called;
    string req_buf;
    string local_ip;
    string remote_ip;
};

/*** Google-test class implementation *********************************************************************************/

class TestHttpServerAcceptAndHandleConn : public ::testing::Test
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

        this->m_netconn_accept_err = ERR_ARG;
        this->m_p_new_conn         = nullptr;
        memset(&this->m_tcp_pcb_for_new_conn, 0, sizeof(this->m_tcp_pcb_for_new_conn));

        this->m_recv_frames.clear();
        this->m_recv_frame_idx = 0;

        this->m_tick_values.clear();
        this->m_tick_idx = 0;

        this->m_netconn_close_err  = ERR_OK;
        this->m_netconn_delete_err = ERR_OK;

        this->m_vTaskDelay_called = false;
        this->m_vTaskDelay_ticks  = 0;

        this->m_serve_capture.called    = false;
        this->m_serve_capture.req_buf   = "";
        this->m_serve_capture.local_ip  = "";
        this->m_serve_capture.remote_ip = "";

        this->m_netconn_delete_call_count = 0;

        // Reset the http_server mutex to NULL
        http_server_use_mutex_for_incoming_connection_handling(nullptr);
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
        http_server_use_mutex_for_incoming_connection_handling(nullptr);
        if (nullptr != this->m_p_new_conn)
        {
            free(this->m_p_new_conn);
            this->m_p_new_conn = nullptr;
        }
        free(this->m_p_conn);
        this->m_p_conn = nullptr;
        g_pTestClass   = nullptr;
        esp_log_wrapper_deinit();
    }

    void
    setup_accept_success_with_valid_pcb()
    {
        this->m_netconn_accept_err = ERR_OK;
        this->m_p_new_conn         = static_cast<struct netconn*>(calloc(1, sizeof(struct netconn)));
        assert(nullptr != this->m_p_new_conn);
        // Set local IP: 192.168.1.1
        const uint8_t local_ip_bytes[] = { 192, 168, 1, 1 };
        memcpy(&this->m_tcp_pcb_for_new_conn.local_ip.u_addr.ip4.addr, local_ip_bytes, 4);
        this->m_tcp_pcb_for_new_conn.local_ip.type = IPADDR_TYPE_V4;
        // Set remote IP: 192.168.1.100
        const uint8_t remote_ip_bytes[] = { 192, 168, 1, 100 };
        memcpy(&this->m_tcp_pcb_for_new_conn.remote_ip.u_addr.ip4.addr, remote_ip_bytes, 4);
        this->m_tcp_pcb_for_new_conn.remote_ip.type = IPADDR_TYPE_V4;

        this->m_p_new_conn->pcb.tcp = &this->m_tcp_pcb_for_new_conn;
    }

    void
    add_recv_frame(const string& data)
    {
        this->m_recv_frames.push_back({ ERR_OK, { data } });
    }

    void
    add_recv_frame_multi_pbuf(const vector<string>& fragments)
    {
        assert(!fragments.empty());
        this->m_recv_frames.push_back({ ERR_OK, fragments });
    }

    void
    add_recv_error(const err_t err)
    {
        this->m_recv_frames.push_back({ err, {} });
    }

public:
    struct netconn* m_p_conn;

    // Mutex stub config
    bool m_mutex_try_lock_result;

    // Accept stub config
    err_t           m_netconn_accept_err;
    struct netconn* m_p_new_conn;
    struct tcp_pcb  m_tcp_pcb_for_new_conn;

    // Recv stub config
    vector<RecvFrame> m_recv_frames;
    size_t            m_recv_frame_idx;

    // Tick count stub config
    vector<TickType_t> m_tick_values;
    size_t             m_tick_idx;

    // Close/delete stub config
    err_t m_netconn_close_err;
    err_t m_netconn_delete_err;

    // Tracking
    bool       m_vTaskDelay_called;
    TickType_t m_vTaskDelay_ticks;
    int        m_netconn_delete_call_count;

    // Capture from serve_handle_req
    ServeHandleReqCapture m_serve_capture;

    bool m_flag_alloc_counting_enabled;
    int  m_alloc_free_call_count;
    int  m_alloc_call_cnt;
    int  m_alloc_fail_on_call_idx;

    TestHttpServerAcceptAndHandleConn();

    ~TestHttpServerAcceptAndHandleConn() override;
};

TestHttpServerAcceptAndHandleConn::TestHttpServerAcceptAndHandleConn()
    : Test()
    , m_p_conn(nullptr)
    , m_mutex_try_lock_result(true)
    , m_netconn_accept_err(ERR_ARG)
    , m_p_new_conn(nullptr)
    , m_tcp_pcb_for_new_conn()
    , m_recv_frame_idx(0)
    , m_tick_idx(0)
    , m_netconn_close_err(ERR_OK)
    , m_netconn_delete_err(ERR_OK)
    , m_vTaskDelay_called(false)
    , m_vTaskDelay_ticks(0)
    , m_netconn_delete_call_count(0)
    , m_serve_capture()
    , m_flag_alloc_counting_enabled(false)
    , m_alloc_free_call_count(0)
    , m_alloc_call_cnt(0)
    , m_alloc_fail_on_call_idx(-1)
{
}

TestHttpServerAcceptAndHandleConn::~TestHttpServerAcceptAndHandleConn() = default;

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

void
http_server_use_mutex_for_incoming_connection_handling(os_mutex_t p_mutex);

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

err_t
netbuf_data(struct netbuf* buf, void** dataptr, u16_t* len)
{
    if ((nullptr == buf) || (nullptr == buf->ptr))
    {
        return ERR_BUF;
    }
    *dataptr = buf->ptr->payload;
    *len     = buf->ptr->len;
    return ERR_OK;
}

s8_t
netbuf_next(struct netbuf* buf)
{
    if ((nullptr == buf) || (nullptr == buf->ptr) || (nullptr == buf->ptr->next))
    {
        return -1;
    }
    buf->ptr = buf->ptr->next;
    if (nullptr == buf->ptr->next)
    {
        return 1;
    }
    return 0;
}

void
netbuf_delete(struct netbuf* buf)
{
    if (nullptr != buf)
    {
        struct pbuf* p = buf->p;
        while (nullptr != p)
        {
            struct pbuf* next_p = p->next;
            free(p);
            p = next_p;
        }
        free(buf);
    }
}

err_t
netconn_accept(struct netconn* conn, struct netconn** new_conn)
{
    if (nullptr == g_pTestClass)
    {
        return ERR_ARG;
    }
    *new_conn = g_pTestClass->m_p_new_conn;
    return g_pTestClass->m_netconn_accept_err;
}

err_t
netconn_recv(struct netconn* conn, struct netbuf** new_buf)
{
    if (nullptr == g_pTestClass)
    {
        return ERR_CLSD;
    }
    if (g_pTestClass->m_recv_frame_idx >= g_pTestClass->m_recv_frames.size())
    {
        *new_buf = nullptr;
        return ERR_CLSD;
    }
    const size_t idx   = g_pTestClass->m_recv_frame_idx;
    RecvFrame&   frame = g_pTestClass->m_recv_frames[idx];
    g_pTestClass->m_recv_frame_idx++;

    if (ERR_OK != frame.err)
    {
        *new_buf = nullptr;
        return frame.err;
    }

    struct netbuf* p_netbuf = static_cast<struct netbuf*>(calloc(1, sizeof(struct netbuf)));
    assert(nullptr != p_netbuf);

    // Build a pbuf chain from the frame's fragments
    struct pbuf* first_pbuf = nullptr;
    struct pbuf* last_pbuf  = nullptr;
    u16_t        total_len  = 0;
    for (const auto& frag : frame.fragments)
    {
        total_len += static_cast<u16_t>(frag.size());
    }

    u16_t remaining = total_len;
    for (const auto& frag : frame.fragments)
    {
        struct pbuf* p = static_cast<struct pbuf*>(calloc(1, sizeof(struct pbuf)));
        if (nullptr == p)
        {
            // Free any already-allocated pbufs and the netbuf
            struct pbuf* cur = first_pbuf;
            while (nullptr != cur)
            {
                struct pbuf* next_p = cur->next;
                free(cur);
                cur = next_p;
            }
            free(p_netbuf);
            *new_buf = nullptr;
            return ERR_MEM;
        }
        p->payload = const_cast<char*>(frag.c_str());
        p->len     = static_cast<u16_t>(frag.size());
        p->tot_len = remaining;
        p->next    = nullptr;
        remaining -= p->len;

        if (nullptr == first_pbuf)
        {
            first_pbuf = p;
        }
        if (nullptr != last_pbuf)
        {
            last_pbuf->next = p;
        }
        last_pbuf = p;
    }

    p_netbuf->p   = first_pbuf;
    p_netbuf->ptr = first_pbuf;
    *new_buf      = p_netbuf;
    return ERR_OK;
}

err_t
netconn_delete(struct netconn* conn)
{
    if (nullptr != g_pTestClass)
    {
        g_pTestClass->m_netconn_delete_call_count++;
        return g_pTestClass->m_netconn_delete_err;
    }
    return ERR_OK;
}

err_t
netconn_close(struct netconn* conn)
{
    if (nullptr != g_pTestClass)
    {
        return g_pTestClass->m_netconn_close_err;
    }
    return ERR_OK;
}

void
http_server_task_wdt_reset(void)
{
}

const char*
wrap_esp_err_to_name_r(const esp_err_t code, char* const p_buf, const size_t buf_len)
{
    (void)snprintf(p_buf, buf_len, "Unknows");
    return p_buf;
}

void
http_server_netconn_serve_handle_req(
    struct netconn* const        p_conn,
    char* const                  p_req_buf,
    const sta_ip_string_t* const p_local_ip_str,
    const sta_ip_string_t* const p_remote_ip_str)
{
    if (nullptr != g_pTestClass)
    {
        g_pTestClass->m_serve_capture.called    = true;
        g_pTestClass->m_serve_capture.req_buf   = (nullptr != p_req_buf) ? string(p_req_buf) : "";
        g_pTestClass->m_serve_capture.local_ip  = string(p_local_ip_str->buf);
        g_pTestClass->m_serve_capture.remote_ip = string(p_remote_ip_str->buf);
    }
}

const char*
conv_lwip_err_to_str(const err_enum_t err)
{
    return "Unknows";
}

char*
ip4addr_ntoa_r(const ip4_addr_t* addr, char* buf, int buflen)
{
    u32_t s_addr;
    char  inv[3];
    char* rp;
    u8_t* ap;
    u8_t  rem;
    u8_t  n;
    u8_t  i;
    int   len = 0;

    s_addr = ip4_addr_get_u32(addr);

    rp = buf;
    ap = (u8_t*)&s_addr;
    for (n = 0; n < 4; n++)
    {
        i = 0;
        do
        {
            rem = *ap % (u8_t)10;
            *ap /= (u8_t)10;
            inv[i++] = (char)('0' + rem);
        } while (*ap);
        while (i--)
        {
            if (len++ >= buflen)
            {
                return NULL;
            }
            *rp++ = inv[i];
        }
        if (len++ >= buflen)
        {
            return NULL;
        }
        *rp++ = '.';
        ap++;
    }
    *--rp = 0;
    return buf;
}

char*
ipaddr_ntoa_r(const ip_addr_t* addr, char* buf, int buflen)
{
    if (!addr)
    {
        return nullptr;
    }
    assert(!IP_IS_V6(addr));
    return ip4addr_ntoa_r(ip_2_ip4(addr), buf, buflen);
}

#ifdef __cplusplus
}
#endif

#define TEST_CHECK_LOG_RECORD(level_, msg_) ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("http_server", level_, msg_)

/*** Unit-Tests *******************************************************************************************************/

// ===== http_server_accept_and_handle_conn: accept path tests =====

TEST_F(TestHttpServerAcceptAndHandleConn, test_accept_no_mutex__err_timeout) // NOLINT
{
    // No mutex set (default), accept returns ERR_TIMEOUT
    this->m_netconn_accept_err = ERR_TIMEOUT;

    http_server_accept_and_handle_conn(this->m_p_conn);

    // ERR_TIMEOUT → vTaskDelay, no error log
    ASSERT_TRUE(this->m_vTaskDelay_called);
    ASSERT_FALSE(this->m_serve_capture.called);
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_accept_no_mutex__err_abrt) // NOLINT
{
    this->m_netconn_accept_err = ERR_ABRT;

    http_server_accept_and_handle_conn(this->m_p_conn);

    ASSERT_FALSE(this->m_vTaskDelay_called);
    ASSERT_FALSE(this->m_serve_capture.called);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "netconn_accept ERR_ABRT");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_accept_no_mutex__err_other) // NOLINT
{
    this->m_netconn_accept_err = ERR_MEM;

    http_server_accept_and_handle_conn(this->m_p_conn);

    ASSERT_FALSE(this->m_vTaskDelay_called);
    ASSERT_FALSE(this->m_serve_capture.called);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("netconn_accept: ") + to_string((int)ERR_MEM));
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_accept_with_mutex__lock_fails) // NOLINT
{
    // Set a non-NULL mutex
    os_mutex_static_t mutex_static = {};
    os_mutex_t        mutex        = os_mutex_create_static(&mutex_static);
    http_server_use_mutex_for_incoming_connection_handling(mutex);
    esp_log_wrapper_clear(); // clear "Activate using mutex" INFO log

    this->m_mutex_try_lock_result = false;

    http_server_accept_and_handle_conn(this->m_p_conn);

    ASSERT_TRUE(this->m_vTaskDelay_called);
    ASSERT_FALSE(this->m_serve_capture.called);

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_accept_ok_but_new_conn_null) // NOLINT
{
    this->m_netconn_accept_err = ERR_OK;
    this->m_p_new_conn         = nullptr;

    http_server_accept_and_handle_conn(this->m_p_conn);

    ASSERT_FALSE(this->m_serve_capture.called);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "netconn_accept returned OK, but p_new_conn is NULL");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_accept_ok_but_listen_conn_pcb_tcp_null) // NOLINT
{
    // Accept succeeds, new_conn is valid, but the listening conn's pcb.tcp is NULL
    this->m_netconn_accept_err = ERR_OK;
    this->m_p_new_conn         = static_cast<struct netconn*>(calloc(1, sizeof(struct netconn)));
    assert(nullptr != this->m_p_new_conn);
    // Ensure the listening connection's pcb.tcp is NULL
    this->m_p_conn->pcb.tcp = nullptr;

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_FALSE(this->m_serve_capture.called);
    ASSERT_EQ(1, this->m_netconn_delete_call_count);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "netconn_accept returned OK, but p_conn->pcb.tcp is NULL");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_accept_with_mutex__err_timeout__mutex_unlocked) // NOLINT
{
    // Verify mutex is unlocked after accept ERR_TIMEOUT
    os_mutex_static_t mutex_static = {};
    os_mutex_t        mutex        = os_mutex_create_static(&mutex_static);
    http_server_use_mutex_for_incoming_connection_handling(mutex);
    esp_log_wrapper_clear();

    this->m_netconn_accept_err = ERR_TIMEOUT;

    http_server_accept_and_handle_conn(this->m_p_conn);

    ASSERT_TRUE(this->m_vTaskDelay_called);
    ASSERT_FALSE(this->m_serve_capture.called);
    // No assertion for mutex unlock directly, but the code path is covered
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== http_server_netconn_serve: connection setup tests =====

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_new_conn_pcb_tcp_null) // NOLINT
{
    // Accept succeeds, listening conn has valid pcb.tcp, but new_conn has pcb.tcp == NULL
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;

    this->m_netconn_accept_err = ERR_OK;
    this->m_p_new_conn         = static_cast<struct netconn*>(calloc(1, sizeof(struct netconn)));
    assert(nullptr != this->m_p_new_conn);
    this->m_p_new_conn->pcb.tcp = nullptr;

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_FALSE(this->m_serve_capture.called);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "p_conn->pcb.tcp is NULL due to race condition(1)");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== http_server_netconn_serve: recv and handling tests =====

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_recv_error) // NOLINT
{
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    // First recv returns an error
    add_recv_error(ERR_CONN);

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_FALSE(this->m_serve_capture.called);
    // First: ERROR from netconn_recv failure
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("netconn recv: ") + to_string((int)ERR_CONN) + " (time: 0 ticks)");
    // Then: WARN about the connection being closed
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Connection from 192.168.1.100 to 192.168.1.1: The connection was closed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_simple_get_no_body) // NOLINT
{
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    const string request = "GET / HTTP/1.1\r\nHost: 192.168.1.1\r\n\r\n";
    add_recv_frame(request);

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_TRUE(this->m_serve_capture.called);
    ASSERT_EQ(request, this->m_serve_capture.req_buf);
    ASSERT_EQ("192.168.1.1", this->m_serve_capture.local_ip);
    ASSERT_EQ("192.168.1.100", this->m_serve_capture.remote_ip);

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_post_with_body_single_frame) // NOLINT
{
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    const string body    = "hello";
    const string request = "POST /data HTTP/1.1\r\nContent-Length: " + to_string(body.size()) + "\r\n\r\n" + body;
    add_recv_frame(request);

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_TRUE(this->m_serve_capture.called);
    ASSERT_EQ(request, this->m_serve_capture.req_buf);

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_post_with_body_multi_frame) // NOLINT
{
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    const string header = "POST /data HTTP/1.1\r\nContent-Length: 5\r\n\r\n";
    const string body   = "hello";
    add_recv_frame(header);
    add_recv_frame(body);

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_TRUE(this->m_serve_capture.called);
    ASSERT_EQ(header + body, this->m_serve_capture.req_buf);

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_header_split_across_frames) // NOLINT
{
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    // Split the request header across two frames
    const string part1 = "GET / HTTP/1.1\r\n";
    const string part2 = "Host: 192.168.1.1\r\n\r\n";
    add_recv_frame(part1);
    add_recv_frame(part2);

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_TRUE(this->m_serve_capture.called);
    ASSERT_EQ(part1 + part2, this->m_serve_capture.req_buf);

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_post_body_in_three_frames) // NOLINT
{
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    const string header = "POST /data HTTP/1.1\r\nContent-Length: 10\r\n\r\n";
    const string body1  = "hello";
    const string body2  = "world";
    add_recv_frame(header);
    add_recv_frame(body1);
    add_recv_frame(body2);

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_TRUE(this->m_serve_capture.called);
    ASSERT_EQ(header + body1 + body2, this->m_serve_capture.req_buf);

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_first_frame_exceeds_max_request_size) // NOLINT
{
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    // Create a frame larger than HTTP_SERVER_MAX_REQUEST_SIZE (4096)
    const string large_request(4097, 'A');
    add_recv_frame(large_request);

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_FALSE(this->m_serve_capture.called);
    // Expect error log about exceeding max request size, then WARN about connection closed
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "Received request size 4097 exceeds maximum allowed 4096");
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Connection from 192.168.1.100 to 192.168.1.1: The connection was closed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_accum_frames_exceed_max_request_size) // NOLINT
{
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    // Send frames that individually fit but together exceed the max (4096)
    // First frame: 3000 bytes (no complete header → will continue)
    const string frame1(3000, 'A');
    // Second frame: 2000 bytes → total 5000 > 4096
    const string frame2(2000, 'B');
    add_recv_frame(frame1);
    add_recv_frame(frame2);

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_FALSE(this->m_serve_capture.called);

    TEST_CHECK_LOG_RECORD(
        ESP_LOG_ERROR,
        "Can't fit new data to request buffer, max request size exceeded, "
        "accum_len: 3000, buf_len: 2000, req_buf_size: 3000, max_request_size: 4096");
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Connection from 192.168.1.100 to 192.168.1.1: The connection was closed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_content_length_exceeds_max_content_size) // NOLINT
{
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    // Content-Length exceeds HTTP_SERVER_MAX_CONTENT_SIZE (8192)
    const string request = "POST /data HTTP/1.1\r\nContent-Length: 9000\r\n\r\n";
    add_recv_frame(request);

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_FALSE(this->m_serve_capture.called);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "Content-Length 9000 exceeds maximum allowed 8192");
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Connection from 192.168.1.100 to 192.168.1.1: The connection was closed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_excess_data_trimmed) // NOLINT
{
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    // Content-Length says 3, but body has 10 bytes → excess data should be trimmed.
    // The code trims accum_len to expected_len but the serve handler still receives the buffer.
    // Verify the request is accepted (not rejected) and serve handler is called.
    const string request = "POST /data HTTP/1.1\r\nContent-Length: 3\r\n\r\nhelloworld";
    add_recv_frame(request);

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_TRUE(this->m_serve_capture.called);
    // The buffer starts with the expected header
    const string expected_header = "POST /data HTTP/1.1\r\nContent-Length: 3\r\n\r\n";
    ASSERT_EQ(0u, this->m_serve_capture.req_buf.find(expected_header));
    // After the header, the body starts with "hel" (first 3 chars)
    ASSERT_EQ(0u, this->m_serve_capture.req_buf.substr(expected_header.size()).find("hel"));

    TEST_CHECK_LOG_RECORD(
        ESP_LOG_WARN,
        "Received data (52 bytes) exceeds declared Content-Length, "
        "expected 45 (header_len: 42, content_len: 3) - discard excess data");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_request_timeout) // NOLINT
{
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    // Send a partial header (no \r\n\r\n) so the code waits for more data
    const string partial_header = "GET / HTTP/1.1\r\nHost: 192.168.1.1\r\n";
    add_recv_frame(partial_header);

    // Tick values:
    // 1: t0 in http_server_netconn_serve = 0
    // 2: timeout check in loop (1st iteration) = 0 → passes (0-0=0, not > 5000)
    // 3: t0 in http_server_recv_and_handle = 0
    // 4: time_for_netconn_recv in recv_and_handle = 0
    // 5: timeout check in loop (2nd iteration) = 6000 → timeout! (6000-0=6000 > 5000)
    this->m_tick_values = { 0, 0, 0, 0, 6000 };

    // Second recv returns ERR_CLSD (no more frames) - but this shouldn't be reached
    // because the timeout fires first. However, if the loop tries to recv before checking
    // timeout... let's check the order: timeout check is BEFORE recv.
    // So after the first recv succeeds, the loop restarts and checks timeout first → breaks.

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_FALSE(this->m_serve_capture.called);
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_ERROR,
        "Connection from 192.168.1.100 to 192.168.1.1: Timeout waiting for HTTP request");
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Connection from 192.168.1.100 to 192.168.1.1: The connection was closed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_content_timeout) // NOLINT
{
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    // Send complete header with Content-Length, but no body
    const string header = "POST /data HTTP/1.1\r\nContent-Length: 100\r\n\r\n";
    add_recv_frame(header);

    // Tick values for content timeout (30000 ms = 30000 ticks):
    // 1: t0 in serve = 0
    // 2: timeout check (1st iter) = 0 → passes
    // 3: t0 in recv_and_handle = 0
    // 4: time_for_netconn_recv = 0
    // After receiving header, content_len is set, is_header_completed=true, not ready yet.
    // Loop continues:
    // 5: timeout check (2nd iter) = 31000 → timeout! (31000-0 > 30000)
    this->m_tick_values = { 0, 0, 0, 0, 31000 };

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_FALSE(this->m_serve_capture.called);
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_ERROR,
        "Connection from 192.168.1.100 to 192.168.1.1: Timeout waiting for HTTP content");
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Connection from 192.168.1.100 to 192.168.1.1: The connection was closed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_recv_error_after_partial_data) // NOLINT
{
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    // First recv: partial header
    const string partial = "GET / HTTP/1.1\r\n";
    add_recv_frame(partial);
    // Second recv: error
    add_recv_error(ERR_CONN);

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_FALSE(this->m_serve_capture.called);
    // First: ERROR from netconn_recv failure
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("netconn recv: ") + to_string((int)ERR_CONN) + " (time: 0 ticks)");
    // Then: WARN about the connection being closed
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Connection from 192.168.1.100 to 192.168.1.1: The connection was closed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== netconn_close/delete error handling =====

TEST_F(TestHttpServerAcceptAndHandleConn, test_netconn_close_error) // NOLINT
{
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    const string request = "GET / HTTP/1.1\r\nHost: 192.168.1.1\r\n\r\n";
    add_recv_frame(request);

    this->m_netconn_close_err = ERR_CONN;

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_TRUE(this->m_serve_capture.called);
    // Check for the close error log
    const string expected_close_err_msg = string("netconn_close failed (Unknows), err=") + to_string((int)ERR_CONN)
                                          + " (Unknows)";
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, expected_close_err_msg);
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_netconn_delete_error) // NOLINT
{
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    const string request = "GET / HTTP/1.1\r\nHost: 192.168.1.1\r\n\r\n";
    add_recv_frame(request);

    this->m_netconn_delete_err = ERR_CONN;

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_TRUE(this->m_serve_capture.called);
    // Check for the delete error log
    const string expected_delete_err_msg = string("netconn_delete failed, err=") + to_string((int)ERR_CONN)
                                           + " (Unknows)";
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, expected_delete_err_msg);
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== Buffer handling edge cases =====

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_post_realloc_to_fit_content) // NOLINT
{
    // Test the path where req_buf_size != expected_len, triggering realloc
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    // First frame: header + partial body (Content-Length says 100 bytes)
    const string header = "POST /data HTTP/1.1\r\nContent-Length: 100\r\n\r\n";
    const string body1(50, 'A');
    add_recv_frame(header + body1);

    // Second frame: remaining body
    const string body2(50, 'B');
    add_recv_frame(body2);

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_TRUE(this->m_serve_capture.called);
    ASSERT_EQ(header + body1 + body2, this->m_serve_capture.req_buf);

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_get_with_various_headers) // NOLINT
{
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    const string request
        = "GET /status HTTP/1.1\r\n"
          "Host: 192.168.1.1\r\n"
          "Accept: application/json\r\n"
          "Connection: keep-alive\r\n"
          "\r\n";
    add_recv_frame(request);

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_TRUE(this->m_serve_capture.called);
    ASSERT_EQ(request, this->m_serve_capture.req_buf);

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_post_max_content_size) // NOLINT
{
    // Content-Length exactly at the max (8192) should be accepted
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    const size_t content_size = 8192;
    const string header       = "POST /data HTTP/1.1\r\nContent-Length: " + to_string(content_size) + "\r\n\r\n";
    const string body(content_size, 'X');

    // Send header and body in separate frames (each < 4096 max request size)
    add_recv_frame(header);
    // Send body in chunks of 4000 to stay under request size limit for individual frames
    for (size_t offset = 0; offset < content_size; offset += 4000)
    {
        const size_t chunk = min((size_t)4000, content_size - offset);
        add_recv_frame(body.substr(offset, chunk));
    }

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_TRUE(this->m_serve_capture.called);
    ASSERT_EQ(header + body, this->m_serve_capture.req_buf);

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_connection_closed_during_body_recv) // NOLINT
{
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    // Header says Content-Length: 100 but connection closes after partial body
    const string header = "POST /data HTTP/1.1\r\nContent-Length: 100\r\n\r\n";
    const string body1(30, 'A');
    add_recv_frame(header + body1);
    // Connection closed (ERR_CLSD) - exhausted frames cause ERR_CLSD from stub
    // (No more frames added, so next recv returns ERR_CLSD)

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_FALSE(this->m_serve_capture.called);
    // First: ERROR from netconn_recv failure (ERR_CLSD)
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, string("netconn recv: ") + to_string((int)ERR_CLSD) + " (time: 0 ticks)");
    // Then: WARN about the connection being closed
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Connection from 192.168.1.100 to 192.168.1.1: The connection was closed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== Mutex + successful serve =====

TEST_F(TestHttpServerAcceptAndHandleConn, test_accept_with_mutex__successful_serve) // NOLINT
{
    os_mutex_static_t mutex_static = {};
    os_mutex_t        mutex        = os_mutex_create_static(&mutex_static);
    http_server_use_mutex_for_incoming_connection_handling(mutex);
    esp_log_wrapper_clear();

    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    const string request = "GET / HTTP/1.1\r\nHost: 192.168.1.1\r\n\r\n";
    add_recv_frame(request);

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_TRUE(this->m_serve_capture.called);
    ASSERT_EQ(request, this->m_serve_capture.req_buf);
    ASSERT_FALSE(this->m_vTaskDelay_called);

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== Buffer overflow check: accum_len + buflen > req_buf_size =====

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_buffer_overflow_check) // NOLINT
{
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    // This tests the path where header is found, Content-Length triggers realloc to smaller size,
    // then the remaining data in the current frame exceeds the new buffer.
    // Scenario: Send a frame where excess data > Content-Length, causing accum_len to be trimmed,
    // then the request should complete normally since excess is trimmed.
    const string header  = "POST /d HTTP/1.1\r\nContent-Length: 2\r\n\r\n";
    const string request = header + "AB";
    add_recv_frame(request);

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_TRUE(this->m_serve_capture.called);
    ASSERT_EQ(request, this->m_serve_capture.req_buf);

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== Allocation failure tests =====

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_malloc_failure_first_frame) // NOLINT
{
    // Lines 79-83: os_malloc failure for the first frame allocation
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    const string request = "GET / HTTP/1.1\r\nHost: 192.168.1.1\r\n\r\n";
    add_recv_frame(request);

    // Fail the first os_malloc_internal call (the allocation for the first frame buffer)
    // Allocations: #1=netbuf, #2=pbuf, #3=os_malloc for frame buffer
    this->m_alloc_fail_on_call_idx = 3;

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_FALSE(this->m_serve_capture.called);
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_ERROR,
        "Failed to allocate " + to_string(request.size()) + " bytes for request buffer");
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Connection from 192.168.1.100 to 192.168.1.1: The connection was closed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_realloc_failure_non_first_frame) // NOLINT
{
    // Lines 106-115: os_realloc_safe_and_clean failure when receiving a non-first frame
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    // Split header across two frames; second triggers realloc because req_buf_size == accum_len
    const string part1 = "GET / HTTP/1.1\r\n";
    const string part2 = "Host: 192.168.1.1\r\n\r\n";
    add_recv_frame(part1);
    add_recv_frame(part2);

    // Calloc calls: #1 netbuf for frame1, #2 pbuf for frame1, #3 os_calloc_internal for os_malloc(frame1),
    // #4 netbuf for frame2, #5 pbuf for frame2, #6 os_calloc_internal inside os_realloc_safe for frame2.
    // Fail #6 to simulate realloc failure.
    this->m_alloc_fail_on_call_idx = 6;

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_FALSE(this->m_serve_capture.called);
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_ERROR,
        "Failed to reallocate request buffer to " + to_string(part1.size() + part2.size())
            + " bytes (accum_len: " + to_string(part1.size()) + ", buf_len: " + to_string(part2.size()) + ")");
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Connection from 192.168.1.100 to 192.168.1.1: The connection was closed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_realloc_failure_content_length_handler) // NOLINT
{
    // Lines 153-162: os_realloc_safe_and_clean failure in content-length handler
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    // Send header with Content-Length that requires realloc (expected_len > req_buf_size)
    const string header = "POST /data HTTP/1.1\r\nContent-Length: 100\r\n\r\n";
    add_recv_frame(header);

    // Calloc calls: #1 netbuf for frame, #2 pbuf for frame, #3 os_calloc_internal for os_malloc(frame buffer),
    // #4 os_calloc_internal inside os_realloc_safe for content-length realloc.
    // Fail #4 to simulate realloc failure.
    this->m_alloc_fail_on_call_idx = 4;

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_FALSE(this->m_serve_capture.called);
    const size_t header_len   = header.size(); // everything up to body start
    const size_t content_len  = 100;
    const size_t expected_len = header_len + content_len;
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_ERROR,
        "Failed to reallocate request buffer to " + to_string(expected_len)
            + " bytes (header_len: " + to_string(header_len) + ", content_len: " + to_string(content_len) + ")");
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Connection from 192.168.1.100 to 192.168.1.1: The connection was closed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== Buffer overflow: accum_len + buflen > req_buf_size (lines 185-193) =====

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_buffer_overflow_accum_plus_buflen_exceeds_req_buf_size) // NOLINT
{
    // Lines 185-193: After content-length realloc sets req_buf_size = expected_len,
    // a new frame arrives that exceeds remaining space.
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    // First frame: header + 2 bytes body. Content-Length says 5 bytes.
    // After content-length handler: realloc to expected_len = header_len + 5
    // accum_len = header_len + 2, req_buf_size = header_len + 5
    const string header = "POST /d HTTP/1.1\r\nContent-Length: 5\r\n\r\n";
    const string body1  = "AB";
    add_recv_frame(header + body1);

    // Second frame: 10 bytes → accum_len + 10 > req_buf_size
    const string body2 = "ABCDEFGHIJ";
    add_recv_frame(body2);

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_FALSE(this->m_serve_capture.called);

    const size_t header_len   = header.size();
    const size_t accum_len    = header_len + body1.size();
    const size_t req_buf_size = header_len + 5;
    TEST_CHECK_LOG_RECORD(
        ESP_LOG_ERROR,
        "Request buffer is full, can't fit new data, accum_len: " + to_string(accum_len)
            + ", buf_len: " + to_string(body2.size()) + ", req_buf_size: " + to_string(req_buf_size));
    TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "Connection from 192.168.1.100 to 192.168.1.1: The connection was closed");
    ASSERT_TRUE(esp_log_wrapper_is_empty());

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

// ===== Multi-pbuf netbuf tests (OOSeq TCP segment merging scenario) =====

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_post_body_multi_pbuf_single_recv) // NOLINT
{
    // Simulates TCP OOSeq merge: one netconn_recv returns a netbuf with 3 chained pbufs
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    const string body_part1(1440, 'A');
    const string body_part2(1440, 'B');
    const string body_part3(1440, 'C');
    const size_t body_len = body_part1.size() + body_part2.size() + body_part3.size();

    const string header = "POST /data HTTP/1.1\r\nContent-Length: " + to_string(body_len) + "\r\n\r\n";

    // First recv: header as single pbuf
    add_recv_frame(header);
    // Second recv: body arrives as 3 chained pbufs in one netbuf (simulating TCP OOSeq merge)
    add_recv_frame_multi_pbuf({ body_part1, body_part2, body_part3 });

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_TRUE(this->m_serve_capture.called);
    ASSERT_EQ(header + body_part1 + body_part2 + body_part3, this->m_serve_capture.req_buf);

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_post_header_and_body_in_multi_pbuf) // NOLINT
{
    // Header + body arrive together in one netbuf with 2 pbufs
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    const string body   = "hello world";
    const string header = "POST /data HTTP/1.1\r\nContent-Length: " + to_string(body.size()) + "\r\n\r\n";

    // Single recv with header and body as separate pbufs in one netbuf
    add_recv_frame_multi_pbuf({ header, body });

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_TRUE(this->m_serve_capture.called);
    ASSERT_EQ(header + body, this->m_serve_capture.req_buf);

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_post_multi_pbuf_then_single_pbuf_frames) // NOLINT
{
    // Mix of multi-pbuf and single-pbuf netbufs across multiple recv calls
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    const string body_part1(1440, 'X');
    const string body_part2(1440, 'Y');
    const string body_part3(966, 'Z');
    const size_t body_len = body_part1.size() + body_part2.size() + body_part3.size();

    const string header = "POST /data HTTP/1.1\r\nContent-Length: " + to_string(body_len) + "\r\n\r\n";

    // First recv: header
    add_recv_frame(header);
    // Second recv: two pbufs chained (OOSeq merge of 2 segments)
    add_recv_frame_multi_pbuf({ body_part1, body_part2 });
    // Third recv: single pbuf (normal in-order segment)
    add_recv_frame(body_part3);

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_TRUE(this->m_serve_capture.called);
    ASSERT_EQ(header + body_part1 + body_part2 + body_part3, this->m_serve_capture.req_buf);

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}

TEST_F(TestHttpServerAcceptAndHandleConn, test_serve_get_no_body_multi_pbuf) // NOLINT
{
    // GET request header arrives as 2 chained pbufs
    struct tcp_pcb listen_pcb = {};
    this->m_p_conn->pcb.tcp   = &listen_pcb;
    setup_accept_success_with_valid_pcb();

    const string part1 = "GET / HTTP/1.1\r\n";
    const string part2 = "Host: 192.168.1.1\r\n\r\n";

    // Single recv with header split across 2 pbufs in one netbuf
    add_recv_frame_multi_pbuf({ part1, part2 });

    http_server_accept_and_handle_conn(this->m_p_conn);

    free(this->m_p_new_conn);
    this->m_p_new_conn = nullptr;

    ASSERT_TRUE(this->m_serve_capture.called);
    ASSERT_EQ(part1 + part2, this->m_serve_capture.req_buf);

    os_malloc_trace_dump();
    ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("MEM_TRACE", ESP_LOG_INFO, "Num blocks allocated: 0");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
    ASSERT_EQ(0, this->m_alloc_free_call_count);
}
