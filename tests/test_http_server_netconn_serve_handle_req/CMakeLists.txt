cmake_minimum_required(VERSION 3.7)

project(ruuvi_esp32-wifi-manager-test-http_server_netconn_serve_handle_req)
set(ProjectId ruuvi_esp32-wifi-manager-test-http_server_netconn_serve_handle_req)

add_executable(${ProjectId}
        test_http_server_netconn_serve_handle_req.cpp
        ../../src/http_server_netconn_serve_handle_req.c
        ../../src/http_server_netconn_serve_handle_req.h
        ../../src/http_req.c
        ../../src/http_req.h
        ../ruuvi.esp_wrappers.c/src/os_malloc.c
        ../ruuvi.esp_wrappers.c/include/os_malloc.h
        ../ruuvi.esp_wrappers.c/src/str_buf.c
        ../ruuvi.esp_wrappers.c/include/str_buf.h
        ../ruuvi.esp_wrappers.c/src/snprintf_with_esp_err_desc.c
        ../ruuvi.esp_wrappers.c/include/snprintf_with_esp_err_desc.h
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
        ${CMAKE_CURRENT_SOURCE_DIR}
        $ENV{IDF_PATH}/components/esp_wifi/include
        $ENV{IDF_PATH}/components/esp_common/include
        $ENV{IDF_PATH}/components/json/cJSON
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_HTTP_SERVER_NETCONN_SERVE_HANDLE_REQ=1
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
