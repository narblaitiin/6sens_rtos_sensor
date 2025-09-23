/*
 * Copyright (c) 2025
 * Regis Rousseau
 * Univ Lyon, INSA Lyon, Inria, CITI, EA3720
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_SHT31_H
#define APP_SHT31_H

//  ========== includes ====================================================================
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <stdint.h>

//  ========== defines =====================================================================
#define TEMP_SCALE  100     // scale for converting to int16_t
#define HUM_SCALE   100     // scale for converting to int16_t

//  ========== prototypes ==================================================================
int16_t app_sht31_get_temp(const struct device *dev);
int16_t app_sht31_get_hum(const struct device *dev);

#endif /* APP_SHT31_H */