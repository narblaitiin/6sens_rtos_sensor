/*
 * Copyright (c) 2025
 * Regis Rousseau
 * Univ Lyon, INSA Lyon, Inria, CITI, EA3720
 * SPDX-License-Identifier: Apache-2.0
 */

 //  ========== includes ===================================================================
#include "app_adc.h"

//  ========== globals =====================================================================
// ADC buffer to store raw ADC readings
static uint16_t ring_buffer[ADC_BUFFER_SIZE];
static uint32_t sampling_rate_ms = SAMPLING_RATE_MS;
static uint16_t sample_buffer;
static bool stop_sampling = false;
static bool adc_initialized = false;

// ADC channel configuration obtained from the device tree
#define DT_SPEC_AND_COMMA(node_id, prop, idx) \
    ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

static const struct adc_dt_spec adc_channels[] = {
    DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels,
                         DT_SPEC_AND_COMMA)
};

// define a stack for the ADC thread, with a size of 1024 bytes
K_THREAD_STACK_DEFINE(adc_stack, 1024);

// structure to hold ADC thread data
struct k_thread adc_thread_data;

// mutex to manage concurrent access to the ADC ring buffer
K_MUTEX_DEFINE(buffer_lock);

// mutex to manage access to buffers when retrieving ADC data
K_MUTEX_DEFINE(get_buffer_lock);

// semaphore to signal when new ADC data is available
K_SEM_DEFINE(data_ready_sem, 0, 1);

// semaphore to signal sampling rate change
K_SEM_DEFINE(rate_change_sem, 0, 1);

// index to track the head of the ring buffer
int ring_head = 0;

// nonlinear mapping via lookup table
// source: https://www.jackery.com/blogs/knowledge/battery-voltage-chart
static const struct {
        float voltage;
        uint8_t percent;
} soc_table[] = {
    {4.20f, 100},
    {4.05f,  90},
    {3.95f,  80},
    {3.85f,  70},
    {3.80f,  60},
    {3.75f,  50},
    {3.70f,  40},
    {3.65f,  30},
    {3.55f,  20},
    {3.40f,  10},
    {3.00f,   0}
};

//  ========== app_adc_read_ch =============================================================
static int8_t app_adc_read_ch(size_t ch)
{
    int err;
    const struct adc_dt_spec *spec = &adc_channels[ch];

    if (!device_is_ready(spec->dev)) {
        printk("ADC device not ready\n");
        return -ENODEV;
    }

    err = adc_channel_setup_dt(spec);
    if (err < 0) {
        printk("channel setup failed. error: %d\n", err);
        return err;
    }

    struct adc_sequence sequence = {
        .buffer = &sample_buffer,
        .buffer_size = sizeof(sample_buffer),
        .resolution = 12,
    };

    err = adc_sequence_init_dt(spec, &sequence);
    if (err < 0) {
        printk("sequence init failed. error: %d\n", err);
        return err;
    }

    err = adc_read(spec->dev, &sequence);
    if (err < 0) {
        printk("ADC read failed. error: %d\n", err);
        return err;
    }

    printk("channel[%d] raw value = %d\n", ch, sample_buffer);
    return 0;
}

//  ========== app_adc_get_bat =============================================================
int16_t app_adc_get_bat()
{
    int16_t percent = 0;

    // read sample from the ADC
    app_adc_read_ch(1);

    // convert raw ADC reading to voltage
    int32_t v_adc = (sample_buffer * ADC_FULL_SCALE_MV) / ADC_RESOLUTION;
    printk("convert voltage AIN1: %d mV\n", v_adc);

    // scale back to actual battery voltage using voltage divider
    int32_t v_bat = (v_adc * DIVIDER_RATIO_NUM) / DIVIDER_RATIO_DEN;
    printk("convert voltage BATT: %d mv\n", v_bat);

    // convert to volts
    float vbat = v_bat / 1000.0f;  

    // convert to volts for lookup
    uint8_t soc = 0;
    if (vbat >= soc_table[0].voltage) {
        soc = 100;
    } else if (vbat <= soc_table[sizeof(soc_table)/sizeof(soc_table[0]) - 1].voltage) {
        soc = 0;
    } else {
        for (int i = 0; i < (int)(sizeof(soc_table)/sizeof(soc_table[0]) - 1); i++) {
            float v1 = soc_table[i].voltage;
            float v2 = soc_table[i+1].voltage;
            if (vbat <= v1 && vbat >= v2) {
                float p1 = soc_table[i].percent;
                float p2 = soc_table[i+1].percent;
                soc = (uint8_t)(p1 + (vbat - v1)*(p2 - p1)/(v2 - v1));
                break;
            }
        }
    }

    percent = soc;
    printk("battery level: %d %%\n", percent);
    return percent;
}

// //  ========== adc_thread ===============================================================
static void app_adc_thread(void *arg1, void *arg2, void *arg3)
{
    while (!stop_sampling) {
         if (app_adc_read_ch(0) == 0) {
            k_mutex_lock(&buffer_lock, K_FOREVER);

            int32_t v_adc = (sample_buffer * ADC_FULL_SCALE_MV) / ADC_RESOLUTION;
            printk("convert voltage AIN0: %d mV\n", v_adc);
            
            ring_buffer[ring_head] = v_adc;
            ring_head = (ring_head + 1) % ADC_BUFFER_SIZE;
            k_mutex_unlock(&buffer_lock);
            k_sem_give(&data_ready_sem);
        } else {
            printk("failed to read ADC sequence\n");
        }

        // check for a rate change signals
        if (k_sem_take(&rate_change_sem, K_NO_WAIT) == 0) {
            printk("sampling rate updated to %d ms\n", sampling_rate_ms);
            k_sem_reset(&rate_change_sem);  // reset semaphore count
        }
        k_sleep(K_MSEC(sampling_rate_ms));
    }
}

//  ========== app_adc_sampling_start ======================================================
// start the ADC sampling thread
// the thread reads data from the ADC and stores it in a ring buffer
void app_adc_sampling_start(void)
{
    stop_sampling = false;
    k_thread_create(&adc_thread_data, adc_stack, K_THREAD_STACK_SIZEOF(adc_stack),
                    app_adc_thread, NULL, NULL, NULL,
                    PRIORITY_ADC, 0, K_NO_WAIT); // priority 2 (higher than LTA)
}

//  ========== app_adc_sampling_stop =======================================================
// stop ADC sampling thread
void app_adc_sampling_stop(void)
{
    stop_sampling = true;
    k_sem_give(&rate_change_sem); // wake if sleeping
    k_thread_join(&adc_thread_data, K_FOREVER);
}

//  ========== app_adc_get_buffer ==========================================================
// copie a portion of the ADC ring buffer to a user-supplied buffer.
// use a mutex to ensure thread-safe access
void app_adc_get_buffer(uint16_t *dest, size_t size, int32_t offset)
{
    if (!dest || size == 0 || size > ADC_BUFFER_SIZE) {
        printk("adc_get_buffer: invalid params (size=%zu)\n", size);
        return;
    }

    // handle negative offsets by wrapping them around the buffer
    int start_index = (ring_head + offset + ADC_BUFFER_SIZE) % ADC_BUFFER_SIZE;

    k_mutex_lock(&buffer_lock, K_FOREVER);

    for (size_t i = 0; i < size; i++) {
        dest[i] = ring_buffer[(start_index + i) % ADC_BUFFER_SIZE];
        // printk("dest[%zu] = ring_buffer[%d] = %d\n", i, (start_index + i) % ADC_BUFFER_SIZE, dest[i]);
    }
    k_mutex_unlock(&buffer_lock);
}

//  ========== app_adc_set_sampling_rate ===================================================
void app_adc_set_sampling_rate(uint32_t rate_ms)
{
    sampling_rate_ms = rate_ms;
    k_sem_give(&rate_change_sem);
    // signal the thread about the rate change
    k_sem_give(&rate_change_sem);
    printk("sampling rate set to %d ms\n", rate_ms);
}
