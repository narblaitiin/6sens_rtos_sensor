/*
 * Copyright (c) 2025
 * Regis Rousseau
 * Univ Lyon, INSA Lyon, Inria, CITI, EA3720
 * SPDX-License-Identifier: Apache-2.0
 */

 //  ========== includes ===================================================================
#include "app_sta_lta_tx.h"
#include "app_lorawan.h"
#include "fs_utils.h"

//  ========== globals =====================================================================
K_THREAD_STACK_DEFINE(sta_lta_stack, 4096);
K_THREAD_STACK_DEFINE(lorawan_stack, 2048);
K_THREAD_STACK_DEFINE(storage_stack, 4096);

// declare thread data structure 
struct k_thread sta_lta_thread_data;
struct k_thread lorawan_thread_data;
struct k_thread storage_thread_data;

// buffer to hold the Short-Term Average (STA) and Long-Term Average (LTA) samples
static uint16_t sta_buffer[STA_WINDOW_SIZE];
static uint16_t lta_buffer[LTA_WINDOW_SIZE];

// message structure to pass detection results to LoRaWAN sender
typedef struct {
    float max_ampl;
    float ratio;
    int64_t timestamp_ms;
} lta_event_t;


// message structure for sample storage
typedef struct {
    int32_t head_snapshot;   // value of ring_head when the event was enqueued
} storage_event_t;

// message queues (4 slots each — tune as needed)
K_MSGQ_DEFINE(lorawan_msgq,  sizeof(lta_event_t),     4, 4);
K_MSGQ_DEFINE(storage_msgq,  sizeof(storage_event_t), 4, 4);

//  ========== calculate_sta ===============================================================
// function to calculate the Short-Term Average (STA) of a given buffer
static float calculate_sta(const uint16_t *buffer, size_t size)
{
    float sum = 0.0;
    for (size_t i = 0; i < size; i++) {
        sum += (float)buffer[i];
    }
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

//  ========== find_max_amplitude ==========================================================
static uint16_t find_max_amplitude(const uint16_t *buffer, size_t size)
{
    uint16_t max_val = 0;
    for (size_t i = 0; i < size; i++) {
        if (buffer[i] > max_val) {
            max_val = buffer[i];
        }
    }
    return max_val;
}

//  ======== float_to_q8_8
static uint16_t float_to_uint16(float val)
{
    // clamp to [0, 255.996] then scale by 256
    if (val < 0.0f)   val = 0.0f;
    if (val > 255.0f) val = 255.0f;
    return (uint16_t)(val * 256.0f);
}

// ========== NEW: LoRaWAN send thread ====================================================
static void app_lorawan_thread(void *arg1, void *arg2, void *arg3)
{
    printk("LoRaWAN thread started\n");

    lta_event_t event;
    uint8_t payload[12];

    while (1) {
        // Block until a detection event is enqueued
        k_msgq_get(&lorawan_msgq, &event, K_FOREVER);

        // retrieve the current timestamp from the RTC device
        uint64_t timestamp_ms = app_ds3231_get_time();

        uint16_t amp_enc   = float_to_uint16(event.max_ampl);
        uint16_t ratio_enc = float_to_uint16(event.ratio);

        // add timestamp to byte payload (big-endian)
        for (int8_t i = 0; i < 8; i++) {
            payload[i] = (timestamp_ms >> (56 - i * 8)) & 0xFF;
        }

        payload[8] = (amp_enc  >> 8) & 0xFF;
        payload[9] =  amp_enc         & 0xFF;
        payload[10] = (ratio_enc >> 8) & 0xFF;
        payload[11] =  ratio_enc       & 0xFF;

        printk("LoRaWAN TX → amp: %.2f  ratio: %.2f  ts: %lld ms\n",
               event.max_ampl, event.ratio, event.timestamp_ms);

        int8_t ret = lorawan_send(LORAWAN_PORT, payload, sizeof(payload), LORAWAN_MSG_UNCONFIRMED);
        if (ret < 0) {
            printk("lorawan_send failed: %d\n", ret);
            return ret == -EAGAIN ? 0 : ret;
        }
    }
}

// ========== app_storage_thread: rewritten to use app_adc_get_buffer ====================
static void app_storage_thread(void *arg1, void *arg2, void *arg3)
{
    printk("Storage thread started\n");

    struct fs_file_t file;
    fs_file_t_init(&file);

    static int file_index = 0;
    char file_path[32];
    snprintf(file_path, sizeof(file_path), "%s_%03d%s", FILE_PREFIX, file_index, FILE_EXT);

    int rc = fs_open(&file, file_path, FS_O_CREATE | FS_O_WRITE | FS_O_APPEND);
    if (rc < 0) {
        printk("file open failed. error: %d\n", rc);
        return;
    }

    size_t   current_file_size = fs_tell(&file);
    storage_event_t event;

    // Reuse STA window size — one window of samples written per ADC event
    static uint16_t storage_buf[STA_WINDOW_SIZE];

    while (1) {
        k_msgq_get(&storage_msgq, &event, K_FOREVER);

        // Compute offset: read the STA_WINDOW_SIZE samples ending at head_snapshot
        int32_t offset = (event.head_snapshot - STA_WINDOW_SIZE + ADC_BUFFER_SIZE)
                         % ADC_BUFFER_SIZE;

        // Use the same safe, mutex-protected function as the STA/LTA thread
        app_adc_get_buffer(storage_buf, STA_WINDOW_SIZE, offset);

        size_t byte_len = STA_WINDOW_SIZE * sizeof(uint16_t);
        rc = fs_write(&file, storage_buf, byte_len);
        if (rc < 0) {
            printk("flash write failed. error: %d\n", rc);
        } else {
            current_file_size += byte_len;
        }

        // Rotate file when max size is reached
        if (current_file_size >= MAX_FILE_SIZE) {
            fs_close(&file);
            file_index++;
            snprintf(file_path, sizeof(file_path), "%s_%03d%s",
                     FILE_PREFIX, file_index, FILE_EXT);
            fs_file_t_init(&file);
            rc = fs_open(&file, file_path, FS_O_CREATE | FS_O_WRITE | FS_O_APPEND);
            if (rc < 0) {
                printk("failed to open new file. error: %d\n", rc);
                return;
            }
            current_file_size = 0;
            printk("rotated to new file: %s\n", file_path);
        }
    }

    fs_close(&file);
}

//  ========== app_lta_thread ==============================================================
static void app_sta_lta_thread(void *arg1, void *arg2, void *arg3)
{
    printk("STA/LTA thread started\n");

    // Threshold above which we consider an event detected (tune for your sensor)
    const float DETECTION_RATIO = 3.0f;

    while (1) {
        k_sem_take(&data_ready_sem, K_FOREVER);

        int32_t sta_offset = (ring_head - STA_WINDOW_SIZE + ADC_BUFFER_SIZE) % ADC_BUFFER_SIZE;
        int32_t lta_offset = (ring_head - LTA_WINDOW_SIZE + ADC_BUFFER_SIZE) % ADC_BUFFER_SIZE;

        app_adc_get_buffer(sta_buffer, STA_WINDOW_SIZE, sta_offset);
        app_adc_get_buffer(lta_buffer, LTA_WINDOW_SIZE, lta_offset);

        float sta   = calculate_sta(sta_buffer, STA_WINDOW_SIZE);
        float lta   = calculate_lta(lta_buffer, LTA_WINDOW_SIZE);
        float ratio = (lta > 0.0f) ? (sta / lta) : 0.0f;   // guard divide-by-zero

        printk("STA: %.2f  LTA: %.2f  ratio: %.2f\n", sta, lta, ratio);

        // before DSP — snapshot ring_head and pass it to the storage thread
        storage_event_t s_evt = {
            .head_snapshot = ring_head,   // capture position before it advances
        };
        if (k_msgq_put(&storage_msgq, &s_evt, K_NO_WAIT) != 0) {
            printk("WARN: storage queue full, sample dropped\n");
        }

        // only send LoRaWAN when a seismic event is detected
        if (ratio >= DETECTION_RATIO) {
            uint16_t max_amp = find_max_amplitude(sta_buffer, STA_WINDOW_SIZE);

            lta_event_t l_evt = {
                .max_ampl = (float)max_amp,
                .ratio         = ratio,
                .timestamp_ms  = k_uptime_get(),
            };

            if (k_msgq_put(&lorawan_msgq, &l_evt, K_NO_WAIT) != 0) {
                printk("WARN: LoRaWAN queue full, event dropped\n");
            }

            printk("EVENT DETECTED → max_amp: %u  ratio: %.2f\n", max_amp, ratio);
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
                    PRIORITY_LTA + 1, 0, K_NO_WAIT);

    // sample storage thread (lowest priority — background write)
    k_thread_create(&storage_thread_data, storage_stack,
                    K_THREAD_STACK_SIZEOF(storage_stack),
                    app_storage_thread, NULL, NULL, NULL,
                    PRIORITY_LTA + 2, 0, K_NO_WAIT);
}