cmake_minimum_required(VERSION 3.13)

find_package(hpm-sdk REQUIRED HINTS $ENV{HPM_SDK_BASE})

project(example)

target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/
)

find_package(WS2812 REQUIRED)

sdk_app_src(ws2812.c)

sdk_link_libraries(
    WS2812
)

generate_ide_projects()