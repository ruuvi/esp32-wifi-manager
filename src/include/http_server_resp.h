/**
 * @file http_server_resp.h
 * @author TheSomeMan
 * @date 2020-10-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef WIFI_MANAGER_HTTP_SERVER_RESP_H
#define WIFI_MANAGER_HTTP_SERVER_RESP_H

#include "wifi_manager_defs.h"
#include "esp_type_wrapper.h"
#include "http_server_auth_type.h"
#include "sta_ip.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP_SERVER_RESP_JSON_AUTH_BUF_SIZE (210U)

#define HTTP_SERVER_EXTRA_HEADER_FIELDS_SIZE (380U)

typedef struct http_header_extra_fields_t
{
    char buf[HTTP_SERVER_EXTRA_HEADER_FIELDS_SIZE];
} http_header_extra_fields_t;

typedef struct http_server_resp_auth_json_t
{
    char buf[HTTP_SERVER_RESP_JSON_AUTH_BUF_SIZE];
} http_server_resp_auth_json_t;

http_server_resp_t
http_server_resp_json_in_heap(const http_resp_code_e http_resp_code, char* const p_json_content);

http_server_resp_t
http_server_resp_err(const http_resp_code_e http_resp_code);

http_server_resp_t
http_server_resp_200_json(const char* const p_json_content);

http_server_resp_t
http_server_resp_200_json_in_heap(char* const p_json_content);

http_server_resp_t
http_server_resp_json_generator(const http_resp_code_e http_resp_code, json_stream_gen_t* const p_json_gen);

http_server_resp_t
http_server_resp_200_json_generator(json_stream_gen_t* const p_json_gen);

http_server_resp_t
http_server_resp_text_in_heap(const http_resp_code_e http_resp_code, char* const p_content);

http_server_resp_t
http_server_resp_302(void);

http_server_resp_t
http_server_resp_400(void);

http_server_resp_t
http_server_resp_401_json(const http_server_resp_auth_json_t* const p_auth_json);

http_server_resp_t
http_server_resp_403_json(const http_server_resp_auth_json_t* const p_auth_json);

http_server_resp_t
http_server_resp_403(void);

http_server_resp_t
http_server_resp_404(void);

http_server_resp_t
http_server_resp_409(void);

http_server_resp_t
http_server_resp_500(void);

http_server_resp_t
http_server_resp_502(void);

http_server_resp_t
http_server_resp_502_json_in_heap(char* const p_json);

http_server_resp_t
http_server_resp_503(void);

http_server_resp_t
http_server_resp_504(void);

http_server_resp_t
http_server_resp_data_in_flash(
    const http_content_type_e     content_type,
    const char* const             p_content_type_param,
    const size_t                  content_len,
    const http_content_encoding_e content_encoding,
    const uint8_t* const          p_buf,
    const bool                    flag_no_cache);

http_server_resp_t
http_server_resp_data_in_static_mem(
    const http_content_type_e     content_type,
    const char* const             p_content_type_param,
    const size_t                  content_len,
    const http_content_encoding_e content_encoding,
    const uint8_t* const          p_buf,
    const bool                    flag_no_cache,
    const bool                    flag_add_header_date);

http_server_resp_t
http_server_resp_200_data_in_heap(
    const http_content_type_e     content_type,
    const char* const             p_content_type_param,
    const size_t                  content_len,
    const http_content_encoding_e content_encoding,
    uint8_t* const                p_buf,
    const bool                    flag_no_cache,
    const bool                    flag_add_header_date);

http_server_resp_t
http_server_resp_data_from_file(
    http_resp_code_e              http_resp_code,
    const http_content_type_e     content_type,
    const char* const             p_content_type_param,
    const size_t                  content_len,
    const http_content_encoding_e content_encoding,
    const socket_t                fd,
    const bool                    flag_no_cache);

http_server_resp_t
http_server_resp_401_auth_digest(
    const wifiman_hostinfo_t* const   p_hostinfo,
    http_header_extra_fields_t* const p_extra_header_fields);

http_server_resp_t
http_server_resp_401_auth_ruuvi_with_new_session_id(
    const sta_ip_string_t* const      p_remote_ip,
    const wifiman_hostinfo_t* const   p_hostinfo,
    http_header_extra_fields_t* const p_extra_header_fields,
    const http_server_auth_type_e     lan_auth_type,
    const char* const                 p_err_message);

http_server_resp_t
http_server_resp_401_auth_ruuvi(
    const wifiman_hostinfo_t* const p_hostinfo,
    const http_server_auth_type_e   lan_auth_type);

http_server_resp_t
http_server_resp_200_auth_allow_with_new_session_id(
    const sta_ip_string_t* const      p_remote_ip,
    const wifiman_hostinfo_t* const   p_hostinfo,
    http_header_extra_fields_t* const p_extra_header_fields);

http_server_resp_t
http_server_resp_403_auth_deny(const wifiman_hostinfo_t* const p_hostinfo);

http_server_resp_t
http_server_resp_403_forbidden(void);

/**
 * @brief Generate an authentication JSON object for HTTP server responses.
 *
 * This function populates a JSON object containing authentication-related details
 * for the HTTP server response based on the provided parameters. The JSON format
 * can include various fields such as the gateway name, firmware versions, LAN
 * authentication type, and LAN access status.
 *
 * @note The firmware version information should not be exposed to unauthenticated users.
 *       Therefore, the `flag_add_ver_info` parameter should be set to `false`.
 *
 * @param[in] p_hostinfo Pointer to the host information structure. Must not be NULL.
 * @param[in] lan_auth_type The LAN authentication type as an enumerated value of type http_server_auth_type_e.
 * @param[in] flag_access_from_lan Boolean flag indicating if access is from LAN.
 * @param[in] p_err_message Optional error message string. Can be NULL if no error message is needed.
 * @param[in] flag_expose_ver_info Boolean flag indicating whether to include version
 *                                 information in the JSON output.
 *                                 It should be set to false for unauthenticated users to avoid exposing
 *                                 firmware version details.
 *
 * @return A pointer to the generated authentication JSON object.
 *         The JSON object is stored in a statically allocated buffer, so the pointer
 *         remains valid until the next call to this function.
 */
const http_server_resp_auth_json_t*
http_server_fill_auth_json(
    const wifiman_hostinfo_t* const p_hostinfo,
    const http_server_auth_type_e   lan_auth_type,
    const bool                      flag_access_from_lan,
    const char* const               p_err_message,
    const bool                      flag_expose_ver_info);

void
http_server_resp_free(http_server_resp_t* const p_resp);

/**
 * @brief Get a pointer to response content when that content is stored in memory.
 *
 * @param[IN] p_resp Pointer to the response object. Must not be NULL.
 * @param[OUT] p_len Pointer to a variable to store the length of the content. Must not be NULL.
 *
 * @return A pointer to the response content if it is stored in memory, NULL otherwise.
 */
const uint8_t*
http_server_resp_get_content_ptr_if_in_memory(const http_server_resp_t* const p_resp, size_t* const p_len);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_HTTP_SERVER_RESP_H
