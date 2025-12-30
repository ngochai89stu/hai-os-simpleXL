cmake_minimum_required(VERSION 3.16)

# ESP-IDF project
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Check if building tests
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # Main project
    project(hai_os_simplexl)
else
    # Test project
    project(hai_os_simplexl_test)
endif()
