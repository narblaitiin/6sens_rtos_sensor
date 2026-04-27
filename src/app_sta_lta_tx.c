/*
 * Copyright (c) 2025
 * Regis Rousseau
 * Univ Lyon, INSA Lyon, Inria, CITI, EA3720
 * SPDX-License-Identifier: Apache-2.0
 */

//  ========== includes ===================================================================
#include "app_sta_lta_tx.h"
#include "app_adc.h"
#include "app_ds3231.h"
#include "data_types.h"
#include "lorawan.h"
#include "fs_utils.h"
#include "config.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(stalta);

//  ========== globals =====================================================================
K_THREAD_STACK_DEFINE(sta_lta_stack, 4096);
K_THREAD_STACK_DEFINE(lorawan_stack, 2048);

// declare thread data structure
struct k_thread sta_lta_thread_data;
struct k_thread lorawan_thread_data;
struct k_thread lorawan_thread_data;

// buffer to hold the Short-Term Average (STA) and Long-Term Average (LTA) samples
static uint16_t sta_buffer[STA_WINDOW_SIZE];
static uint16_t lta_buffer[LTA_WINDOW_SIZE];

// buffer to hold a signal sample of 1 second, to send via LoRaWAN - TODO : Make it so we can handle multiple 1s samples at a time
static uint16_t send_buffer[STA_WINDOW_SIZE];

// Timer to check when the last LTA/STA ratio exceed the threshold
uint64_t last_anomaly_time;

// message structure to pass detection results to LoRaWAN sender
typedef struct
{
    uint64_t timestamp_ms;
    int16_t max_ampl;
    int16_t min_ampl;
    int16_t mean_ampl;
    float ratio;
} lta_event_t;

// message structure for sample storage
typedef struct
{
    int32_t head_snapshot; // value of ring_head when the event was enqueued
} storage_event_t;

// message queues (4 slots each — tune as needed)
K_MSGQ_DEFINE(lorawan_msgq, sizeof(lta_event_t), 4, 4);
K_MSGQ_DEFINE(storage_msgq, sizeof(storage_event_t), 4, 4);

//  ========== calculate_squared_avg ===============================================================
// function to calculate the average of a given buffer
float calculate_squared_avg(const uint16_t *buffer, size_t size)
{
    float sum = 0.0;
    for (size_t i = 0; i < size; i++)
    {
        float x = (float)(buffer[i] - 1770);
        sum += x * x;
    }
    return sum / size;
}

//  ========== calculate_avg ===============================================================
// function to calculate the average of a given buffer
float calculate_avg(const uint16_t *buffer, size_t size)
{
    float sum = 0.0;
    for (size_t i = 0; i < size; i++)
    {
        sum += (float)(buffer[i]);
    }
    return sum / size;
}

//  ========== find_max_amplitude ==========================================================
uint16_t find_max_amplitude(const uint16_t *buffer, size_t size)
{
    uint16_t max_val = 0;
    for (size_t i = 0; i < size; i++)
    {
        if (buffer[i] > max_val)
        {
            max_val = buffer[i];
        }
    }
    return max_val;
}

uint16_t find_min_amplitude(const uint16_t *buffer, size_t size)
{
    uint16_t max_val = 5000;
    for (size_t i = 0; i < size; i++)
    {
        if (buffer[i] < max_val)
        {
            max_val = buffer[i];
        }
    }
    return max_val;
}

//  ======== float_to_int16 ================================================================
int16_t float_to_int16(float val)
{
    if (val > 32767.0f)
        return 32767;
    if (val < -32768.0f)
        return -32768;
    return (int16_t)val;
}

// ========== NEW: LoRaWAN send thread ====================================================
void app_lorawan_thread(void *arg1, void *arg2, void *arg3)
{
    LOG_INF("LoRaWAN thread started");

    lta_event_t event;
    int ret;

    while (1)
    {
        // block until a detection event is enqueued
        k_msgq_get(&lorawan_msgq, &event, K_FOREVER);

        if (ANOMALY_SEND != 0)
        {
            struct anomaly_payload_t payload;
            payload.max = event.max_ampl;
            payload.min = event.min_ampl;
            payload.mean = event.mean_ampl;
            payload.stalta = float_to_int16(event.ratio * 100);
            lora_send_timestamp(ANOMALY, event.timestamp_ms, (uint8_t *)&payload, sizeof(struct anomaly_payload_t));
        }

        // If configuration is set ANOMALY_SEND_SAMPLES to 0, skip the part where we send samples,
        if (ANOMALY_SEND_SAMPLES == 0)
        {
            continue;
        }

        int sent = 0;
        int64_t elapsed_time = 0;
        int16_t *p = send_buffer;

        while (sent < STA_WINDOW_SIZE)
        {
            size_t nb_to_send = STA_WINDOW_SIZE - sent;
            // Ceil to MAX_SAMPLES
            if (nb_to_send > MAX_SAMPLES)
            {
                nb_to_send = MAX_SAMPLES;
            }
            ret = lora_send_timestamp(SAMPLES, event.timestamp_ms + elapsed_time, (uint8_t *)p, nb_to_send * 2);
            if (ret == 0)
            {
                p += nb_to_send;
                sent += nb_to_send;
                elapsed_time += nb_to_send * SAMPLING_RATE_MS;
                k_sleep(K_MINUTES(1));
            }
            else
            {
                LOG_WRN("Could not send signal with anomaly, retrying in 30s");
                k_msleep(30000);
            }
        }
    }
}

//  ========== app_lta_thread ==============================================================
void app_sta_lta_thread(void *arg1, void *arg2, void *arg3)
{
    LOG_INF("STA/LTA thread started");
    // threshold above which we consider an event detected
    const float DETECTION_RATIO = 3.f;

    last_anomaly_time = k_uptime_get() + 100000;
    while (1)
    {
        k_sem_take(&data_ready_sem, K_FOREVER);

        app_adc_get_buffer(sta_buffer, STA_WINDOW_SIZE, -STA_WINDOW_SIZE);
        app_adc_get_buffer(lta_buffer, LTA_WINDOW_SIZE, -LTA_WINDOW_SIZE);

        float sta = calculate_squared_avg(sta_buffer, STA_WINDOW_SIZE);
        float lta = calculate_squared_avg(lta_buffer, LTA_WINDOW_SIZE);
        float ratio = (lta > 0.0f) ? (sta / lta) : 0.0f; // guard divide-by-zero

        if (k_uptime_get() - last_anomaly_time < MINIMAL_DELAY_ANOMALY_MS)
        {
            // printk("Detected anomaly %lld ms ago, skipping\n", k_uptime_get() - last_anomaly_time);
            continue;
        }
        // only send LoRaWAN when a seismic event is detected
        if (ratio >= DETECTION_RATIO)
        {
            last_anomaly_time = k_uptime_get();
            uint64_t timestamp = app_get_timestamp();
            uint16_t max_amp = find_max_amplitude(sta_buffer, STA_WINDOW_SIZE);
            uint16_t min_amp = find_min_amplitude(sta_buffer, STA_WINDOW_SIZE);
            uint16_t mean = (uint16_t)calculate_avg(sta_buffer, STA_WINDOW_SIZE);

            lta_event_t l_evt = {
                .timestamp_ms = timestamp,
                .max_ampl = max_amp,
                .min_ampl = min_amp,
                .mean_ampl = mean,
                .ratio = ratio,
            };

            memcpy(send_buffer, sta_buffer, STA_WINDOW_SIZE * 2);

            if (k_msgq_put(&lorawan_msgq, &l_evt, K_NO_WAIT) != 0)
            {
                LOG_ERR("warning: LoRaWAN queue full, event dropped");
            }

            LOG_INF("event detected: max amplitude: %u, ratio: %.2f", max_amp, (double)ratio);
        }
    }
}

// ========== app_sta_lta_start ===========================================================
void app_sta_lta_start_tx(void)
{
    // original STA/LTA detection thread
    k_thread_create(&sta_lta_thread_data, sta_lta_stack,
                    K_THREAD_STACK_SIZEOF(sta_lta_stack),
                    app_sta_lta_thread, NULL, NULL, NULL,
                    PRIORITY_LTA, 0, K_NO_WAIT);

    // LoRaWAN sender thread (lower priority — network I/O can wait)
    k_thread_create(&lorawan_thread_data, lorawan_stack,
                    K_THREAD_STACK_SIZEOF(lorawan_stack),
                    app_lorawan_thread, NULL, NULL, NULL,
                    PRIORITY_TTN + 1, 0, K_NO_WAIT);
}