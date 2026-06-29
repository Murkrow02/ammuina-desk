// LibraryDeskSense — firmware entrypoint
// Fase 00: blink + log periodico. Le fasi successive aggiungono task e component.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sensors.h"
#include "net.h"
#include "config.h"
#include "types.h"

static const char *TAG = "main";

#define PRIORITY_SENSING 5

// Code FreeRTOS
QueueHandle_t sensor_queue;     // Coda dati singoli (Sensing -> Aggregatore)
QueueHandle_t network_tx_queue; // Coda di PUNTATORI (Aggregatore -> Rete)

#define SENSOR_QUEUE_LEN 5
#define NET_QUEUE_LEN    3 // Può contenere fino a 3 finestre intere in attesa


static void sensing_task(void *arg)
{
    while (1)
    {
        // Read data from sensors
        sensor_data_t data = {.lux = sensors_read_light(),
                              .noise_level = sensors_read_noise(),
                              .occupancy = sensors_read_occupancy()};

        // Send data to queue (blocking)
        xQueueSend(sensor_queue, &data, portMAX_DELAY);
        // if (status != pdPASS)
        // {
        //     // Gestisci l'errore se non usi portMAX_DELAY e la coda è piena
        // }
        // Periodo letto dalla config runtime: aggiornabile a caldo via MQTT.
        vTaskDelay(pdMS_TO_TICKS(config_get().sampling_rate_ms));
    }
}



static void aggregator_task(void *arg)
{
    // Buffer locale temporaneo sullo stack del task
    static network_payload_t local_window;
    int buffer_index = 0;

    ESP_LOGI(TAG, "Aggregator task avviato.");

    while (1)
    {
        sensor_data_t single_sample;

        // Aspetta il singolo campione dal task dei sensori
        if (xQueueReceive(sensor_queue, &single_sample, portMAX_DELAY) == pdPASS)
        {
            // Accumula il dato nella finestra locale
            local_window.samples[buffer_index] = single_sample;
            buffer_index++;

            // La finestra è piena! È ora di spedire
            if (buffer_index >= BUFFER_SIZE)
            {

                local_window.total_samples = BUFFER_SIZE;

                // Alloca memoria dinamica nella HEAP per "confezionare" il pacchetto
                network_payload_t *packet_to_send = malloc(sizeof(network_payload_t));

                if (packet_to_send != NULL)
                {

                    // Copia il contenuto della finestra locale nella memoria appena allocata
                    memcpy(packet_to_send, &local_window, sizeof(network_payload_t));

                    // Invia il PUNTATORE (l'indirizzo) alla coda di rete.
                    // Passiamo &packet_to_send (l'indirizzo del puntatore) perché FreeRTOS 
                    // copierà i 4 byte del puntatore dentro la coda.
                    BaseType_t status = xQueueSend(network_tx_queue, &packet_to_send, 0);

                    if (status == pdPASS) {
                        ESP_LOGI(TAG, "Finestra piena. Passato puntatore %p al task di rete.", packet_to_send);
                    } else {
                        ESP_LOGW(TAG, "Coda di rete piena! Pacchetto scartato per evitare blocchi.");
                        free(packet_to_send); // Evita memory leak se la coda rifiuta il pacchetto
                    }
                }
                else
                {
                    ESP_LOGE(TAG, "Errore Out-Of-Memory! Impossibile allocare la finestra.");
                }

                // Resetta l'indice per iniziare una nuova finestra immediatamente
                buffer_index = 0;
            }
        }
    }
}

static void network_output_task(void *arg)
{
    // Questa variabile conterrà l'indirizzo estratto dalla coda
    network_payload_t *received_packet = NULL;

    ESP_LOGI(TAG, "Network output task avviato.");

    while (1)
    {
        // Resta in attesa di un puntatore. 
        // FreeRTOS copia i 4 byte dell'indirizzo dentro la variabile 'received_packet'
        if (xQueueReceive(network_tx_queue, &received_packet, portMAX_DELAY) == pdPASS)
        {
            // Ora questo task è l'UNICO proprietario di questa memoria.
            ESP_LOGI(TAG, "Ricevuto pacchetto da inviare all'indirizzo: %p", received_packet);
            ESP_LOGI(TAG, "-> Primo campione nella finestra: Lux=%.1f", received_packet->samples[0].lux);

            // Invio telemetria al proxy sul trasporto attivo (HTTP o CoAP, fase 04):
            // operazione I/O bloccante, il task si addormenta lasciando la CPU libera.
            net_telemetry_send(received_packet);

            // Fase 05: valuta la finestra contro le soglie di config e pubblica
            // gli eventi rilevati via MQTT (desk occupied/released, high noise,
            // poor lighting).
            net_events_eval(received_packet);

            // IMPORTANTISSIMO: Abbiamo finito con i dati. Dobbiamo liberare la memoria
            // allocata dall'aggregatore, altrimenti l'ESP32 finirà la RAM in pochi minuti.
            free(received_packet);
            ESP_LOGI(TAG, "Memoria all'indirizzo %p liberata correttamente.", received_packet);
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "AmmuinaDesk BOOTING...");

    // 1. Crea la coda per i singoli dati (Passaggio per VALORE)
    sensor_queue = xQueueCreate(SENSOR_QUEUE_LEN, sizeof(sensor_data_t));
    
    // 2. Crea la coda per le finestre (Passaggio per RIFERIMENTO/PUNTATORE)
    // NOTA: Specifichiamo sizeof(network_payload_t *), cioè la dimensione di un puntatore (4 byte)
    network_tx_queue = xQueueCreate(NET_QUEUE_LEN, sizeof(network_payload_t *));

    if (sensor_queue == NULL || network_tx_queue == NULL) {
        ESP_LOGE(TAG, "Errore nella creazione delle code.");
        return;
    }

    // Inizializza i sensori hardware/simulati
    sensors_init();

    // Connetti il WiFi prima di avviare i task (bloccante, ritenta all'infinito).
    // net_init() inizializza anche la config runtime (config_init).
    net_init();

    // Avvia il client MQTT: pubblica eventi e riceve update di config a caldo.
    net_mqtt_start();

    // 3. Avvia i Task
    xTaskCreate(sensing_task, "sensing", 3072, NULL, 5, NULL);
    xTaskCreate(aggregator_task, "aggregator", 4096, NULL, 5, NULL);
    // Stack a 6144: la via CoAP (libcoap io_process + getaddrinfo) consuma piu'
    // dell'HTTP plain; margine per evitare overflow al cambio di trasporto.
    xTaskCreate(network_output_task, "network_io", 6144, NULL, 4, NULL);
}