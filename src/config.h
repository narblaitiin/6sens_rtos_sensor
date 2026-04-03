#ifndef USER_CONFIG_H
#define USER_CONFIG_H


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
#define ANOMALY_SEND_SAMPLES 1


#endif