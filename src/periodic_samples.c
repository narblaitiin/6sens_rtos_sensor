#include "periodic_samples.h"

#include <zephyr/kernel.h>
#include "config.h"
#include "app_sta_lta_tx.h"
#include "lorawan.h"

#include "config.h" // for log level
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(periodic_samples);

// Thread data structures
struct k_thread periodic_thread_data;
K_THREAD_STACK_DEFINE(periodic_thread_stack, 2048);

// Buffer used for computing statistics
uint16_t buffer[STA_WINDOW_SIZE];

struct periodic_sample_payload_t get_statistics(const uint16_t *buffer, int size)
{
    struct periodic_sample_payload_t p;
    int16_t max = 0;
    int16_t min = 5000;
    float sum = 0.0;
    if(size < 0) {
        return p;
    }
    for (int i = 0; i < size; i++) {
        int16_t x = (buffer[i]);
        if(x > max) {
            max = x;
        }
        if(x < min) {
            min = x;
        }
        sum += (float) x;
    }

    p.max = max;
    p.min = min;
    
    p.mean = sum / size;
    return p;
}

static void periodic_sample_app(void *arg1, void *arg2, void *arg3) {
    struct periodic_sample_payload_t p;
    app_adc_get_buffer(buffer, STA_WINDOW_SIZE, -STA_WINDOW_SIZE);
    p = get_statistics(buffer, STA_WINDOW_SIZE);
    lora_send_packet(PERIODIC_SAMPLE, (uint8_t *) &p, sizeof(p));
    k_sleep(PERIODIC_SAMPLE_PERIOD);
}

void start_periodic_sample(void)
{
    // original STA/LTA detection thread
    k_thread_create(&periodic_thread_data, periodic_thread_stack,
                    K_THREAD_STACK_SIZEOF(periodic_thread_stack),
                    periodic_sample_app, NULL, NULL, NULL,
                    5, 0, PERIODIC_SAMPLE_PERIOD);

}