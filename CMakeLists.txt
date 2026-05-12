cmake_minimum_required(VERSION 3.20.0)

# =========== Retrieve build parameters =====================
# Set default log level (OFF, ERR, WRN, INF, DBG)
set(SASTRESS_LOG_LVL LOG_LEVEL_INF)

message("========= SASTRESS PARAMETERS ==========")
# NODE ID handling
set(env_node_id $ENV{NODE_ID})
if(NOT DEFINED ENV{NODE_ID})
    message(FATAL_ERROR "La variable d'environnement NODE_ID n'est pas définie ! Faites 'export NODE_ID=X'")
endif()

message("NODE ID is ${env_node_id}")


# Deploy Mode :
# - Change logging system from Serial to Filesystem
# - Set default logging code to LOG_LEVEL_INF
if(DEFINED ENV{DEPLOY})
    message("Building DEPLOY configuration !")
    set(EXTRA_CONF_FILE deploy.conf)
    set(SASTRESS_LOG_LVL LOG_LEVEL_INF)
endif()
message("========================================")


# =============== Actual CMakeList ====================
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(6sens_rtos_sensor)
zephyr_compile_definitions("SASTRESS_LOG_LVL=${SASTRESS_LOG_LVL}")
zephyr_compile_definitions(NODE_ID=${env_node_id})
FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})


