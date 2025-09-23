/*
 * Copyright (c) 2025
 * Regis Rousseau
 * Univ Lyon, INSA Lyon, Inria, CITI, EA3720
 * SPDX-License-Identifier: Apache-2.0
 */

 //  ========== includes ===================================================================
#include "app_sta_lta.h"
#include "app_lorawan.h"

//  ========== globals =====================================================================
K_THREAD_STACK_DEFINE(sta_lta_stack, 4096);

// declare a thread data structure to manage the STA/LTA thread
struct k_thread sta_lta_thread_data;

// buffer to hold the Short-Term Average (STA) and Long-Term Average (LTA) samples
static uint16_t sta_buffer[STA_WINDOW_SIZE];
static uint16_t lta_buffer[LTA_WINDOW_SIZE];

//  ========== calculate_sta ===============================================================
// function to calculate the Short-Term Average (STA) of a given buffer
static float calculate_sta(const uint16_t *buffer, size_t size)
{
    float sum = 0.0;
    for (size_t i = 0; i < size; i++) {
        sum += (float)buffer[i];
    }
    //printk("STA sum: %.2f\n", sum/size);
    return sum / size;
}

//  ========== calculate_lta ===============================================================
// function to calculate the Long-Term Average (LTA) of a given buffer
static float calculate_lta(const uint16_t *buffer, size_t size)
{
    float sum = 0.0;
    for (size_t i = 0; i < size; i++) {
        sum += (float)buffer[i];
    }
    //printk("LTA sum: %.2f\n", sum/size);
    return sum / size;
}

//  ========== app_lta_thread ==============================================================
static void app_sta_lta_thread(void *arg1, void *arg2, void *arg3)
{
    printk("STA and LTA thread started\n");

    while (1) {
        // wait for a semaphore indicating that new ADC data is available
        k_sem_take(&data_ready_sem, K_FOREVER);

        int32_t sta_offset = (ring_head - STA_WINDOW_SIZE + ADC_BUFFER_SIZE) % ADC_BUFFER_SIZE;
        int32_t lta_offset = (ring_head - LTA_WINDOW_SIZE + ADC_BUFFER_SIZE) % ADC_BUFFER_SIZE;
        
        // retrieve the most recent data for the STA and LTA buffers
        app_adc_get_buffer(sta_buffer, STA_WINDOW_SIZE, sta_offset);
        app_adc_get_buffer(lta_buffer, LTA_WINDOW_SIZE, lta_offset);

        // debug the buffer
        // for (size_t i = 0; i < LTA_WINDOW_SIZE; i++) {
        //     printk("lta_buffer[%zu] = %d\n", i, sta_buffer[i]);
        // }

        // calculate the STA and LTA values
        float sta = calculate_sta(sta_buffer, STA_WINDOW_SIZE);
        float lta = calculate_lta(lta_buffer, LTA_WINDOW_SIZE);

        // check the ration of long and short window
        float ratio = sta/lta;
        printk("STA: %.2f, LTA: %.2f, ratio: %.2f\n", sta, lta, ratio);
    }
}

//  ========== app_lta_start ===============================================================
// create and initialize the thread with the specified stack and priority
void app_sta_lta_start(void)
{
    k_thread_create(&sta_lta_thread_data, sta_lta_stack, K_THREAD_STACK_SIZEOF(sta_lta_stack),
                    app_sta_lta_thread, NULL, NULL, NULL, PRIORITY_LTA , 0, K_NO_WAIT);
}