# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

cmake_minimum_required (VERSION 3.13)

project (sample6-wrap-unwrap LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable (
  sample6-wrap-unwrap
  sample6_wrap_unwrap.cpp
)

target_compile_definitions(sample6-wrap-unwrap PRIVATE _azure_BUILDING_SAMPLES)

create_per_service_target_build_for_sample(keyvault sample6-wrap-unwrap)

target_link_libraries(sample6-wrap-unwrap PRIVATE azure-security-keyvault-keys azure-identity get-env-helper)
