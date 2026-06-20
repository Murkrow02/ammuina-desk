#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>

#define BUFFER_SIZE 30


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