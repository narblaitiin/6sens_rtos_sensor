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
#include <zephyr/drivers/rtc.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys_clock.h>
#include <time.h>

//  ========== defines ===============================================================================
#define ONE_YEAR_MS         31536000000LL
#define DS3231_I2C_ADDR     0x68
#define DS3231_REG_TIME     0x00

//  ========== prototypes ============================================================================
int8_t app_i2c_write_time(const struct device *i2c_dev, const struct tm *tm);
int8_t app_i2c_read_time(const struct device *i2c_dev, struct tm *tm);
const struct device *app_ds3231_init(void);
void app_ds3231_set_time(const struct device *i2c_dev, int64_t unix_time);
int8_t  app_ds3231_sync_uptime(const struct device *i2c_dev);
uint64_t app_ds3231_get_time();
int8_t app_ds3231_periodic_sync(const struct device *i2c_dev);

#endif /* APP_DS3231_H */