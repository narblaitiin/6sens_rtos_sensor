/*
 * Copyright (c) 2025
 * Regis Rousseau
 * Univ Lyon, INSA Lyon, Inria, CITI, EA3720
 * SPDX-License-Identifier: Apache-2.0
 */

//  ========== includes ===================================================================
#include "app_ds3231.h"

//  ========== globals ====================================================================
static int64_t  rtc_offset_ms = 0;   // anchors nRF ticks to DS3231 unix time
static struct k_mutex offset_mutex;

//  ========== bin_to_bcd ==================================================================
static uint8_t bin_to_bcd(uint8_t val)
{
    return ((val / 10) << 4) | (val % 10);
}

//  ========== app_ds3231_init =============================================================
const struct device *app_ds3231_init(void)
{
    const struct device *dev = DEVICE_DT_GET_ONE(maxim_ds3231);
    if (!device_is_ready(dev)) {   // ← correct check
        printk("DS3231 not ready\n");
        return NULL;
    }

    k_mutex_init(&offset_mutex);
    printk("DS3231 ready\n");
    return dev;
}

//  ========== app_ds3231_set_time =========================================================
// called once at boot to physically write unix time to DS3231 registers
int8_t app_ds3231_set_time(const struct device *ds3231_dev, uint32_t unix_secs)
{
    if (!device_is_ready(ds3231_dev)) {
        printk("DS3231 not ready\n");
        return -ENODEV;
    }

    // write unix time to DS3231 registers via direct I2C
    const struct device *i2c = DEVICE_DT_GET(DT_BUS(DT_INST(0, maxim_ds3231)));
    if (!device_is_ready(i2c)) {
        printk("I2C bus not ready\n");
        return -ENODEV;
    }
    
    struct tm utc;
    time_t t = (time_t)unix_secs;
    gmtime_r(&t, &utc);

    uint8_t buf[8];
    buf[0] = DS3231_REG_TIME;
    buf[1] = bin_to_bcd(utc.tm_sec);
    buf[2] = bin_to_bcd(utc.tm_min);
    buf[3] = bin_to_bcd(utc.tm_hour);
    buf[4] = bin_to_bcd(utc.tm_wday + 1);
    buf[5] = bin_to_bcd(utc.tm_mday);
    buf[6] = bin_to_bcd(utc.tm_mon + 1);
    buf[7] = bin_to_bcd(utc.tm_year - 100);

    int8_t ret = i2c_write(i2c, buf, sizeof(buf), DS3231_I2C_ADDR);
    if (ret < 0) {
        printk("failed to write DS3231. error: %d\n", ret);
        return ret;
    }

    k_sleep(K_MSEC(100));

    // compute initial offset after write
    ret = app_ds3231_periodic_sync(ds3231_dev);
    if (ret < 0) {
        return ret;
    }

    printk("DS3231 time set to unix = %u\n", unix_secs);
    return 0;
}
//  ========== app_ds3231_periodic_sync ====================================================
// re-anchors offset every 30s to correct nRF crystal drift
int8_t app_ds3231_periodic_sync(const struct device *ds3231_dev)
{
    if (!device_is_ready(ds3231_dev)) {
        return -ENODEV;
    }

    // read DS3231 whole unix seconds
    uint32_t unix_secs;
    int ret = counter_get_value(ds3231_dev, &unix_secs);
    if (ret < 0) {
        printk("failed to read DS3231: %d\n", ret);
        return ret;
    }

    // read nRF internal RTC ticks at same moment
    const struct device *nrf_rtc = DEVICE_DT_GET(DT_NODELABEL(rtc2));
    uint32_t ticks;
    counter_get_value(nrf_rtc, &ticks);
    uint32_t freq   = counter_get_frequency(nrf_rtc);
    int64_t tick_ms = ((int64_t)ticks * 1000) / freq;

    // offset = DS3231_unix_ms - nRF_tick_ms
    int64_t new_offset = (int64_t)unix_secs * 1000 - tick_ms;

    k_mutex_lock(&offset_mutex, K_FOREVER);
    rtc_offset_ms = new_offset;
    k_mutex_unlock(&offset_mutex);

    printk("sync: DS3231 = %u s, tick_ms = %lld ms, offset = %lld ms\n",
           unix_secs, tick_ms, new_offset);
    return 0;
}

//  ========== app_get_timestamp ===========================================================
// returns unix timestamp in ms with sub-second precision from nRF RTC ticks
uint64_t app_get_timestamp(void)
{
    const struct device *nrf_rtc = DEVICE_DT_GET(DT_NODELABEL(rtc2));
    uint32_t ticks;
    counter_get_value(nrf_rtc, &ticks);
    uint32_t freq   = counter_get_frequency(nrf_rtc);
    int64_t tick_ms = ((int64_t)ticks * 1000) / freq;

    int64_t offset;
    k_mutex_lock(&offset_mutex, K_FOREVER);
    offset = rtc_offset_ms;
    k_mutex_unlock(&offset_mutex);

    int64_t result = tick_ms + offset;
    if (result < 0) {
        return 0;
    }
    return (uint64_t)result;
}