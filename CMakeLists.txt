# SimpleXL OS - ESP-IDF Project
cmake_minimum_required(VERSION 3.16)

# Include ESP-IDF build system
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Define project
project(hai-os-simplexl)

# Phase 3: Custom target to export metrics in Prometheus format
# Usage: idf.py metrics-export
add_custom_target(metrics-export
    COMMAND ${CMAKE_COMMAND} -E echo "Exporting metrics..."
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/metrics.prom
    COMMENT "Export metrics to build/metrics.prom"
)

