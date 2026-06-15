// sensors.h — astrazione lettura sensori (backend sim vs HW reale).
// Fase 00: solo dichiarazioni stub. Implementazione reale in fase 01.
#pragma once

#include <stdbool.h>

// Inizializza il backend sensori (I2C/ADC/GPIO secondo la build).
void sensors_init(void);

// Luce ambientale in lux (BH1750 su HW / LDR ADC in sim).
float sensors_read_light(void);

// Livello di rumore relativo, 0..1 (MAX9814 ADC su HW / potenziometro in sim).
// Vincolo privacy: solo intensità derivata, mai audio grezzo.
float sensors_read_noise(void);

// Occupazione: true se la scrivania è in uso (PIR GPIO su HW / pulsante in sim).
bool sensors_read_occupancy(void);
