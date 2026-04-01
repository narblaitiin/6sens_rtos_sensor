/*
 * Copyright (c) 2025
 * Regis Rousseau
 * Univ Lyon, INSA Lyon, Inria, CITI, EA3720
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_STA_LTA_TX__H
#define APP_STA_LTA_TX_H

//  ========== includes ====================================================================
#include "app_adc.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/ring_buffer.h>

//  ========== defines =====================================================================
// STA and LTA window durations in milliseconds
#define STA_WINDOW_DURATION_MS      1024     // 1 seconds
#define LTA_WINDOW_DURATION_MS      16384    // 10 seconds

// Minimal time between two anomalies
#define MINIMAL_DELAY_ANOMALY_MS 10000

// ADC buffer size in bytes
#define ADC_BUFFER_SIZE             (LTA_WINDOW_SIZE * 2) 

// derived buffer sizes
#define STA_WINDOW_SIZE (STA_WINDOW_DURATION_MS / SAMPLING_RATE_MS)
#define LTA_WINDOW_SIZE (LTA_WINDOW_DURATION_MS / SAMPLING_RATE_MS)

//  ========== prototypes ==================================================================
static void app_lta_thread(void *arg1, void *arg2, void *arg3);
void app_sta_lta_start_tx(void);

//  ========== defines =====================================================================
// priority of the different threads involved
#define PRIORITY_ADC                2
#define PRIORITY_STORAGE            3
#define PRIORITY_LTA                4

#endif /* APP_STA_LTA_TX_H */