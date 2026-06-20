#include "sensors.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

#define LIGHT_CH        ADC_CHANNEL_6   // GPIO34
#define NOISE_CH        ADC_CHANNEL_7   // GPIO35
#define OCCUPANCY_GPIO  GPIO_NUM_14

static const char *TAG = "sensors";
static adc_oneshot_unit_handle_t s_adc1;



void sensors_init(void)
{
#if defined(CONFIG_SENSOR_BACKEND_SIM)

    // We use ADC1 to convert the analog signals from the simulated sensors (noise and light) to digital values
    adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&unit_cfg, &s_adc1);

    // How to interpret the raw ADC values: 12-bit resolution (0..4095) and 12 dB attenuation (full range ~0..3.3V)
    adc_oneshot_chan_cfg_t ch_cfg = {
        .atten = ADC_ATTEN_DB_12,          // range pieno ~0..3.3V
        .bitwidth = ADC_BITWIDTH_DEFAULT,  // 12 bit -> 0..4095
    };

    // Let the ADC know which channels we will use
    adc_oneshot_config_channel(s_adc1, NOISE_CH, &ch_cfg);
    adc_oneshot_config_channel(s_adc1, LIGHT_CH, &ch_cfg);

    // Configure the GPIO pin for the simulated occupancy sensor (PIR)
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << OCCUPANCY_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
    };
    gpio_config(&io);
    ESP_LOGI(TAG, "backend SIM pronto");
#else
    // backend HW: BH1750 I2C + MAX9814 ADC + PIR. TODO fase HW.
    ESP_LOGW(TAG, "backend HW non ancora implementato");
#endif
}

float sensors_read_light(void)
{
#if defined(CONFIG_SENSOR_BACKEND_SIM)
    int raw = 0;
    adc_oneshot_read(s_adc1, LIGHT_CH, &raw);
    return (1.0f - raw / 4095.0f) * 1000.0f;
#else
    return 0.0f;
#endif
}

float sensors_read_noise(void)
{
#if defined(CONFIG_SENSOR_BACKEND_SIM)
    int raw = 0;
    adc_oneshot_read(s_adc1, NOISE_CH, &raw);
    return raw / 4095.0f;               // 0..1
#else
    return 0.0f;
#endif
}

bool sensors_read_occupancy(void)
{
#if defined(CONFIG_SENSOR_BACKEND_SIM)
    return gpio_get_level(OCCUPANCY_GPIO) == 1;
#else
    return false;
#endif
}