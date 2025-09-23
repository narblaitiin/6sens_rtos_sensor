/*
 * Copyright (c) 2025
 * Regis Rousseau
 * Univ Lyon, INSA Lyon, Inria, CITI, EA3720
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_SENSORS_H
#define APP_SENSORS_H

//  ========== includes ====================================================================
#include "app_adc.h"
#include "app_lorawan.h"
#include "app_sht31.h"

//  ========== defines =====================================================================
#define BYTE_PAYLOAD    12                  // 6 int16 values => 12 bytes                 

//  ========== prototypes ==================================================================
int8_t app_sensors_handler();

#endif /* APP_SENSORS_H */