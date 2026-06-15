// LibraryDeskSense — firmware entrypoint
// Fase 00: blink + log periodico. Le fasi successive aggiungono task e component.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "main";

// LED onboard (regolare per la board reale; in Wokwi spesso GPIO2).
#define LED_GPIO GPIO_NUM_2

void app_main(void)
{
    ESP_LOGI(TAG, "LibraryDeskSense boot — phase 00 skeleton");

    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    bool on = false;
    while (true) {
        on = !on;
        gpio_set_level(LED_GPIO, on);
        ESP_LOGI(TAG, "alive, led=%d", on);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
