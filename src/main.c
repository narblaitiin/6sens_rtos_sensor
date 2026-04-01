/*
 * Copyright (c) 2025
 * Regis Rousseau
 * Univ Lyon, INSA Lyon, Inria, CITI, EA3720
 * SPDX-License-Identifier: Apache-2.0
 */

//  ========== includes ====================================================================
#include "app_adc.h"
#include "app_ds3231.h"
#include "lorawan.h"
#include "app_sensors.h"
#include "periodic_samples.h"
#include "app_sta_lta_tx.h"
#include "fs_utils.h"

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
    printk("sensor thread started\n");
    while (bth_thread_flag == true) {
        printk("performing periodic sensor read\n");
        (void)app_sensors_handler();
        k_sleep(K_SECONDS(180));
    }
}
K_THREAD_DEFINE(bth_thread_id, 2048, bth_thread_func,
                NULL, NULL, NULL, PRIORITY_TTN, 0, K_TICKS_FOREVER);



//  ========== main ========================================================================
int main(void)
{
	int8_t ret;

	printk("initializing RTC Devices\n");

	// initialize DS3231 RTC device via I2C (Pins: SDA -> P0.09, SCL -> P0.0)
	const struct device *ds3231_dev = app_ds3231_init();
    if (!ds3231_dev) {
        printk("failed to initialize DS3231\n");
        return 0;
    }

	// set time (also computes initial offset)
    app_ds3231_set_time(ds3231_dev, 1741773600);

	// start nRF internal RTC counter for sub-second precision
    const struct device *nrf_rtc = DEVICE_DT_GET(DT_NODELABEL(rtc2));
    counter_start(nrf_rtc);

	// unblock RTC sync thread
    k_sem_give(&init_done_sem);

	ret = lora_init();
	if (ret != 0) {
		printk("[ERROR] Could not initalize LoRa\n");
		return -1; // TODO make the sensor reset on failure
	}

	ret = lora_joinnet();
	if (ret != 0) {
		printk("[ERROR] Could not connect to LoRa net\n");
		return -1; // TODO make the sensor reset on failure
	}
	printk("Geophone Measurement and Process Information\n");

	// start threads and sampling only after all HW is ready
    bth_thread_flag = true;
    k_thread_start(bth_thread_id);
    k_thread_start(rtc_thread_id); 

	// start ADC sampling
    app_adc_sampling_start();

	// start storage and strategy to watch an event with sent the event
	app_sta_lta_start_tx();
    
    // Start to send a periodic sample every hour
    start_periodic_sample();
	return 0;
}
