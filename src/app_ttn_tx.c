/*
 * Copyright (c) 2025
 * Regis Rousseau
 * Univ Lyon, INSA Lyon, Inria, CITI, EA3720
 * SPDX-License-Identifier: Apache-2.0
 */

//  ========== includes ====================================================================
#include "app_lorawan.h"
#include "app_adc.h"

//  ========== globals =====================================================================
// define a stack for the LoRaWAN thread with a size of 1024 bytes
K_THREAD_STACK_DEFINE(lorawan_stack, 4096);

// declare a thread structure to manage the LoRaWAN thread's data
struct k_thread lorawan_thread_data;

// buffer to hold the Long-Term Average (LTA) samples (1000)
static uint16_t lta_buffer[LTA_WINDOW_SIZE];

//  ========== app_lorawan_thread ==========================================================
// LoRaWAN thread function: handles ADC data acquisition and sends it over LoRaWAN
static void app_lorawan_thread(void *arg1, void *arg2, void *arg3) 
{
    printk("Lorawan thread started\n");
    
    // allocate a buffer for ADC data
    uint8_t payload[255];   // LoRaWAN payload buffer (max ~242 B for DR_5 EU868)
    static int32_t sample_index = 0;

    while (1) {
        // compute offset for last 1s or 10s block
        int32_t lta_offset = (ring_head - LTA_WINDOW_SIZE + ADC_BUFFER_SIZE) % ADC_BUFFER_SIZE;

        // retrieve the most recent data for the STA and LTA buffers
        app_adc_get_buffer(lta_buffer, LTA_WINDOW_SIZE, lta_offset);

        // pack all samples (16-bit) into chunks that fit LoRaWAN
        int32_t total_samples = LTA_WINDOW_SIZE;
        int32_t sample_index = 0;

        // send in LoRaWAN-compatible chunks WITHOUT timestamp
        uint8_t unused, max_payload;
        lorawan_get_payload_sizes(&unused, &max_payload);

        int32_t max_samples_per_chunk = max_payload / 2; // only samples, 2 bytes each
        if (max_samples_per_chunk <= 0) {
            printk("error: LoRaWAN max payload too small\n");
            k_sleep(K_SECONDS(10));
            continue;
        }

        // compute chunk size
        int32_t remaining_samples = LTA_WINDOW_SIZE - sample_index;
        int32_t chunk_samples = MIN(max_samples_per_chunk, remaining_samples);

        for (int32_t i = 0; i < chunk_samples; i++) {
            payload[2*i] = (lta_buffer[sample_index + i] >> 8) & 0xFF;
            payload[2*i + 1] = lta_buffer[sample_index + i] & 0xFF;
        }

        int32_t payload_len = chunk_samples * 2;
        int8_t ret = lorawan_send(LORAWAN_PORT, payload, payload_len,
            LORAWAN_MSG_UNCONFIRMED);
        if (ret < 0) {
            printk("LoRaWAN send failed: %d\n", ret);
        } else {
            printk("sent %d samples (index %d/%d)\n", chunk_samples, sample_index, LTA_WINDOW_SIZE);
        }
        sample_index += chunk_samples;
        if (sample_index >= LTA_WINDOW_SIZE) {
            sample_index = 0; // all samples sent, start next batch after 3 min
            k_sleep(K_MINUTES(2));
        } else {
            k_sleep(K_SECONDS(10)); // wait to respect duty cycle
        }
    }
}

// ======================= optimized LoRaWAN streaming thread ==========================
// static void app_lorawan_thread(void *arg1, void *arg2, void *arg3)
// {
//     printk("Lorawan thread started (optimized streaming)\n");

//     static int32_t sample_index = 0;      // next sample to send
//     uint8_t payload[255];
//     bool use_delta_compression = true;    // enable optional delta compression

//     int16_t last_value = 0;               // for delta compression

//     while (1) {
//         // get latest LTA samples
//         int32_t lta_offset = (ring_head - LTA_WINDOW_SIZE + ADC_BUFFER_SIZE) % ADC_BUFFER_SIZE;
//         app_adc_get_buffer(lta_buffer, LTA_WINDOW_SIZE, lta_offset);

//         // determine max samples per packet
//         uint8_t unused, max_payload;
//         lorawan_get_payload_sizes(&unused, &max_payload);

//         int32_t max_samples_per_chunk = max_payload / (use_delta_compression ? 1 : 2);
//         if (max_samples_per_chunk <= 0) {
//             printk("LoRaWAN payload too small\n");
//             k_sleep(K_SECONDS(10));
//             continue;
//         }

//         // number of samples left to send
//         int32_t remaining_samples = LTA_WINDOW_SIZE - sample_index;
//         int32_t chunk_samples = MIN(max_samples_per_chunk, remaining_samples);

//         // pack chunk
//         if (use_delta_compression) {
//             for (int i = 0; i < chunk_samples; i++) {
//                 int16_t delta = lta_buffer[sample_index + i] - last_value;
//                 payload[i] = (uint8_t)(delta & 0xFF); // 1 byte delta
//                 last_value = lta_buffer[sample_index + i];
//             }
//         } else {
//             for (int i = 0; i < chunk_samples; i++) {
//                 payload[2*i] = (lta_buffer[sample_index + i] >> 8) & 0xFF;
//                 payload[2*i + 1] = lta_buffer[sample_index + i] & 0xFF;
//             }
//         }

//         int32_t payload_len = use_delta_compression ? chunk_samples : chunk_samples*2;

//         // try sending with retry
//         int8_t ret = lorawan_send(LORAWAN_PORT, payload, payload_len, LORAWAN_MSG_UNCONFIRMED);
//         if (ret < 0) {
//             printk("Send failed (retry in 5s): %d\n", ret);
//             k_sleep(K_SECONDS(5));
//             continue;  // retry same chunk
//         } else {
//             printk("Sent %d samples (index %d/%d)\n", chunk_samples, sample_index, LTA_WINDOW_SIZE);
//         }

//         sample_index += chunk_samples;

//         if (sample_index >= LTA_WINDOW_SIZE) {
//             // all samples sent, wait 3 minutes before next batch
//             sample_index = 0;
//             last_value = 0;  // reset for delta compression
//             k_sleep(K_MINUTES(3));
//         } else {
//             // spacing between chunks to respect duty cycle
//             k_sleep(K_SECONDS(10));
//         }
//     }
// }

//  ========== app_lorawan_start ===========================================================
// function to initialize and start the LoRaWAN transmission thread
int app_lorawan_start_tx(void)
{
    // create the LoRaWAN thread with the defined stack and function
    k_thread_create(&lorawan_thread_data, lorawan_stack, K_THREAD_STACK_SIZEOF(lorawan_stack),
                    app_lorawan_thread, NULL, NULL, NULL, PRIORITY_LTA, 0, K_NO_WAIT);
}