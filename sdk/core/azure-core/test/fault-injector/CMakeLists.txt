# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

cmake_minimum_required (VERSION 3.13)

set(azure-core-test-fault-injector)

project (azure-core-test-fault-injector LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable (
  azure-core-test-fault-injector
  fault_injector_test.cpp
)

target_link_libraries(azure-core-test-fault-injector PRIVATE azure-core)

target_compile_definitions(azure-core-test-fault-injector PRIVATE _azure_BUILDING_TESTS)

create_map_file(azure-core-test-fault-injector azure-core-test-fault-injector.map)
