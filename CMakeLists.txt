# Copyright (c) 2024 HalfSweet
# SPDX-License-Identifier: MIT

sdk_inc(${CMAKE_CURRENT_SOURCE_DIR}/Lib)
sdk_src(${CMAKE_CURRENT_SOURCE_DIR}/Lib/WS2812.c)
# sdk_compile_definitions(-DWS2812_DEBUG=printf)
