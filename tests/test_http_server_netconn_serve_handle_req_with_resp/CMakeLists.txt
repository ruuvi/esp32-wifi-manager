cmake_minimum_required(VERSION 3.7)

project(ruuvi_esp32-wifi-manager-test-http_server_netconn_serve_handle_req_with_resp)
set(ProjectId ruuvi_esp32-wifi-manager-test-http_server_netconn_serve_handle_req_with_resp)

add_executable(${ProjectId}
        test_http_server_netconn_serve_handle_req_with_resp.cpp
        ../../src/http_server_netconn_serve_handle_req.c
        ../../src/http_server_netconn_serve_handle_req.h
        ../../src/http_server_netconn_resp.c
        ../../src/http_server_netconn_resp.h
        ../../src/http_req.c
        ../../src/http_req.h
        ${RUUVI_ESP_WRAPPERS}/src/os_malloc.c
        ${RUUVI_ESP_WRAPPERS}/include/os_malloc.h
        ${RUUVI_ESP_WRAPPERS}/src/str_buf.c
        ${RUUVI_ESP_WRAPPERS}/include/str_buf.h
        ${RUUVI_ESP_WRAPPERS}/src/snprintf_with_esp_err_desc.c
        ${RUUVI_ESP_WRAPPERS}/include/snprintf_with_esp_err_desc.h
        ${RUUVI_JSON_STREAM_GEN_SRC}/json_stream_gen.c
        ${RUUVI_JSON_STREAM_GEN_INC}/json_stream_gen.h
)

set_target_properties(${ProjectId} PROPERTIES
        C_STANDARD 11
        CXX_STANDARD 14
)

target_include_directories(${ProjectId} PUBLIC
        ${gtest_SOURCE_DIR}/include
        ${gtest_SOURCE_DIR}
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/include
        ../../src/include
        ../../src
        include
        ../include
        ${CMAKE_CURRENT_SOURCE_DIR}
        $ENV{IDF_PATH}/components/esp_wifi/include
        $ENV{IDF_PATH}/components/esp_common/include
        $ENV{IDF_PATH}/components/json/cJSON
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_HTTP_SERVER_NETCONN_SERVE_HANDLE_REQ_WITH_RESP=1
        LWIP_SO_SNDTIMEO=1
        LWIP_SO_RCVTIMEO=1
        OS_MALLOC_TRACE=1
        OS_MALLOC_TRACE_DISABLE_TIMESTAMP=1
)

target_compile_options(${ProjectId} PUBLIC
        -g3
        -ggdb
        -fprofile-arcs
        -ftest-coverage
        --coverage
)

# CMake has a target_link_options starting from version 3.13
#target_link_options(${ProjectId} PUBLIC
#        --coverage
#)

target_link_libraries(${ProjectId}
        gtest
        gtest_main
        gcov
#        ruuvi_esp_wrappers
        ruuvi_esp_wrappers-common_test_funcs
        --coverage
)

target_link_options(${ProjectId} PRIVATE
        "-Wl,--wrap=malloc"
        "-Wl,--wrap=calloc"
        "-Wl,--wrap=free"
)
