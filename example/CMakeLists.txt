cmake_minimum_required(VERSION 3.13)

find_package(hpm-sdk REQUIRED HINTS $ENV{HPM_SDK_BASE})

project(example)

sdk_inc(${CMAKE_CURRENT_SOURCE_DIR}/)

add_subdirectory(..  ${CMAKE_BINARY_DIR}/WS2812)

sdk_app_src(example.c)

generate_ide_projects()