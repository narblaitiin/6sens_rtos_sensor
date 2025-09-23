/*
 * Copyright (c) 2025
 * Regis Rousseau
 * Univ Lyon, INSA Lyon, Inria, CITI, EA3720
 * SPDX-License-Identifier: Apache-2.0
 */

//  ========== includes ==============================================================================
#include "app_ds3231.h"

//  ========== globals ===============================================================================
static int64_t system_rtc_offset_ms = 0;
static struct k_mutex offset_mutex;

//  ========== bcd_to_bin =========================================================================== 
static uint8_t bcd_to_bin(uint8_t val)
{
    return ((val >> 4) * 10) + (val & 0x0F);
}

// ========== bin_to_bcd ============================================================================
static uint8_t bin_to_bcd(uint8_t val)
{
    return ((val / 10) << 4) | (val % 10);
}

// ========== app_i2c_write_time =====================================================================
int8_t app_i2c_write_time(const struct device *i2c_dev, const struct tm *tm)
{
    uint8_t time_buf[7];
    int8_t ret;

    if (!i2c_dev || !tm) {
        return -EINVAL;
    }

    time_buf[0] = bin_to_bcd(tm->tm_sec);
    time_buf[1] = bin_to_bcd(tm->tm_min);
    time_buf[2] = bin_to_bcd(tm->tm_hour);
    time_buf[3] = bin_to_bcd(tm->tm_wday + 1);         // struct tm: 0=Sun → DS3231: 1=Sun
    time_buf[4] = bin_to_bcd(tm->tm_mday);
    time_buf[5] = bin_to_bcd(tm->tm_mon + 1);          // struct tm: 0=Jan → DS3231: 1=Jan
    time_buf[6] = bin_to_bcd(tm->tm_year - 100);       // struct tm: years since 1900 → DS3231: years since 2000

    ret = i2c_burst_write(i2c_dev, DS3231_I2C_ADDR, DS3231_REG_TIME, time_buf, sizeof(time_buf));
    if (ret < 0) {
        printk("failed to write time to DS3231: %d\n", ret);
        return ret;
    }

    printk("DS3231 time set successfully\n");
    return 0;
}

//  ========== app_i2c_read_time ==================================================================== 
int8_t app_i2c_read_time(const struct device *i2c_dev, struct tm *tm)
{
    uint8_t time_buf[7];
    int8_t ret;

    if (!i2c_dev || !tm) {
        return -EINVAL;
    }

    ret = i2c_burst_read(i2c_dev, DS3231_I2C_ADDR, DS3231_REG_TIME, time_buf, sizeof(time_buf));
    if (ret < 0) {
        printk("failed to read DS3231 registers. error: %d\n", ret);
        return ret;
    }

    tm->tm_sec  = bcd_to_bin(time_buf[0] & 0x7F);
    tm->tm_min  = bcd_to_bin(time_buf[1] & 0x7F);
    tm->tm_hour = bcd_to_bin(time_buf[2] & 0x3F);  // 24-hour format
    tm->tm_wday = bcd_to_bin(time_buf[3] & 0x07) - 1;  // DS3231: 1=Sun...7=Sat → struct tm: 0=Sun...6=Sat
    tm->tm_mday = bcd_to_bin(time_buf[4] & 0x3F);
    tm->tm_mon  = bcd_to_bin(time_buf[5] & 0x1F) - 1;  // struct tm: 0=Jan
    tm->tm_year = bcd_to_bin(time_buf[6]) + 100;       // since 1900 → 2000 + xx

    return 0;
}

//  ========== app_rtc_init ==========================================================================
const struct device *app_ds3231_init(void)
{
    const struct device *i2c_dev = DEVICE_DT_GET_ONE(maxim_ds3231);
    if (!i2c_dev) {
        printk("no DS3231 device found\n");
        return NULL;
    }

    // initialize mutex
    k_mutex_init(&offset_mutex);

    printk("DS3231 initialized and started successfully (device: %s)\n", i2c_dev->name);
    return i2c_dev;
}

//  ========== app_ds3231_set_time ==================================================================
void app_ds3231_set_time(const struct device *i2c_dev, int64_t unix_time)
{
    struct tm tm;
    time_t t = (time_t)unix_time;
    gmtime_r(&t, &tm);  
    //timeutil_localtime64(unix_time, &tm);
    app_i2c_write_time(i2c_dev, &tm);
}

//  ========== app_ds3231_sync_uptime ================================================================
// synchronize the system uptime with the DS3231 RTC
int8_t app_ds3231_sync_uptime(const struct device *i2c_dev)
{
    if (!i2c_dev) {
        printk("DS3231 device is NULL\n");
        return -EINVAL;
    }

    struct tm rtc_tm;
    int64_t rtc_epoch_s;
    int64_t rtc_epoch_ms;
    int64_t current_uptime_ms;
    int64_t new_offset_ms;

    // get time from external RTC
    if (app_i2c_read_time(i2c_dev, &rtc_tm) != 0) {
        printk("failed to read time from DS3231\n");
        return -EIO;
    }

    // calculate the offset between RTC time and system uptime
    rtc_epoch_s = timeutil_timegm64(&rtc_tm);
    rtc_epoch_ms = rtc_epoch_s * 1000;
    current_uptime_ms = k_uptime_get();
    new_offset_ms = rtc_epoch_ms - current_uptime_ms;

    // safely update the global offset
    k_mutex_lock(&offset_mutex, K_FOREVER);
    system_rtc_offset_ms = new_offset_ms;
    k_mutex_unlock(&offset_mutex);

    // debugging output
    printk("synced: DS3231 epoch_ms = %lld, uptime = %lld, offset = %lld\n",
           rtc_epoch_ms, current_uptime_ms, new_offset_ms);

    return 0;
}

//  ========== app_rtc_get_time ======================================================================
uint64_t app_ds3231_get_time()
{
    int64_t now_ms = k_uptime_get();
    int64_t offset;

    k_mutex_lock(&offset_mutex, K_FOREVER);
    offset = system_rtc_offset_ms;
    k_mutex_unlock(&offset_mutex);

    // protect against overflow
    if (offset > 0 && now_ms > UINT64_MAX - offset) {
        return UINT64_MAX;
    }
    if (offset < 0 && now_ms < (uint64_t)(-offset)) {
        return 0;
    }
    uint64_t timestamp_ms = now_ms + offset;
    return timestamp_ms;
}

//  ========== app_ds3231_periodic_sync ==============================================================
int8_t app_ds3231_periodic_sync(const struct device *i2c_dev)
{
    if (!i2c_dev) {
        printk("RTC device is NULL\n");
        return -EINVAL;
    }
    
    // call this periodically from a thread or workqueue
    int ret = app_ds3231_sync_uptime(i2c_dev);
    if (ret < 0) {
        printk("periodic sync failed, error: %d", ret);
    }
    return 0;
}