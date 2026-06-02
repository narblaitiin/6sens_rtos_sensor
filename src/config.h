#ifndef USER_CONFIG_H
#define USER_CONFIG_H

#ifndef SASTRESS_LOG_LVL
#warning SASTRESS LOG LVL not defined, setting log level to DEBUG
#define SASTRESS_LOG_LVL LOG_LEVEL_DBG
#endif

#define LOG_LEVEL SASTRESS_LOG_LVL

#define BTH_PERIOD K_MINUTES(30)
#define PERIODIC_SAMPLE_PERIOD K_MINUTES(30)

// Configuration of the sensor functionalities
// BTH_ENABLE : if set to 0, the sensor won't send sensor status messages (battery, temperature, humidity) 
#define BTH_ENABLE 1
// PERIODIC_SAMPLE_ENABLE : if set to 0, the sensor won't send samples periodically
#define PERIODIC_SAMPLE_ENABLE 1
// PERIODIC_SAMPLE_ENABLE : if set to 0, the sensor won't send anomaly detected messages
#define ANOMALY_SEND 1
// ANOMALY_SEND_SAMPLES : if set to 0, the sensor won't send the samples linked to a detected anomaly
#define ANOMALY_SEND_SAMPLES 0

// The length of the signal to store in ms
#define ANOMALY_STORED_MS 5000

// The size of an anomaly in memory (TODO Remove hardcoded sampling period of 10ms)
#define STORED_ANOMALY_SIZE (ANOMALY_STORED_MS/10) 

// threshold above which we consider an event detected and :
// - Must be sent
#define SEND_RATIO 3.f
// - Must be stored
#define STORE_RATIO 4.5f
#endif