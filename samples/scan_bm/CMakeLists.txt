#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: BSD-3-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nrf70_scan_bm)

set(NRF70_BM_DIR ../../nrf70_bm_lib)

target_include_directories(app PRIVATE
  src
  ${NRF70_BM_DIR}/include
)

target_sources(
  app
  PRIVATE
  src/main.c
  src/utils.c
)

if(DEFINED CONFIG_NRF70_DEBUG_SHELL)
  target_include_directories(app PRIVATE
    $<TARGET_PROPERTY:nrf-wifi-osal,INTERFACE_INCLUDE_DIRECTORIES>
  )

  target_sources(
    app
    PRIVATE
    src/wifi_util.c
  )
endif()

target_link_libraries(app PRIVATE nrf70-bm-lib)
