cmake_minimum_required(VERSION 3.7)

project(ruuvi_esp32-wifi-manager-test-wifiman_cfg_blob_convert)
set(ProjectId ruuvi_esp32-wifi-manager-test-wifiman_cfg_blob_convert)

add_executable(${ProjectId}
        test_wifiman_cfg_blob_convert.cpp
        sdkconfig.h
        ../../src/wifiman_config.c
        ../../src/wifiman_config.h
        ../../src/wifiman_cfg_blob_convert.c
        ../../src/wifiman_cfg_blob_convert.h
        $ENV{IDF_PATH}/components/lwip/lwip/src/core/ipv4/ip4_addr.c
        $ENV{IDF_PATH}/components/lwip/lwip/src/core/def.c
)

set_target_properties(${ProjectId} PROPERTIES
        C_STANDARD 11
        CXX_STANDARD 14
)

target_include_directories(${ProjectId} PUBLIC
        ${gtest_SOURCE_DIR}/include
        ${gtest_SOURCE_DIR}
        ../../src/include
        ../../src
        include
        ${CMAKE_CURRENT_SOURCE_DIR}
        $ENV{IDF_PATH}/components/esp_wifi/include
        $ENV{IDF_PATH}/components/esp_common/include
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_WIFIMAN_CFG_BLOB_CONVERT=1
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
        ruuvi_esp_wrappers-common_test_funcs
        --coverage
)
