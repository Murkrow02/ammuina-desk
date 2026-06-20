// LibraryDeskSense — firmware entrypoint
// Fase 00: blink + log periodico. Le fasi successive aggiungono task e component.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sensors.h"

static const char *TAG = "main";

#define SAMPLING_RATE_MS 1000

static void sensing_task(void *arg)
{
    for (;;) {
        float light = sensors_read_light();
        float noise = sensors_read_noise();
        bool  occ   = sensors_read_occupancy();
        ESP_LOGI(TAG, "light=%.0f lux  noise=%.2f  occupancy=%d",
                light, noise, occ);
        vTaskDelay(pdMS_TO_TICKS(SAMPLING_RATE_MS));
    }
}


void app_main(void)
{
    ESP_LOGI(TAG, "AmmuinaDesk BOOTING...");


    // Init sensors and start sensing
    sensors_init();
    xTaskCreate(sensing_task, "sensing", 4096, NULL, 5, NULL);    
}

