#ifndef DATA_TYPE_H
#define DATA_TYPE_H

// Number of samples we can send in the worst case scenario
// With a spreading factor of 12, our packet can at most be 64 bytes long. LoRa Header is 13 bytes long, so we have a payload of 51 bytes
// We substract the size of our header to get the useful payload in bytes. Each sample is 2 byte.
#define MAX_SAMPLES (51 - sizeof(PACKET_TYPE) - sizeof(uint64_t))/2
// TODO : should be based on Sampling Rate ! -> STORED_ANOMALY_SIZE = (STORED_ANOMALY_DURATION_MS / SAMPLING_RATE_MS)

#include <stdint.h>
#include "config.h"

typedef enum packet_type {
    BTH = 1,
    ANOMALY = 2,
    SAMPLES = 3,
    PERIODIC_SAMPLE = 4
} PACKET_TYPE;

struct bth_payload_t {
    int16_t battery;
    int16_t temperature;
    int16_t humidity;
};

struct anomaly_payload_t {
    int16_t min;
    int16_t max;
    int16_t stalta;
    int16_t mean;
};

struct periodic_sample_payload_t {
    int16_t min;
    int16_t max;
    int16_t mean;
};

struct samples_payload_t {
    int16_t samples[MAX_SAMPLES];
};


typedef struct packet_t {
    PACKET_TYPE type;
    uint64_t timestamp;
    uint8_t payload[100];
} __attribute__((packed)) PACKET;

// message structure to pass detection results to LoRaWAN sender
typedef struct {
    uint64_t timestamp_ms;
    int16_t max_ampl;
    int16_t min_ampl;
    int16_t mean_ampl;
    float ratio;
} lta_event_t;


typedef struct {
    lta_event_t event;
    int16_t samples[STORED_ANOMALY_SIZE];
} stored_anomaly_t;
#endif