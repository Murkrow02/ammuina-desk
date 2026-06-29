#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>

#define BUFFER_SIZE 30


// Trasporto telemetria selezionabile a runtime (fase 04). Vive qui (shared) cosi'
// e' usabile sia da net/ sia dalla config runtime (config/) senza dipendenze cicliche.
typedef enum {
    COMM_MODE_HTTP = 0,
    COMM_MODE_COAP = 1,
} communication_mode_t;


typedef struct {
    float lux;
    float noise_level;
    bool occupancy;
} sensor_data_t;


typedef struct {
    sensor_data_t samples[BUFFER_SIZE];
    int total_samples;
} network_payload_t;

#endif