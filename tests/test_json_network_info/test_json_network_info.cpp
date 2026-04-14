/**
 * @file test_json_network_info.cpp
 * @author TheSomeMan
 * @date 2020-08-23
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gtest/gtest.h"
#include "json_network_info.h"
#include <string>
#include "os_mutex.h"

using namespace std;

class TestJsonNetworkInfo;
static TestJsonNetworkInfo* g_pTestClass;

class MemAllocTrace
{
    vector<void*> allocated_mem;

    std::vector<void*>::iterator
    find(void* ptr)
    {
        for (auto iter = this->allocated_mem.begin(); iter != this->allocated_mem.end(); ++iter)
        {
            if (*iter == ptr)
            {
                return iter;
            }
        }
        return this->allocated_mem.end();
    }

public:
    void
    add(void* ptr)
    {
        auto iter = find(ptr);
        assert(iter == this->allocated_mem.end()); // ptr was found in the list of allocated memory blocks
        this->allocated_mem.push_back(ptr);
    }
    void
    remove(void* ptr)
    {
        auto iter = find(ptr);
        assert(iter != this->allocated_mem.end()); // ptr was not found in the list of allocated memory blocks
        this->allocated_mem.erase(iter);
    }
    bool
    is_empty()
    {
        return this->allocated_mem.empty();
    }
};

/*** Google-test class implementation *********************************************************************************/

class TestJsonNetworkInfo : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        json_network_info_init();
        g_pTestClass = this;

        this->m_malloc_cnt                     = 0;
        this->m_malloc_fail_on_cnt             = 0;
        this->m_mutex_lock_with_timeout_result = true;
        this->m_mutex_unlock_call_cnt          = 0;
        this->m_callback_called                = false;
        this->m_callback_info_is_null          = false;
        this->m_callback_http_resp_code        = 0;
    }

    void
    TearDown() override
    {
        json_network_info_deinit();
        g_pTestClass = nullptr;
    }

public:
    TestJsonNetworkInfo();

    ~TestJsonNetworkInfo() override;

    MemAllocTrace m_mem_alloc_trace {};
    uint32_t      m_malloc_cnt {};
    uint32_t      m_malloc_fail_on_cnt {};
    bool          m_mutex_lock_with_timeout_result { true };
    uint32_t      m_mutex_unlock_call_cnt {};
    bool          m_callback_called {};
    bool          m_callback_info_is_null {};
    int           m_callback_http_resp_code {};
};

TestJsonNetworkInfo::TestJsonNetworkInfo() = default;

TestJsonNetworkInfo::~TestJsonNetworkInfo() = default;

extern "C" {

void*
os_malloc(const size_t size)
{
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    void* ptr = malloc(size);
    assert(nullptr != ptr);
    g_pTestClass->m_mem_alloc_trace.add(ptr);
    return ptr;
}

void
os_free_internal(void* ptr)
{
    g_pTestClass->m_mem_alloc_trace.remove(ptr);
    free(ptr);
}

void*
os_calloc(const size_t nmemb, const size_t size)
{
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    void* ptr = calloc(nmemb, size);
    assert(nullptr != ptr);
    g_pTestClass->m_mem_alloc_trace.add(ptr);
    return ptr;
}

os_mutex_t
os_mutex_create_static(os_mutex_static_t* const p_mutex_static)
{
    return reinterpret_cast<os_mutex_t>(p_mutex_static);
}

void
os_mutex_delete(os_mutex_t* p_mutex)
{
    if (nullptr != *p_mutex)
    {
        *p_mutex = nullptr;
    }
}

bool
os_mutex_lock_with_timeout(os_mutex_t const h_mutex, const os_delta_ticks_t ticks_to_wait)
{
    (void)h_mutex;
    (void)ticks_to_wait;
    if (nullptr == g_pTestClass)
    {
        return true;
    }
    return g_pTestClass->m_mutex_lock_with_timeout_result;
}

void
os_mutex_unlock(os_mutex_t const h_mutex)
{
    (void)h_mutex;
    if (nullptr != g_pTestClass)
    {
        ++g_pTestClass->m_mutex_unlock_call_cnt;
    }
}

} // extern "C"

static string
json_network_info_get(void)
{
    http_server_resp_status_json_t resp_status_json = {};
    json_network_info_generate(&resp_status_json);
    string json_info_copy(resp_status_json.buf);
    return json_info_copy;
}

extern "C" void
test_cb_do_action_with_timeout(json_network_info_t* const p_info, void* const p_param)
{
    (void)p_param;
    g_pTestClass->m_callback_called         = true;
    g_pTestClass->m_callback_info_is_null   = (nullptr == p_info);
    g_pTestClass->m_callback_http_resp_code = (nullptr == p_info) ? 503 : 200;
}

extern "C" void
test_cb_do_action_with_timeout_with_const_param(json_network_info_t* const p_info, const void* const p_param)
{
    (void)p_param;
    g_pTestClass->m_callback_called         = true;
    g_pTestClass->m_callback_info_is_null   = (nullptr == p_info);
    g_pTestClass->m_callback_http_resp_code = (nullptr == p_info) ? 503 : 200;
}

extern "C" void
test_cb_do_action_with_timeout_without_param(json_network_info_t* const p_info)
{
    g_pTestClass->m_callback_called         = true;
    g_pTestClass->m_callback_info_is_null   = (nullptr == p_info);
    g_pTestClass->m_callback_http_resp_code = (nullptr == p_info) ? 503 : 200;
}

extern "C" void
test_cb_do_const_action_with_timeout(const json_network_info_t* const p_info, void* const p_param)
{
    (void)p_param;
    g_pTestClass->m_callback_called         = true;
    g_pTestClass->m_callback_info_is_null   = (nullptr == p_info);
    g_pTestClass->m_callback_http_resp_code = (nullptr == p_info) ? 503 : 200;
}

extern "C" void
test_cb_do_const_action_with_timeout_with_const_param(
    const json_network_info_t* const p_info,
    const void* const                p_param)
{
    (void)p_param;
    g_pTestClass->m_callback_called         = true;
    g_pTestClass->m_callback_info_is_null   = (nullptr == p_info);
    g_pTestClass->m_callback_http_resp_code = (nullptr == p_info) ? 503 : 200;
}

/*** Unit-Tests *******************************************************************************************************/

TEST_F(TestJsonNetworkInfo, test_after_init) // NOLINT
{
    string json_str = json_network_info_get();
    ASSERT_EQ(string("{}\n"), json_str);
}

TEST_F(TestJsonNetworkInfo, test_clear) // NOLINT
{
    json_network_info_clear();
    string json_str = json_network_info_get();
    ASSERT_EQ(string("{}\n"), json_str);
}

TEST_F(TestJsonNetworkInfo, test_generate_ssid_null_lan_false) // NOLINT
{
    const network_info_str_t network_info = {
        { "192.168.0.50" },
        { "192.168.0.1" },
        { "255.255.255.0" },
        { "192.168.0.2" },
    };
    json_network_info_update(nullptr, &network_info, UPDATE_CONNECTION_OK);
    string json_str = json_network_info_get();
    ASSERT_EQ(
        string("{\"ssid\":null,\"ip\":\"192.168.0.50\",\"netmask\":\"255.255.255.0\",\"gw\":\"192.168.0.1\",\"dhcp\":"
               "\"192.168.0.2\",\"urc\":0}\n"),
        json_str);
}

TEST_F(TestJsonNetworkInfo, test_generate_ssid_null_lan_true) // NOLINT
{
    const network_info_str_t network_info = {
        { "192.168.0.50" },
        { "192.168.0.1" },
        { "255.255.255.0" },
        { "192.168.0.2" },
    };
    json_network_info_update(nullptr, &network_info, UPDATE_CONNECTION_OK);
    string json_str = json_network_info_get();
    ASSERT_EQ(
        string("{\"ssid\":null,\"ip\":\"192.168.0.50\",\"netmask\":\"255.255.255.0\",\"gw\":\"192.168.0.1\",\"dhcp\":"
               "\"192.168.0.2\",\"urc\":0}\n"),
        json_str);
}

TEST_F(TestJsonNetworkInfo, test_generate_ssid_empty) // NOLINT
{
    const network_info_str_t network_info = {
        { "192.168.0.50" },
        { "192.168.0.1" },
        { "255.255.255.0" },
        { "192.168.0.2" },
    };
    const wifiman_wifi_ssid_t ssid = { "" };
    json_network_info_update(&ssid, &network_info, UPDATE_CONNECTION_OK);
    string json_str = json_network_info_get();
    ASSERT_EQ(
        string("{\"ssid\":\"\",\"ip\":\"192.168.0.50\",\"netmask\":\"255.255.255.0\",\"gw\":\"192.168.0.1\",\"dhcp\":"
               "\"192.168.0.2\",\"urc\":0}\n"),
        json_str);
}

TEST_F(TestJsonNetworkInfo, test_generate_connection_ok) // NOLINT
{
    const network_info_str_t network_info = {
        { "192.168.0.50" },
        { "192.168.0.1" },
        { "255.255.255.0" },
        { "192.168.0.2" },
    };

    const wifiman_wifi_ssid_t ssid = { "test_ssid" };
    json_network_info_update(&ssid, &network_info, UPDATE_CONNECTION_OK);
    string json_str = json_network_info_get();
    ASSERT_EQ(
        string("{"
               "\"ssid\":\"test_ssid\","
               "\"ip\":\"192.168.0.50\","
               "\"netmask\":\"255.255.255.0\","
               "\"gw\":\"192.168.0.1\","
               "\"dhcp\":\"192.168.0.2\","
               "\"urc\":0"
               "}\n"),
        json_str);
}

TEST_F(TestJsonNetworkInfo, test_generate_failed_attempt) // NOLINT
{
    const network_info_str_t network_info = {
        { "0" },
        { "0" },
        { "0" },
        { "" },
    };
    const wifiman_wifi_ssid_t ssid = { "test_ssid" };
    json_network_info_update(&ssid, &network_info, UPDATE_FAILED_ATTEMPT);
    string json_str = json_network_info_get();
    ASSERT_EQ(
        string("{"
               "\"ssid\":\"test_ssid\","
               "\"ip\":\"0\","
               "\"netmask\":\"0\","
               "\"gw\":\"0\","
               "\"dhcp\":\"\","
               "\"urc\":1"
               "}\n"),
        json_str);
}

TEST_F(TestJsonNetworkInfo, test_generate_failed_attempt_2) // NOLINT
{
    const network_info_str_t network_info = {
        { "192.168.0.50" },
        { "192.168.0.1" },
        { "255.255.255.0" },
        { "192.168.0.2" },
    };
    const wifiman_wifi_ssid_t ssid = { "test_ssid" };
    json_network_info_update(&ssid, &network_info, UPDATE_FAILED_ATTEMPT);
    string json_str = json_network_info_get();
    ASSERT_EQ(
        string("{"
               "\"ssid\":\"test_ssid\","
               "\"ip\":\"192.168.0.50\","
               "\"netmask\":\"255.255.255.0\","
               "\"gw\":\"192.168.0.1\","
               "\"dhcp\":\"192.168.0.2\","
               "\"urc\":1"
               "}\n"),
        json_str);
}

TEST_F(TestJsonNetworkInfo, test_generate_user_disconnect) // NOLINT
{
    const network_info_str_t network_info = {
        { "0" },
        { "0" },
        { "0" },
        { "" },
    };
    const wifiman_wifi_ssid_t ssid = { "test_ssid" };
    json_network_info_update(&ssid, &network_info, UPDATE_USER_DISCONNECT);
    string json_str = json_network_info_get();
    ASSERT_EQ(
        string("{"
               "\"ssid\":\"test_ssid\","
               "\"ip\":\"0\","
               "\"netmask\":\"0\","
               "\"gw\":\"0\","
               "\"dhcp\":\"\","
               "\"urc\":2"
               "}\n"),
        json_str);
}

TEST_F(TestJsonNetworkInfo, test_generate_lost_connection) // NOLINT
{
    const network_info_str_t network_info = {
        { "0" },
        { "0" },
        { "0" },
        { "" },
    };
    const wifiman_wifi_ssid_t ssid = { "test_ssid" };
    json_network_info_update(&ssid, &network_info, UPDATE_LOST_CONNECTION);
    string json_str = json_network_info_get();
    ASSERT_EQ(
        string("{"
               "\"ssid\":\"test_ssid\","
               "\"ip\":\"0\","
               "\"netmask\":\"0\","
               "\"gw\":\"0\","
               "\"dhcp\":\"\","
               "\"urc\":3"
               "}\n"),
        json_str);
}

TEST_F(TestJsonNetworkInfo, test_do_action_with_timeout_lock_failure_invokes_callback_with_null) // NOLINT
{
    this->m_mutex_lock_with_timeout_result = false;
    json_network_info_do_action_with_timeout(&test_cb_do_action_with_timeout, nullptr, 1U);
    ASSERT_TRUE(this->m_callback_called);
    ASSERT_TRUE(this->m_callback_info_is_null);
    ASSERT_EQ(503, this->m_callback_http_resp_code);
    ASSERT_EQ(0, this->m_mutex_unlock_call_cnt);
}

TEST_F(
    TestJsonNetworkInfo,
    test_do_action_with_timeout_with_const_param_lock_failure_invokes_callback_with_null) // NOLINT
{
    this->m_mutex_lock_with_timeout_result = false;
    json_network_info_do_action_with_timeout_with_const_param(
        &test_cb_do_action_with_timeout_with_const_param,
        nullptr,
        1U);
    ASSERT_TRUE(this->m_callback_called);
    ASSERT_TRUE(this->m_callback_info_is_null);
    ASSERT_EQ(503, this->m_callback_http_resp_code);
    ASSERT_EQ(0, this->m_mutex_unlock_call_cnt);
}

TEST_F(TestJsonNetworkInfo, test_do_action_with_timeout_without_param_lock_failure_invokes_callback_with_null) // NOLINT
{
    this->m_mutex_lock_with_timeout_result = false;
    json_network_info_do_action_with_timeout_without_param(&test_cb_do_action_with_timeout_without_param, 1U);
    ASSERT_TRUE(this->m_callback_called);
    ASSERT_TRUE(this->m_callback_info_is_null);
    ASSERT_EQ(503, this->m_callback_http_resp_code);
    ASSERT_EQ(0, this->m_mutex_unlock_call_cnt);
}

TEST_F(TestJsonNetworkInfo, test_do_const_action_with_timeout_lock_failure_invokes_callback_with_null) // NOLINT
{
    this->m_mutex_lock_with_timeout_result = false;
    json_network_info_do_const_action_with_timeout(&test_cb_do_const_action_with_timeout, nullptr, 1U);
    ASSERT_TRUE(this->m_callback_called);
    ASSERT_TRUE(this->m_callback_info_is_null);
    ASSERT_EQ(503, this->m_callback_http_resp_code);
    ASSERT_EQ(0, this->m_mutex_unlock_call_cnt);
}

TEST_F(
    TestJsonNetworkInfo,
    test_do_const_action_with_timeout_with_const_param_lock_failure_invokes_callback_with_null) // NOLINT
{
    this->m_mutex_lock_with_timeout_result = false;
    json_network_info_do_const_action_with_timeout_with_const_param(
        &test_cb_do_const_action_with_timeout_with_const_param,
        nullptr,
        1U);
    ASSERT_TRUE(this->m_callback_called);
    ASSERT_TRUE(this->m_callback_info_is_null);
    ASSERT_EQ(503, this->m_callback_http_resp_code);
    ASSERT_EQ(0, this->m_mutex_unlock_call_cnt);
}

TEST_F(TestJsonNetworkInfo, test_do_action_wrapper_invokes_callback_with_non_null_info) // NOLINT
{
    json_network_info_do_action(&test_cb_do_action_with_timeout, nullptr);
    ASSERT_TRUE(this->m_callback_called);
    ASSERT_FALSE(this->m_callback_info_is_null);
    ASSERT_EQ(200, this->m_callback_http_resp_code);
    ASSERT_EQ(1, this->m_mutex_unlock_call_cnt);
}

TEST_F(TestJsonNetworkInfo, test_do_const_action_wrapper_invokes_callback_with_non_null_info) // NOLINT
{
    json_network_info_do_const_action(&test_cb_do_const_action_with_timeout, nullptr);
    ASSERT_TRUE(this->m_callback_called);
    ASSERT_FALSE(this->m_callback_info_is_null);
    ASSERT_EQ(200, this->m_callback_http_resp_code);
    ASSERT_EQ(1, this->m_mutex_unlock_call_cnt);
}

TEST_F(TestJsonNetworkInfo, test_set_reason_user_disconnect_updates_json_reason) // NOLINT
{
    json_network_info_set_reason_user_disconnect();
    const string json_str = json_network_info_get();
    ASSERT_EQ(
        string("{"
               "\"ssid\":null,"
               "\"ip\":\"\","
               "\"netmask\":\"\","
               "\"gw\":\"\","
               "\"dhcp\":\"\","
               "\"urc\":2"
               "}\n"),
        json_str);
}

TEST_F(TestJsonNetworkInfo, test_update_with_null_network_info_clears_network_fields) // NOLINT
{
    const wifiman_wifi_ssid_t ssid = { "test_ssid" };
    json_network_info_update(&ssid, nullptr, UPDATE_FAILED_ATTEMPT);
    const string json_str = json_network_info_get();
    ASSERT_EQ(
        string("{"
               "\"ssid\":\"test_ssid\","
               "\"ip\":\"\","
               "\"netmask\":\"\","
               "\"gw\":\"\","
               "\"dhcp\":\"\","
               "\"urc\":1"
               "}\n"),
        json_str);
}

TEST_F(TestJsonNetworkInfo, test_set_extra_info_is_appended_when_reason_is_defined) // NOLINT
{
    const network_info_str_t network_info = {
        { "192.168.0.50" },
        { "192.168.0.1" },
        { "255.255.255.0" },
        { "192.168.0.2" },
    };
    const wifiman_wifi_ssid_t ssid = { "test_ssid" };
    json_network_info_update(&ssid, &network_info, UPDATE_CONNECTION_OK);
    json_network_set_extra_info("\"rssi\":-42");
    const string json_str = json_network_info_get();
    ASSERT_EQ(
        string("{"
               "\"ssid\":\"test_ssid\","
               "\"ip\":\"192.168.0.50\","
               "\"netmask\":\"255.255.255.0\","
               "\"gw\":\"192.168.0.1\","
               "\"dhcp\":\"192.168.0.2\","
               "\"urc\":0,"
               "\"extra\":{\"rssi\":-42}"
               "}\n"),
        json_str);
}

TEST_F(TestJsonNetworkInfo, test_set_extra_info_is_generated_when_reason_is_undefined) // NOLINT
{
    json_network_set_extra_info("\"state\":\"idle\"");
    const string json_str = json_network_info_get();
    ASSERT_EQ(string("{\"extra\":{\"state\":\"idle\"}}\n"), json_str);
}

TEST_F(TestJsonNetworkInfo, test_set_extra_info_null_clears_extra_section) // NOLINT
{
    json_network_set_extra_info("\"state\":\"idle\"");
    json_network_set_extra_info(nullptr);
    const string json_str = json_network_info_get();
    ASSERT_EQ(string("{}\n"), json_str);
}
