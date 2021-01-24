cmake_minimum_required(VERSION 3.5)

set(PROJECT_VER "0.9.2")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
list(APPEND EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/azure-iot-device/components"
    "${CMAKE_CURRENT_LIST_DIR}/esp-azure/port"
    "${CMAKE_CURRENT_LIST_DIR}/esp-idf-lib/components")

project(esp32_azure_iot)
