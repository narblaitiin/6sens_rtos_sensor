/*
 * Copyright (c) 2025
 * Regis Rousseau
 * Univ Lyon, INSA Lyon, Inria, CITI, EA3720
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_DS3231_H
#define APP_DS3231_H

//  ========== includes ==============================================================================
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/i2c.h>
#include <time.h>

//  ========== defines ===============================================================================
#define DS3231_I2C_ADDR     0x68
#define DS3231_REG_TIME     0x00

//  ========== prototypes ============================================================================
const struct device *app_ds3231_init(void);
int8_t app_ds3231_set_time(const struct device *ds3231_dev, uint32_t unix_secs);
int8_t app_ds3231_periodic_sync(const struct device *ds3231_dev);
uint64_t app_get_timestamp(void);

#endif /* APP_DS3231_H */