#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "sensor/soil_moisture.h"
#include "sensor/sw520.h"
#include "sensor/battery.h"
#include "driver/adc.h"
#include "communication/communication.h"
#include "lora/lora.h"

#define NODE_ID 2
#define MOISTURE_THRESHOLD 3276 // 80% of 4095
#define BATTERY_MIN_MV 3300
#define SLEEP_24H_US (24ULL * 3600 * 1000000ULL)

#define SOIL_ADC_CHANNEL ADC2_CHANNEL_9
#define TILT_PIN1 33
#define TILT_PIN2 15
#define TILT_PIN3 23
#define TILT_PIN4 22
#define BATTERY_ADC_CHANNEL ADC_CHANNEL_4

static const char* TAG = "NodeSensor";

static soil_moisture_sensor_t soil_moisture;
static bool is_synced = false;
static uint32_t last_sync_time = 0;
static uint16_t broadcast_count = 0;
static uint16_t sequence = 1;

static void read_sensors(Packet* pkt) {
    int adc_val;
    if (soil_moisture_read_raw(&soil_moisture, &adc_val) == ESP_OK) {
        pkt->soil_moisture = adc_val;
        ESP_LOGI(TAG, "Read soil moisture: %d", adc_val);
    } else {
        pkt->soil_moisture = 0;
        ESP_LOGE(TAG, "Failed to read soil moisture");
    }

    pkt->tilt_status = tilt_sensor_is_tilted() ? 1 : 0;
    ESP_LOGI(TAG, "Read tilt status: %d", pkt->tilt_status);

    if (battery_read(BATTERY_ADC_CHANNEL, &pkt->battery_level) == ESP_OK) {
        ESP_LOGI(TAG, "Read battery level: %d mV", pkt->battery_level);
    } else {
        pkt->battery_level = 0;
        ESP_LOGE(TAG, "Failed to read battery voltage");
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Node %d starting", NODE_ID);

    if (lora_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LoRa");
        return;
    }

    if (soil_moisture_init(&soil_moisture, SOIL_ADC_CHANNEL) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize soil moisture sensor");
        return;
    }
    tilt_sensor_init(TILT_PIN1, TILT_PIN2, TILT_PIN3, TILT_PIN4);

    esp_sleep_enable_timer_wakeup(SLEEP_24H_US);

    while (1) {
        Packet pkt = {
            .node_id = NODE_ID,
            .count = 0
        };
        read_sensors(&pkt);

        if (pkt.soil_moisture < MOISTURE_THRESHOLD) {
            ESP_LOGI(TAG, "Soil dry (%d < %d), sending BT0", pkt.soil_moisture, MOISTURE_THRESHOLD);
            send_packet_with_ack(&pkt, 3);
            continue;
        } else {
            ESP_LOGI(TAG, "Soil wet (%d >= %d), sending BT0 and waiting for Sync", pkt.soil_moisture, MOISTURE_THRESHOLD);
            send_packet_with_ack(&pkt, 3);
            sync_with_master(&pkt, &is_synced, &broadcast_count, &last_sync_time);

            while (is_synced && broadcast_count < MAX_BROADCAST_COUNT) {
                Packet pkt = {
                    .node_id = NODE_ID,
                    .sequence = sequence++,
                    .count = ++broadcast_count
                };
                read_sensors(&pkt);
                send_packet_with_ack(&pkt, 3);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            if (broadcast_count >= MAX_BROADCAST_COUNT) {
                is_synced = false;
                broadcast_count = 0;
                ESP_LOGI(TAG, "Completed 100 BT1 transmissions, waiting for Sync again");
            }
        }

        lora_sleep();
        esp_deep_sleep_start();
    }
}