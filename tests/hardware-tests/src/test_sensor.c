#include <zephyr/ztest.h>
#include "app_sensors.h"

ZTEST(sensor_test, test_temperature)
{   
    const struct device *dev = DEVICE_DT_GET_ONE(sensirion_sht3xd);
    zassert(device_is_ready(dev), "Could not get sensor Sensirion SHT3XD");
    k_sleep(K_SECONDS(1));
    int16_t temp = app_sht_get_temp(dev);
    zassert_within(temp, 2500, 2500);

    // Must wait before checking the temperature again, otherwise it result in an error
    k_sleep(K_SECONDS(1));
    // Check variability
    int16_t prev = app_sht_get_temp(dev);
    for(int i =0; i < 10; i++) {
        k_sleep(K_SECONDS(1));
        int16_t temp = app_sht_get_temp(dev);
        zassert_within(temp, 2500, 2500);
        zassert_within(temp, prev, 100);
        prev = temp; 
    }
}

ZTEST(sensor_test, test_humidity)
{   
    const struct device *dev = DEVICE_DT_GET_ONE(sensirion_sht3xd);
    zassert(device_is_ready(dev), "Could not get sensor Sensirion SHT3XD");
    int16_t hum = app_sht_get_hum(dev);
    zassert(hum < 10000 && hum > 1000, "Humidity not in the range 10, 100");
}


// Setup the test suite
ZTEST_SUITE(sensor_test, NULL, NULL, NULL, NULL, NULL);
