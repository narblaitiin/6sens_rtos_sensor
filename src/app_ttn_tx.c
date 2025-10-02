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

// buffer to hold the Short-Term Average (STA) and Long-Term Average (LTA) samples
static uint16_t sta_buffer[STA_WINDOW_SIZE];
static uint16_t lta_buffer[LTA_WINDOW_SIZE];

//  ========== app_lorawan_thread ==========================================================
// LoRaWAN thread function: handles ADC data acquisition and sends it over LoRaWAN
static void app_lorawan_thread(void *arg1, void *arg2, void *arg3) 
{
    printk("Lorawan thread started\n");
    
    // allocate a buffer for ADC data
    uint8_t payload[255];   // LoRaWAN payload buffer (max ~242 B for DR_5 EU868)
    int32_t collected = 0;

    while (1) {
        // sleep until the next send cycle
        k_sleep(K_MINUTES(3));

        // compute offset for last 1s or 10s block
        int32_t sta_offset = (ring_head - STA_WINDOW_SIZE + ADC_BUFFER_SIZE) % ADC_BUFFER_SIZE;
        //int32_t lta_offset = (ring_head - LTA_WINDOW_SIZE + ADC_BUFFER_SIZE) % ADC_BUFFER_SIZE;

        // retrieve the most recent data for the STA and LTA buffers
        app_adc_get_buffer(sta_buffer, STA_WINDOW_SIZE, sta_offset);
        //app_adc_get_buffer(lta_buffer, LTA_WINDOW_SIZE, lta_offset);

        // pack all samples (16-bit) into chunks that fit LoRaWAN
        int32_t total_samples = STA_WINDOW_SIZE;
        int32_t sample_index = 0;

        // debug the buffer
        printk("first 3 samples: %d %d %d\n",
            sta_buffer[0], sta_buffer[1], sta_buffer[2], lta_buffer[3]);

        // send in LoRaWAN-compatible chunks WITHOUT timestamp
        while (sample_index < total_samples) {  
            uint8_t unused, max_payload;
            lorawan_get_payload_sizes(&unused, &max_payload);

            int32_t max_samples_per_chunk = max_payload / 2; // only samples, 2B each
            if (max_samples_per_chunk <= 0) {
                printk("error: LoRaWAN max payload too small\n");
                break;
            }

            int32_t chunk_samples = MIN(max_samples_per_chunk, total_samples - sample_index);

            for (int32_t i = 0; i < chunk_samples; i++) {
                payload[2*i] = (lta_buffer[sample_index + i] >> 8) & 0xFF;
                payload[2*i + 1] = lta_buffer[sample_index + i] & 0xFF;
            }

            int32_t payload_len = chunk_samples * 2;
            int8_t ret = lorawan_send(LORAWAN_PORT, payload, payload_len,
            LORAWAN_MSG_UNCONFIRMED);
            if (ret < 0) {
                printk("LoRaWAN send failed: %d\n", ret);
                break;
            } else {
                printk("ADC data sent!\n");
            }

            sample_index += chunk_samples;
            k_sleep(K_MSEC(200)); // duty-cycle protection
        }
    }
}

//  ========== app_lorawan_start ===========================================================
// function to initialize and start the LoRaWAN transmission thread
int app_lorawan_start_tx(void)
{
    // create the LoRaWAN thread with the defined stack and function
    k_thread_create(&lorawan_thread_data, lorawan_stack, K_THREAD_STACK_SIZEOF(lorawan_stack),
                    app_lorawan_thread, NULL, NULL, NULL, PRIORITY_LTA, 0, K_NO_WAIT);
}