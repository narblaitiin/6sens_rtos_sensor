/*
 * Copyright (c) 2025
 * Regis Rousseau
 * Univ Lyon, INSA Lyon, Inria, CITI, EA3720
 * SPDX-License-Identifier: Apache-2.0
 */

//  ========== includes ====================================================================
#include "app_sensors.h"
#include "data_types.h"

#include "config.h" // for log level
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensors);

//  ========== app_sensors_handler =========================================================
int8_t app_sensors_handler()
{
    int8_t ret;
    
    // get sensor device
    const struct device *dev = DEVICE_DT_GET_ONE(sensirion_sht3xd);
    if (!device_is_ready(dev)) {
        LOG_ERR("sensor device not ready");
        return -ENODEV;
    }

    // collect sensor data and add to byte payloa
    struct bth_payload_t payload;
    payload.battery = app_adc_get_bat();
    payload.temperature = app_sht_get_temp(dev);
    k_sleep(K_SECONDS(5));		// small delay between reading the temperature and humidity values
    payload.humidity = app_sht_get_hum(dev);

    ret = lora_send_packet(BTH, (uint8_t *) &payload, sizeof(struct bth_payload_t));

    LOG_INF("BTH data sent!");
    return 0;
}