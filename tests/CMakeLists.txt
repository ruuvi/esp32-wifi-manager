cmake_minimum_required(VERSION 3.5)

project(ruuvi_esp32-wifi-manager-tests)

add_definitions(
        -DRUUVI_TESTS=1
        -D_GNU_SOURCE
)

enable_testing()
add_subdirectory(googletest)
add_subdirectory(posix_gcc_simulator)

set(RUUVI_ESP_WRAPPERS ${CMAKE_CURRENT_SOURCE_DIR}/ruuvi.esp_wrappers.c)
set(RUUVI_ESP_WRAPPERS_INC ${RUUVI_ESP_WRAPPERS}/include)

set(RUUVI_ESP_WRAPPERS_TESTS_COMMON ${RUUVI_ESP_WRAPPERS}/tests/common)
set(RUUVI_ESP_WRAPPERS_TESTS_COMMON_INC ${RUUVI_ESP_WRAPPERS_TESTS_COMMON}/include)

set(RUUVI_JSON_STREAM_GEN ${CMAKE_CURRENT_SOURCE_DIR}/ruuvi.json_stream_gen.c)
set(RUUVI_JSON_STREAM_GEN_SRC ${RUUVI_JSON_STREAM_GEN}/src)
set(RUUVI_JSON_STREAM_GEN_INC ${RUUVI_JSON_STREAM_GEN}/include)

include_directories(
        ${RUUVI_ESP_WRAPPERS_INC}
        ${RUUVI_ESP_WRAPPERS_TESTS_COMMON_INC}
        ${RUUVI_JSON_STREAM_GEN_INC}
)

include_directories(
        "posix_gcc_simulator/FreeRTOS_Posix"
        "posix_gcc_simulator/FreeRTOS_Posix/FreeRTOS_Kernel/include"
        "posix_gcc_simulator/FreeRTOS_Posix/FreeRTOS_Kernel/include/freertos"
        "posix_gcc_simulator/FreeRTOS_Posix/FreeRTOS_Kernel/portable/GCC/Posix"
)

add_subdirectory(${RUUVI_ESP_WRAPPERS} ruuvi.esp_wrappers.c)
add_subdirectory(${RUUVI_ESP_WRAPPERS_TESTS_COMMON} ruuvi.esp_wrappers_tests)

add_subdirectory(esp_simul)

add_subdirectory(lwip)

include_directories(
        "lwip/lwip/src/include"
        "lwip/extra"
        "lwip/lwip/contrib/ports/freertos/include"
        "lwip/lwip/contrib/ports/unix/port/include"
)

#include_directories(
#        "$ENV{IDF_PATH}/components/log/include/"
#        "$ENV{IDF_PATH}/components/esp_common/include/"
#        "$ENV{IDF_PATH}/components/esp_event/include/"
#        "$ENV{IDF_PATH}/components/"
#        "${CMAKE_BINARY_DIR}/../../../config/"
#)

add_subdirectory(test_access_points_list)
add_subdirectory(test_ap_ssid)
add_subdirectory(test_http_req)
add_subdirectory(test_http_server_handle_req_get_auth)
add_subdirectory(test_http_server_resp)
add_subdirectory(test_json)
add_subdirectory(test_json_access_points)
add_subdirectory(test_json_network_info)
add_subdirectory(test_sta_ip_unsafe)
add_subdirectory(test_sta_ip_safe)
add_subdirectory(test_wifiman_cfg_blob_convert)

add_test(NAME test_access_points_list
        COMMAND ruuvi_esp32-wifi-manager-test-access_points_list
        --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_esp32-wifi-manager-test-access_points_list>/gtestresults.xml
)

add_test(NAME test_ap_ssid
        COMMAND ruuvi_esp32-wifi-manager-test-ap_ssid
        --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_esp32-wifi-manager-test-ap_ssid>/gtestresults.xml
)

add_test(NAME test_http_req
        COMMAND ruuvi_esp32-wifi-manager-test-http_req
        --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_esp32-wifi-manager-test-http_req>/gtestresults.xml
)

add_test(NAME test_http_server_handle_req_get_auth
        COMMAND ruuvi_esp32-wifi-manager-test-http_server_handle_req_get_auth
        --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_esp32-wifi-manager-test-http_server_handle_req_get_auth>/gtestresults.xml
)

add_test(NAME test_http_server_resp
        COMMAND ruuvi_esp32-wifi-manager-test-http_server_resp
        --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_esp32-wifi-manager-test-http_server_resp>/gtestresults.xml
)

add_test(NAME test_json
        COMMAND ruuvi_esp32-wifi-manager-test-json
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_esp32-wifi-manager-test-json>/gtestresults.xml
)

add_test(NAME test_json_access_points
        COMMAND ruuvi_esp32-wifi-manager-test-json_access_points
        --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_esp32-wifi-manager-test-json_access_points>/gtestresults.xml
)

add_test(NAME test_json_network_info
        COMMAND ruuvi_esp32-wifi-manager-test-json_network_info
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_esp32-wifi-manager-test-json_network_info>/gtestresults.xml
)

add_test(NAME test_sta_ip_unsafe
        COMMAND ruuvi_esp32-wifi-manager-test-sta_ip_unsafe
        --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_esp32-wifi-manager-test-sta_ip_unsafe>/gtestresults.xml
)

add_test(NAME test_sta_ip_safe
        COMMAND ruuvi_esp32-wifi-manager-test-sta_ip_safe
        --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_esp32-wifi-manager-test-sta_ip_safe>/gtestresults.xml
)

add_test(NAME test_wifiman_cfg_blob_convert
        COMMAND ruuvi_esp32-wifi-manager-test-wifiman_cfg_blob_convert
        --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_esp32-wifi-manager-test-wifiman_cfg_blob_convert>/gtestresults.xml
)
