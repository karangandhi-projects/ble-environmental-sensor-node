# Standalone "unit-test-app" project for running on-target Unity tests.
#
# Usage:
#   cd firmware/test_app
#   idf.py set-target esp32c3
#   idf.py build flash monitor
#
# It includes all components from ../components/ and runs every test/
# subdirectory's TEST_CASE() registrations via ESP-IDF's test framework.
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components")
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components/app_core/test")
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components/ble_env/test")
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components/env_sensor/test")
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components/display/test")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ble_env_unit_tests)
