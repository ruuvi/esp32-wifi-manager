/**
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_mutex.h"
#include <esp_attr.h>
#include "os_mutex.h"
#include "http_server.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"
static const char TAG[] = "http_server";

static os_mutex_t IRAM_ATTR g_p_mutex_accept_conn;

void
http_server_use_mutex_for_incoming_connection_handling(os_mutex_t p_mutex)
{
    if (NULL != p_mutex)
    {
        if (NULL == g_p_mutex_accept_conn)
        {
            LOG_INFO("Activate using mutex for http_server");
            g_p_mutex_accept_conn = p_mutex;
        }
    }
    else
    {
        if (NULL != g_p_mutex_accept_conn)
        {
            LOG_INFO("Deactivate using mutex for http_server");
            g_p_mutex_accept_conn = NULL;
        }
    }
}

os_mutex_t
http_server_get_mutex(void)
{
    return g_p_mutex_accept_conn;
}
