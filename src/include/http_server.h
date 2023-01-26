/*
Copyright (c) 2017-2019 Tony Pottier

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

@file http_server.h
@author Tony Pottier
@brief Defines all functions necessary for the HTTP server to run.

Contains the freeRTOS task for the HTTP listener and all necessary support
function to process requests, decode URLs, serve files, etc. etc.

@note http_server task cannot run without the wifi_manager task!
@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#ifndef HTTP_SERVER_H_INCLUDED
#define HTTP_SERVER_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>
#include "http_server_auth_type.h"
#include "attribs.h"
#include "str_buf.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum http_server_user_req_code_e
{
    HTTP_SERVER_USER_REQ_CODE_1 = 1,
    HTTP_SERVER_USER_REQ_CODE_2 = 2,
} http_server_user_req_code_e;

/**
 * @brief Init the http server.
 * @brief This function should be executed before start/stop.
 */
void
http_server_init(void);

void
http_server_sema_send_wait_immediate(void);

bool
http_server_sema_send_wait_timeout(const uint32_t send_timeout_ms);

/**
 * @brief Create the task for the http server.
 */
void
http_server_start(void);

/**
 * @brief Stop the http server task.
 */
ATTR_UNUSED
void
http_server_stop(void);

void
http_server_user_req(const http_server_user_req_code_e req_code);

bool
http_server_decrypt(const char* const p_http_body, str_buf_t* const p_str_buf);

bool
http_server_decrypt_by_params(
    const char* const p_encrypted_val,
    const char* const p_iv,
    const char* const p_hash,
    str_buf_t* const  p_str_buf);

#ifdef __cplusplus
}
#endif

#endif
