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

// duration between 2 samples
#define SAMPLING_RATE_MS            10

// priority of the different threads involved
#define PRIORITY_ADC                2

// ========== globals ======================================================================
extern struct k_sem data_ready_sem;
extern int32_t ring_head;

//  ========== prototypes ==================================================================
int16_t app_adc_get_bat();
static int8_t app_adc_read_ch(size_t ch);

static void app_adc_thread(void *arg1, void *arg2, void *arg3);

void app_adc_sampling_start(void);
void app_adc_sampling_stop(void);

void app_adc_get_buffer(uint16_t *dest, size_t size, int32_t offset);
void app_adc_set_sampling_rate(uint32_t rate_ms);

#endif /* APP_ADC_H */