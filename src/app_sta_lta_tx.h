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
#include "fs_utils.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/ring_buffer.h>

//  ========== prototypes ==================================================================
static void app_lta_thread(void *arg1, void *arg2, void *arg3);
void app_sta_lta_start_tx(void);

//  ========== defines =====================================================================
// priority of the different threads involved
#define PRIORITY_ADC                2
#define PRIORITY_STORAGE            3
#define PRIORITY_LTA                4

#endif /* APP_STA_LTA_TX_H */