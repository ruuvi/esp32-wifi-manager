cmake_minimum_required(VERSION 3.7)

project(ruuvi_esp32-wifi-manager-test-http_server_resp)
set(ProjectId ruuvi_esp32-wifi-manager-test-http_server_resp)

add_executable(${ProjectId}
        test_http_server_resp.cpp
        ../../src/http_server_resp.c
        ../../src/include/http_server_resp.h
        ../../src/http_server_auth.c
        ../../src/http_server_auth.h
        ../../src/http_server_auth_type.c
        ../../src/include/http_server_auth_type.h
        ../../src/wifiman_sha256.c
        ../../src/wifiman_sha256.h
        ${RUUVI_JSON_STREAM_GEN_SRC}/json_stream_gen.c
        ${RUUVI_JSON_STREAM_GEN_INC}/json_stream_gen.h
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/library/sha256.c
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/include/mbedtls/sha256.h
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/library/platform_util.c
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/include/mbedtls/platform_util.h
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
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_HTTP_SERVER_RESP=1
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
        ruuvi_esp_wrappers
        ruuvi_esp_wrappers-common_test_funcs
        --coverage
)
