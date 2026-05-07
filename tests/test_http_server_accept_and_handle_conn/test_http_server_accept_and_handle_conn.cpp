/**
 * @file test_http_server_accept_and_handle_conn.cpp
 * @author TheSomeMan
 * @date 2026-05-07
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gtest/gtest.h"
#include "http_server_accept_and_handle_conn.h"
#include <string>
#include <cassert>
#include "esp_err.h"
#include "os_task.h"
#include "wifi_manager_defs.h"
#include "http_server_auth.h"
#include "esp_log_wrapper.hpp"

using namespace std;

class TestHttpServerAcceptAndHandleConn;
static TestHttpServerAcceptAndHandleConn* g_pTestClass;

/*** Google-test class implementation *********************************************************************************/

class TestHttpServerAcceptAndHandleConn : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        esp_log_wrapper_init();
        g_pTestClass   = this;
        this->m_p_conn = static_cast<struct netconn*>(calloc(1, sizeof(struct netconn)));
        assert(nullptr != this->m_p_conn);
    }

    void
    TearDown() override
    {
        free(this->m_p_conn);
        esp_log_wrapper_deinit();
    }

public:
    struct netconn* m_p_conn;

    TestHttpServerAcceptAndHandleConn();

    ~TestHttpServerAcceptAndHandleConn() override;
};

static TestHttpServerAcceptAndHandleConn* g_pTestObj;

TestHttpServerAcceptAndHandleConn::TestHttpServerAcceptAndHandleConn()
    : Test()
    , m_p_conn(nullptr)
{
}

TestHttpServerAcceptAndHandleConn::~TestHttpServerAcceptAndHandleConn()
{
    g_pTestObj = this;
}

#ifdef __cplusplus
extern "C" {
#endif

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
    // TODO: implement: store an array of timestamps in TestHttpServerAcceptAndHandleConn for each call and return them
    // one by one. Return 0 if the array is not set. Fail with assert if the array is exhausted.
    return 0;
}

void
vTaskDelay(const TickType_t xTicksToDelay)
{
    // Do nothing.
}

err_t
netbuf_data(struct netbuf* buf, void** dataptr, u16_t* len)
{
    // TODO: implement.
    return ERR_ARG;
}

void
netbuf_delete(struct netbuf* buf)
{
    // TODO: implement.
}

err_t
netconn_accept(struct netconn* conn, struct netconn** new_conn)
{
    // TODO: implement.
    return ERR_ARG;
}

err_t
netconn_recv(struct netconn* conn, struct netbuf** new_buf)
{
    // TODO: implement: return the error code and data from the pre-configured array of in
    // TestHttpServerAcceptAndHandleConn. Return ERR_ARG if the array is not set or exhausted.
    return ERR_ARG;
}

err_t
netconn_delete(struct netconn* conn)
{
    // TODO: implement.
    return ERR_ARG;
}

err_t
netconn_close(struct netconn* conn)
{
    // TODO: implement.
    return ERR_ARG;
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
    // TODO: implement: store the parameters in TestHttpServerAcceptAndHandleConn for later verification.
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

TEST_F(TestHttpServerAcceptAndHandleConn, test_1) // NOLINT
{
    http_server_accept_and_handle_conn(this->m_p_conn);
    TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "");
    ASSERT_TRUE(esp_log_wrapper_is_empty());
}
