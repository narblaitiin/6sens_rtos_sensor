cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hardware_tests)

set(env_node_id $ENV{NODE_ID})
if(NOT DEFINED ENV{NODE_ID})
    set(env_node_id 666)
    message("Using debug NODE ID : ${env_node_id}")
else()
    message(WARNING "Using NODE ID ${env_node_id} for testing !")
endif()




zephyr_compile_definitions(NODE_ID=${env_node_id})
zephyr_compile_definitions("SASTRESS_LOG_LVL=LOG_LVL_DBG")

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
target_sources(app PRIVATE ../../src/app_adc.c)
target_sources(app PRIVATE ../../src/app_sht31.c)
target_sources(app PRIVATE ../../src/app_ds3231.c)
target_sources(app PRIVATE ../../src/lorawan.c)
target_include_directories(app PRIVATE ../../src/)

