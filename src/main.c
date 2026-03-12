/*
 * Copyright (c) 2025
 * Regis Rousseau
 * Univ Lyon, INSA Lyon, Inria, CITI, EA3720
 * SPDX-License-Identifier: Apache-2.0
 */

//  ========== includes ====================================================================
#include "app_adc.h"
#include "app_ds3231.h"
#include "app_lorawan.h"
#include "app_sensors.h"
#include "app_sta_lta_tx.h"
#include "fs_utils.h"

//  ========== globals =====================================================================
static void dl_callback(uint8_t port, bool data_pending,
			int16_t rssi, int8_t snr,
			uint8_t len, const uint8_t *hex_data)
{
	printk("Port %d, Pending %d, RSSI %ddB, SNR %ddBm\n", port, data_pending, rssi, snr);
}

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

//  ========== LoRaWAN callbacks ===========================================================
static void lorwan_datarate_changed(enum lorawan_datarate dr)
{
    uint8_t unused, max_size;
    lorawan_get_payload_sizes(&unused, &max_size);
    printk("new datarate: DR_%d, max payload %d\n", dr, max_size);
}

//  ========== main ========================================================================
int8_t main(void)
{
	int8_t ret;
	int clean_fs = false;

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
	
	// initialize LoRaWAN protocol and register the device
	const struct device *lora_dev;
	struct lorawan_join_config join_cfg;
	uint8_t dev_eui[] = LORAWAN_DEV_EUI;
	uint8_t join_eui[] = LORAWAN_JOIN_EUI;
	uint8_t app_key[] = LORAWAN_APP_KEY;

	struct lorawan_downlink_cb downlink_cb = {
		.port = LW_RECV_PORT_ANY,
		.cb = dl_callback
	};

	lora_dev = DEVICE_DT_GET(DT_ALIAS(lora0));
	if (!device_is_ready(lora_dev)) {
		printk("%s: device not ready\n", lora_dev->name);
		return 0;
	}

	ret = lorawan_start();
	if (ret < 0) {
		printk("lorawan_start failed. error: %d", ret);
		return 0;
	}

	lorawan_register_downlink_callback(&downlink_cb);
	lorawan_register_dr_changed_callback(lorwan_datarate_changed);

	join_cfg.mode = LORAWAN_ACT_OTAA;
	join_cfg.dev_eui = dev_eui;
	join_cfg.otaa.join_eui = join_eui;
	join_cfg.otaa.app_key = app_key;
	join_cfg.otaa.nwk_key = app_key;
	join_cfg.otaa.dev_nonce = 0u;

	printk("joining network over OTAA\n");
	ret = lorawan_join(&join_cfg);
	if (ret < 0) {
		printk("lorawan_join_network failed. error %d\n", ret);
		return 0;
	}

	printk("Geophone Measurement and Process Information\n");

	// mount filesystem
	ret = mount_lfs();
	if (ret < 0) {
        printk("mount failed. stopping application: %d\n", ret);
        return ret;
    }
	
	// start threads and sampling only after all HW is ready
    bth_thread_flag = true;
    k_thread_start(bth_thread_id);
    k_thread_start(rtc_thread_id); 

	// dump the content of /lfs filesystem
	//dump_fs(clean_fs);

	// start ADC sampling
    app_adc_sampling_start();

	// start storage and strategy to watch an event with sent the event
	app_sta_lta_start_tx();

	return 0;
}
