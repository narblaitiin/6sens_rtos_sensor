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

//  ========== bcd_to_bin ==================================================================
static uint8_t bcd_to_bin(uint8_t val)
{
    return ((val >> 4) * 10) + (val & 0x0f);
}

//  ========== bin_to_bcd ==================================================================
static uint8_t bin_to_bcd(uint8_t val)
{
    return ((val / 10) << 4) | (val % 10);
}

//  ========== ds3231_read_unix ============================================================
// reads DS3231 registers directly via I2C, bypassing counter_get_value
static int ds3231_read_unix(uint32_t *unix_secs)
{
    const struct device *i2c = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    if (!device_is_ready(i2c)) {
        return -ENODEV;
    }

    uint8_t reg = DS3231_REG_TIME;
    uint8_t buf[7];

    int ret = i2c_write_read(i2c, DS3231_I2C_ADDR, &reg, 1, buf, 7);
    if (ret < 0) {
        printk("DS3231 i2c read failed: %d\n", ret);
        return ret;
    }

    // convert registers to struct tm
    struct tm utc = {
        .tm_sec  = bcd_to_bin(buf[0] & 0x7f),
        .tm_min  = bcd_to_bin(buf[1] & 0x7f),
        .tm_hour = bcd_to_bin(buf[2] & 0x3f),
        .tm_mday = bcd_to_bin(buf[4] & 0x3f),
        .tm_mon  = bcd_to_bin(buf[5] & 0x1f) - 1,
        .tm_year = bcd_to_bin(buf[6]) + 100,
    };

    // convert to unix seconds
    *unix_secs = (uint32_t)timeutil_timegm64(&utc);

    printk("DS3231 direct read: %02d:%02d:%02d %02d/%02d/%04d -> unix=%u\n",
           utc.tm_hour, utc.tm_min, utc.tm_sec,
           utc.tm_mday, utc.tm_mon + 1, utc.tm_year + 1900,
           *unix_secs);

    return 0;
}

//  ========== app_ds3231_init =============================================================
const struct device *app_ds3231_init(void)
{
    //const struct device *dev = DEVICE_DT_GET_ONE(maxim_ds3231);
    const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    if (!device_is_ready(dev)) {   // ← correct check
        printk("DS3231 not ready\n");
        return NULL;
    }

    k_mutex_init(&offset_mutex);
    printk("DS3231 ready\n");
    return dev;
}

//  ========== app_ds3231_set_time =========================================================
int8_t app_ds3231_set_time(const struct device *ds3231_dev, uint32_t unix_secs)
{
    if (!device_is_ready(ds3231_dev)) {
        return -ENODEV;
    }

    const struct device *i2c = DEVICE_DT_GET(DT_NODELABEL(i2c0));
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

    int ret = i2c_write(i2c, buf, sizeof(buf), DS3231_I2C_ADDR);
    if (ret < 0) {
        printk("i2c_write failed: %d\n", ret);
        return ret;
    }

    k_sleep(K_MSEC(1100));

    // read back directly via I2C, NOT counter_get_value
    uint32_t readback;
    ret = ds3231_read_unix(&readback);
    if (ret < 0) {
        return ret;
    }

    int64_t uptime_ms = k_uptime_get();
    int64_t target_ms = (int64_t)readback * 1000;

    k_mutex_lock(&offset_mutex, K_FOREVER);
    rtc_offset_ms = target_ms - uptime_ms;
    k_mutex_unlock(&offset_mutex);

    printk("DS3231 time set to unix=%u, offset=%lld ms\n",
           readback, rtc_offset_ms);
    return 0;
}

//  ========== app_ds3231_periodic_sync ====================================================
int8_t app_ds3231_periodic_sync(const struct device *ds3231_dev)
{
    uint32_t unix_secs;
    int ret = ds3231_read_unix(&unix_secs);   // direct I2C, not counter_get_value
    if (ret < 0) {
        printk("DS3231 periodic sync failed: %d\n", ret);
        return ret;
    }

    const struct device *nrf_rtc = DEVICE_DT_GET(DT_NODELABEL(rtc2));
    uint32_t ticks;
    counter_get_value(nrf_rtc, &ticks);
    uint32_t freq   = counter_get_frequency(nrf_rtc);
    int64_t tick_ms = ((int64_t)ticks * 1000) / freq;

    int64_t new_offset = (int64_t)unix_secs * 1000 - tick_ms;

    k_mutex_lock(&offset_mutex, K_FOREVER);
    rtc_offset_ms = new_offset;
    k_mutex_unlock(&offset_mutex);

    printk("sync: DS3231=%u s, tick_ms=%lld ms, offset=%lld ms\n",
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