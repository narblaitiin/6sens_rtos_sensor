/*
 * Copyright (c) 2025
 * Regis Rousseau
 * Univ Lyon, INSA Lyon, Inria, CITI, EA3720
 * SPDX-License-Identifier: Apache-2.0
 */

//  ========== includes ====================================================================
#include "app_sensors.h"

//  ========== app_sensors_handler =========================================================
int8_t app_sensors_handler()
{
    int8_t ret;
    uint8_t byte_payload[BYTE_PAYLOAD] = {0};

    // retrieve the current timestamp from the RTC device
    uint64_t timestamp = app_ds3231_get_time();

    // add timestamp to byte payload (big-endian)
    for (int8_t i = 0; i < 8; i++) {
        byte_payload[i] = (timestamp >> (56 - i * 8)) & 0xFF;
    }
    
    // get sensor device
    const struct device *dev = DEVICE_DT_GET_ONE(sensirion_sht3xd);
    if (!device_is_ready(dev)) {
        printk("sensor device not ready\n");
        return -ENODEV;
    }

    // collect sensor data and add to byte payloa
    int8_t index = 8;           // start after the timestamp
    int16_t bat = app_adc_get_bat();
    int16_t temp = app_sht_get_temp(dev);
    k_sleep(K_SECONDS(5));		// small delay between reading the temperature and humidity values
    int16_t hum = app_sht_get_hum(dev);

    // convert and append each sensor value to byte payload (big-endian)
    int16_t sensor_data[] = {bat, temp, hum};
    for (int j = 0; j < sizeof(sensor_data) / sizeof(sensor_data[0]); j++) {
        byte_payload[index++] = (sensor_data[j] >> 8) & 0xFF;       // high byte
        byte_payload[index++] = sensor_data[j] & 0xFF;              // low byte
    }

    // ret = lorawan_send(LORAWAN_PORT, byte_payload, index, LORAWAN_MSG_UNCONFIRMED);

    // if (ret < 0) {
    //     printk("lorawan_send failed: %d\n", ret);
    //     return ret == -EAGAIN ? 0 : ret;
    // }

    // printk("BTH data sent!\n");
    return 0;
}