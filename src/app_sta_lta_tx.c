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
#include <zephyr/logging/log_ctrl.h>
LOG_MODULE_REGISTER(stalta);

//  ========== globals =====================================================================
K_THREAD_STACK_DEFINE(sta_lta_stack, 4096);
K_THREAD_STACK_DEFINE(lorawan_stack, 2048);
K_THREAD_STACK_DEFINE(storage_stack, 8000);
// declare thread data structure
struct k_thread sta_lta_thread_data;
struct k_thread lorawan_thread_data;
struct k_thread storage_thread_data;

// buffer to hold the Short-Term Average (STA) and Long-Term Average (LTA) samples
static uint16_t sta_buffer[STA_WINDOW_SIZE];
static uint16_t lta_buffer[LTA_WINDOW_SIZE];

// buffer to hold a signal sample of 1 second, to send via LoRaWAN - TODO : Make it so we can handle multiple 1s samples at a time
static uint16_t send_buffer[STA_WINDOW_SIZE];


// Timer to check when the last LTA/STA ratio exceed the threshold
uint64_t last_anomaly_time;

// Boolean to check if there is enough space for recording new signals
bool storing = true;

// message queues (4 slots each — tune as needed)
K_MSGQ_DEFINE(lorawan_msgq, sizeof(lta_event_t), 1, 4);
K_MSGQ_DEFINE(anomalies_to_store_msgq, sizeof(lta_event_t), 1, 4);

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
        int nb_retries = 0;
        int64_t elapsed_time = 0;
        int16_t *p = send_buffer;

        while (sent < STA_WINDOW_SIZE && nb_retries < NB_SEND_RETRIES)
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

void app_anomaly_store(void *arg1, void *arg2, void *arg3)
{
    LOG_INF("Anomaly storage thread started");

    lta_event_t event;
    stored_anomaly_t anomaly;
    int ret;
    struct fs_file_t file;
    fs_file_t_init(&file);

    char file_path[400];

    while(true) {
        // block until a detection event is enqueued
        k_msgq_get(&anomalies_to_store_msgq, &event, K_FOREVER);
        if(storing == false) {
            continue;
        }
        k_sleep(K_MSEC(TIME_BTW_DETECT_AND_STORE_MS));

        uint64_t timestamp = app_get_timestamp();
        uint64_t delta_detect = timestamp - event.timestamp_ms;

        // On veut centrer le signal sur le moment de la détection
        int overshoot = delta_detect - TIME_BTW_DETECT_AND_STORE_MS;

        if(overshoot < 0) {
            overshoot = 0;
            LOG_WRN("Overshoot < 0, we sleep less than expected ");
        }
        // We must store the signal from t_detect - 2.5s to t_detect +2.5s
        // Since we waited 2.5s since detection, we should store signal from -5s from now
        // However, we might have waited more than 2.5s, we must correct for that
        int start_offset_time = -ANOMALY_STORED_MS - overshoot;
        uint64_t start_time = timestamp + start_offset_time;
        int start_offset = start_offset_time / 10;
        printk("Overshoot %d - Starting at offset %d\n", overshoot, start_offset);
        printk("Start time is %lld\n", start_time);
        if(ADC_BUFFER_SIZE - start_offset < 0) {
            LOG_WRN("Cannot store the signal, offset is too important (%d ms)", start_offset_time);
            continue;
        }
        // Le moment où on a capturé l'événement, on prends 10s avant, et 10s après
        app_adc_get_buffer(anomaly.samples, STORED_ANOMALY_SIZE, start_offset);
        anomaly.event = event;
        anomaly.event.timestamp_ms = start_time;
        
        snprintf(file_path, sizeof(file_path), "/data/signal_%llu.dat", anomaly.event.timestamp_ms);
        
        ret = fs_open(&file, file_path, FS_O_CREATE | FS_O_WRITE);
        if (ret < 0) {
            LOG_ERR("file open failed. error: %d", ret);
            storing = false;
            continue;
        }
        
        
        // void * data = (void *) &anomaly;
        // for(int i = 0; i<)
        ret = fs_write(&file, &anomaly, sizeof(stored_anomaly_t));
        if (ret < 0) {
            LOG_ERR("file write failed. error: %d", ret);
            continue;
        }
        
        ret = fs_sync(&file);
        if (ret < 0) {
            LOG_ERR("file sync failed. error: %d", ret);
            continue;
        }
        fs_close(&file);
    }
}

//  ========== app_lta_thread ==============================================================
void app_sta_lta_thread(void *arg1, void *arg2, void *arg3)
{
    LOG_INF("STA/LTA thread started");
    char time_str[100];

    last_anomaly_time = k_uptime_get() + 100000;
    while (1)
    {
        k_sem_take(&data_ready_sem, K_FOREVER);

        app_adc_get_buffer(sta_buffer, STA_WINDOW_SIZE, -STA_WINDOW_SIZE);
        app_adc_get_buffer(lta_buffer, LTA_WINDOW_SIZE, -LTA_WINDOW_SIZE);

        float sta = calculate_squared_avg(sta_buffer, STA_WINDOW_SIZE);
        float lta = calculate_squared_avg(lta_buffer, LTA_WINDOW_SIZE);
        float ratio = (lta > 0.0f) ? (sta / lta) : 0.0f; // guard divide-by-zero

        // If an anomaly was detected MINIMAL_DELAY_ANOMALY_MS ms ago, this might be the same
        if (k_uptime_get() - last_anomaly_time < MINIMAL_DELAY_ANOMALY_MS)
        {
            continue;
        }
        // BUGFIX : If we sent a LoRa message less than 12s ago, this can create
        // voltage drops that get detected has an anomaly. 
        if(k_uptime_get() - get_last_msg_uptime() < 12000) {
            // LOG_INF("Expecting voltage drop, skipping ! %lld" , k_uptime_get() - get_last_msg_uptime());
            continue;
        }
        // only send LoRaWAN when a seismic event is detected
        if (ratio >= SEND_RATIO)
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
            

            timestamp_to_string(time_str, timestamp);
            LOG_INF("event detected at %s max amplitude: %u, ratio: %.2f", time_str, max_amp, (double)ratio);
            log_flush();


            // If the event has been detected but is not worth sending, continue to next detection.
            if (ratio < SEND_RATIO) {
                continue;
            }
            // TODO SUPPORT PARALLEL SYNCS
            memcpy(send_buffer, sta_buffer, STA_WINDOW_SIZE * 2);

            if (k_msgq_put(&lorawan_msgq, &l_evt, K_NO_WAIT) != 0)
            {
                LOG_ERR("LoRaWAN queue full, event dropped");
            }
            
            // If the event has been detected but is not worth recording
            if (ratio < STORE_RATIO) {
                continue;
            }

            if (k_msgq_put(&anomalies_to_store_msgq, &l_evt, K_NO_WAIT) != 0)
            {
                LOG_ERR("Anomaly storage queue full, event dropped");
            }
            
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
                    PRIORITY_TTN + 2, 0, K_NO_WAIT);

    // Storage thread (lower priority — Yet, must execute before send)
    k_thread_create(&storage_thread_data, storage_stack,
                    K_THREAD_STACK_SIZEOF(storage_stack),
                    app_anomaly_store, NULL, NULL, NULL,
                    PRIORITY_TTN + 1, 0, K_NO_WAIT);
}