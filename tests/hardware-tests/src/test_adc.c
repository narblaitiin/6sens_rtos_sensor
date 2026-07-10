#include <zephyr/ztest.h>
#include "app_adc.h"
#include "app_sta_lta_tx.h"

ZTEST(adc_test, test_battery)
{   
    int16_t prev = app_adc_get_bat();
    for (int i = 0; i < 10; i++)
    {
        int16_t volt = app_adc_get_bat();
        zassert_not_equal(volt, 0, "Battery voltage is zero ! Is the battery connected ?");
        zassert_within(volt, 4000, 1000, "Battery voltage is not within the 3000-5000mV range !");
        zassert_within(volt, prev, 50, "Battery voltage fluctuates more than 50mV");
        prev = volt;
    }
}

ZTEST(adc_test, test_sample_one_by_one)
{
    app_adc_sampling_start();
    uint16_t samples[10];
    uint16_t sample;

    int64_t t_start = k_uptime_get();
    for (int i = 0; i < 10; i++)
    {
        k_sem_take(&data_ready_sem, K_FOREVER);
        app_adc_get_buffer(&sample, 1, -1);
        samples[i] = sample;
    }
    int64_t t_end = k_uptime_get();
    app_adc_sampling_stop();

    float sum = 0.0f;
    for (int i = 0; i < 10; i++)
    {
        uint16_t s = samples[i];
        zassert(s > 0, "Measured sensor voltage is zero !");
        zassert(s < 4090, "Measured sensor voltage is above 4090 !");
        sum += (float) s;
    }
    zassert_within(sum/10, 1700, 500, "Error, the signal recorded is not centered around 1700mV ! Average is %u", (uint16_t) (sum/10));
    zassert_within(t_end - t_start, SAMPLING_RATE_MS * 9.8, SAMPLING_RATE_MS * 10.5, "Took longer than expected");
}

uint16_t samples[LTA_WINDOW_SIZE];

ZTEST(adc_test, test_samples)
{   
    printk("Lauching LTA test : It will take a few seconds, please wait\n");
    for(int i = 0; i < LTA_WINDOW_SIZE; i ++) {
        samples[i] = 0;
    }
    app_adc_sampling_start();

    int64_t t_start = k_uptime_get();
    // Wait until at the LTA window is full
    for (int i = 0; i < LTA_WINDOW_SIZE; i++)
    {
        k_sem_take(&data_ready_sem, K_FOREVER);
    }
    int64_t t_end = k_uptime_get();
    zassert_within(t_end - t_start, LTA_WINDOW_SIZE * SAMPLING_RATE_MS * 0.99, LTA_WINDOW_SIZE * SAMPLING_RATE_MS * 10.5, "Took longer than expected");

    k_sem_take(&data_ready_sem, K_FOREVER);
    app_adc_get_buffer(samples, LTA_WINDOW_SIZE, -LTA_WINDOW_SIZE);

     app_adc_sampling_stop();

    float sum = 0;
    for (int i = 0; i < LTA_WINDOW_SIZE; i++)
    {
        uint16_t s = samples[i];
        sum += s;
        zassert(s > 0, "Measured sensor voltage is zero !");
        zassert(s < 4090, "Measured sensor voltage is above 4090 !");
    }
    zassert_within(sum/ LTA_WINDOW_SIZE, 1700, 1000, "Measured sensor voltage is not within the range 700/2700 : %u", (uint16_t) sum/ LTA_WINDOW_SIZE);
}

// Setup the test suite
ZTEST_SUITE(adc_test, NULL, NULL, NULL, NULL, NULL);