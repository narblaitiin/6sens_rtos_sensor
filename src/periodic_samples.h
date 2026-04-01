#ifndef PERIODIC_SAMPLES_H
#define PERIODIC_SAMPLES_H

#include <stdint.h>
#include "data_types.h"
// Get statistics over a given window
struct periodic_sample_payload_t get_statistics(const uint16_t *buffer, int size);
void start_periodic_sample(void);

#endif