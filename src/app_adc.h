/*
 * Copyright (c) 2025
 * Regis Rousseau
 * Univ Lyon, INSA Lyon, Inria, CITI, EA3720
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_ADC_H
#define APP_ADC_H

//  ========== includes ====================================================================
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

//  ========== defines =====================================================================
// ADC characteristics (nRF52 SAADC)
#define ADC_REF_INTERNAL_MV         600     // 0.6 V internal reference
#define ADC_GAIN                    6       // using ADC_GAIN_1_6
#define ADC_RESOLUTION              4096    // 12-bit

// effective full-scale voltage in mV
#define ADC_FULL_SCALE_MV           ((ADC_REF_INTERNAL_MV * ADC_GAIN))  // 3600 mV

// voltage divider correction
#define DIVIDER_RATIO_NUM           4600    // (R4 + R5)
#define DIVIDER_RATIO_DEN           3600    // R5
#define DIVIDER_CORRECTION          ((DIVIDER_RATIO_NUM * 1000) / DIVIDER_RATIO_DEN) // ≈1280

// battery thresholds (in mV, actual battery voltage!)
#define BATTERY_MAX_VOLTAGE         2980
#define BATTERY_MIN_VOLTAGE         2270

// duration between 2 samples
#define SAMPLING_RATE_MS            10

// STA and LTA window durations in milliseconds
#define STA_WINDOW_DURATION_MS      1000     // 1 seconds
#define LTA_WINDOW_DURATION_MS      10000    // 10 seconds

// ADC buffer size in bytes
#define ADC_BUFFER_SIZE             (LTA_WINDOW_SIZE * 2) 

// derived buffer sizes
#define STA_WINDOW_SIZE (STA_WINDOW_DURATION_MS / SAMPLING_RATE_MS)
#define LTA_WINDOW_SIZE (LTA_WINDOW_DURATION_MS / SAMPLING_RATE_MS)

// trigger thresholds with hysteresis
#define TRIGGER_THRESHOLD           1.0f //0.8f    // 5.0f // STA/LTA ratio to trigger event
#define RESET_THRESHOLD             0.6f    // 2.5f // STA/LTA ratio to reset trigger

// priority of the different threads involved
#define PRIORITY_ADC                2
#define PRIORITY_LTA                3
#define PRIORITY_BTH                4

// ========== globals ======================================================================
extern struct k_sem data_ready_sem;
extern int32_t ring_head;


//  ========== prototypes ==================================================================
int8_t app_nrf52_adc_init();
int16_t app_nrf52_get_bat();

static void adc_thread(void *arg1, void *arg2, void *arg3);

void app_adc_sampling_start(void);
void app_adc_sampling_stop(void);

void app_adc_get_buffer(uint16_t *dest, size_t size, int32_t offset);
void app_adc_set_sampling_rate(uint32_t rate_ms);

#endif /* APP_ADC_H */