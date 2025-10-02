/*
 * Copyright (c) 2025
 * Regis Rousseau
 * Univ Lyon, INSA Lyon, Inria, CITI, EA3720
 * SPDX-License-Identifier: Apache-2.0
 */

//  ========== includes ====================================================================
#include "app_adc.h"
#include "app_sensors.h"
#include "app_sta_lta.h"

//  ========== globals =====================================================================
static void dl_callback(uint8_t port, bool data_pending,
			int16_t rssi, int8_t snr,
			uint8_t len, const uint8_t *hex_data)
{
	printk("Port %d, Pending %d, RSSI %ddB, SNR %ddBm\n", port, data_pending, rssi, snr);
}


// thread to send environment value when no activity
bool bth_thread_flag = true;
void bth_thread_func(void)
{
	printk("sensor thread started\n");

	while (bth_thread_flag == true) {
        printk("performing periodic action\n");
		// perform your task: get battery level, temperature and humidity
        (void)app_sensors_handler();
        k_sleep(K_SECONDS(120));		
	}
}
K_THREAD_DEFINE(bth_thread_id, 2048, bth_thread_func, NULL, NULL, NULL, PRIORITY_BTH, 0, 0);

static void lorwan_datarate_changed(enum lorawan_datarate dr)
{
	uint8_t unused, max_size;

	lorawan_get_payload_sizes(&unused, &max_size);
	printk("New Datarate: DR_%d, Max Payload %d\n", dr, max_size);
}

//  ========== main ========================================================================
int8_t main(void)
{
	const struct device *dev;
	int8_t ret;

	printk("Initializtion of all Hardware Devices\n");

	// initialize ADC device
	ret = app_nrf52_adc_init();
	if (ret != 1) {
		printk("failed to initialize ADC device\n");
		return 0;
	}

	// initialize DS3231 RTC device via I2C (Pins: SDA -> P0.09, SCL -> P0.0)
	const struct device *rtc_dev = app_ds3231_init();
    if (!rtc_dev) {
        printk("failed to initialize RTC device\n");
        return 0;
    }

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

	printk("Joining network over OTAA\n");
	ret = lorawan_join(&join_cfg);
	if (ret < 0) {
		printk("lorawan_join_network failed. error %d\n", ret);
		return 0;
	}

	printk("Geophone Measurement and Process Information\n");

	// enable environmental sensor and battery level thread
	bth_thread_flag = false;

	// start ADC sampling and LTA threads
    app_adc_sampling_start();

	// start recording data and sent to TTN
	app_lorawan_start_tx();

	// start strategy to watch an event without sent the event
	//app_sta_lta_start();

	return 0;
}