/*
 * Copyright (c) 2025
 * Regis Rousseau
 * Univ Lyon, INSA Lyon, Inria, CITI, EA3720
 * SPDX-License-Identifier: Apache-2.0
 */

//  ========== includes ====================================================================
#include "app_adc.h"
#include "config.h"
#include "app_ds3231.h"
#include "lorawan.h"
#include "app_sensors.h"
#include "periodic_samples.h"
#include "app_sta_lta_tx.h"
#include "fs_utils.h"

#include <zephyr/sys/reboot.h>
#include <zephyr/lorawan/lorawan.h>

#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
LOG_MODULE_REGISTER(main);

// define GPIO configurations for the LEDs
static const struct gpio_dt_spec led_0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
// static const struct gpio_dt_spec led_1 = GPIO_DT_SPEC_GET(LED_1, gpios);
//  ========== RTC thread ==================================================================
// thread to have periodic synchronisation of timestamp
K_SEM_DEFINE(init_done_sem, 0, 1);

void rtc_thread_func(void)
{
    k_sem_take(&init_done_sem, K_FOREVER);
    const struct device *ds3231_dev = DEVICE_DT_GET_ONE(maxim_ds3231);

    while (true) {
        k_sleep(K_SECONDS(30));                     // wait first, then sync
        app_ds3231_periodic_sync(ds3231_dev);       // re-anchor offset to DS3231
    }
}
K_THREAD_DEFINE(rtc_thread_id, 2048, rtc_thread_func,
                NULL, NULL, NULL, 2, 0, K_TICKS_FOREVER);

//  ========== sensor thread ===============================================================
// thread to send environment value when no activity
bool bth_thread_flag = true;

void bth_thread_func(void)
{
    LOG_INF("sensor thread started");
    while (bth_thread_flag == true) {
        LOG_INF("performing periodic sensor read");
        (void)app_sensors_handler();
        k_sleep(BTH_PERIOD);
    }
}
K_THREAD_DEFINE(bth_thread_id, 2048, bth_thread_func,
                NULL, NULL, NULL, PRIORITY_TTN, 0, K_TICKS_FOREVER);


// Synchro clock 
int sync_clock(const struct device * ds3231_dev)
{
    int ret;
    uint32_t gps_time;
    time_t unix_time;
    struct tm timeinfo;
    char buf[32];

    ret = lorawan_request_device_time(1);
    if (ret != 0)
    {
        LOG_ERR("lorawan_request_device_time returned %d\n", ret);
        return ret;
    }

    /*
    * Once time synchronisation has occurred, lorawan_clock_sync_get() can
    * be called to populate an uint32_t variable with GPS Time. This is the
    * number of seconds since Jan 6th 1980
    */
    ret = lorawan_device_time_get(&gps_time);
    if (ret != 0)
    {
        LOG_ERR("lorawan_device_time_get returned %d\n", ret);
        return ret;
    }
    else
    {
        gps_time += 315964782;
        unix_time = gps_time;
        app_ds3231_set_time(ds3231_dev, gps_time);
        localtime_r(&unix_time, &timeinfo);
        strftime(buf, sizeof(buf), "%A %B %d %Y %I:%M:%S %p %Z", &timeinfo);
        LOG_INF("Sync with GPS Time = %lli, UTC Time: %s", unix_time, buf);
    }
    return 0;
}

void setup_leds() {
    	// test if GPIO is ready to use
	if ((!gpio_is_ready_dt(&led_0))) {
        return ;
    }

	// configure the LEDs as active output pins
	gpio_pin_configure_dt(&led_0, GPIO_OUTPUT_ACTIVE);
	//gpio_pin_configure_dt(&led_1, GPIO_OUTPUT_ACTIVE);

    gpio_pin_set_dt(&led_0, 0);
}

int restart_sensor() {
    log_flush();
    sys_reboot(SYS_REBOOT_COLD); // Reset on failure
}

//  ========== main ========================================================================
int main(void)
{
	int8_t ret;

    setup_leds();

	LOG_INF("initializing RTC Devices");
	// initialize DS3231 RTC device via I2C (Pins: SDA -> P0.09, SCL -> P0.0)
	const struct device *ds3231_dev = app_ds3231_init();
    if (!ds3231_dev) {
        LOG_ERR("failed to initialize DS3231");
        restart_sensor();
    }

    ret = mount_lfs();
    if(ret != 0)  {
        LOG_ERR("Could not mount lfs, resetting...");
        restart_sensor();
    }

	// unblock RTC sync thread
    k_sem_give(&init_done_sem);
	ret = lora_init();
	if (ret != 0) {
		LOG_ERR("Could not initalize LoRa");
		restart_sensor();
	}
    LOG_INF("Lora INIT OK...");

	ret = lora_joinnet();
	if (ret != 0) {
		LOG_ERR("Could not connect to LoRa net");
		restart_sensor();
	}
	
    ret = sync_clock(ds3231_dev);
    if(ret != 0)  {
        LOG_ERR("Could not get time, resetting...");
        restart_sensor();
    }

	// start threads and sampling only after all HW is ready
    bth_thread_flag = true;
    if(BTH_ENABLE != 0) {
        k_thread_start(bth_thread_id);
    }

    k_thread_start(rtc_thread_id); 

    LOG_INF("Geophone Measurement and Process Information");
	// start ADC sampling
    app_adc_sampling_start();

	// start storage and strategy to watch an event with sent the event
	app_sta_lta_start_tx();
    

    // Start to send a periodic sample every hour
    if(PERIODIC_SAMPLE_ENABLE != 0) {
        start_periodic_sample();
    }

    start_dump_rm_thread();

    gpio_pin_set_dt(&led_0, 1);
    k_sleep(K_SECONDS(60));
    log_flush();
    gpio_pin_set_dt(&led_0, 0);
	return 0;
}
