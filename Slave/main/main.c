#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_random.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "lora/lora.h"
#include "sensor/soil_moisture.h"
#include "sensor/sw520.h"
#include "sensor/battery.h"
#include "driver/adc.h"

#define NODE_ID 1
#define SLOT_OFFSET_MS ((NODE_ID - 1) * 10000)
#define MOISTURE_THRESHOLD 3276 // Ngưỡng độ ẩm (80% của 4095)
#define BATTERY_MIN_MV 3300
#define SYNC_WINDOW_S 90
#define SYNC_MAX_INDEX 6 // 1-6 cho round 0, 1-3 cho round i
#define SYNC_ID 0xFF
#define MAX_BROADCAST_COUNT 100 // 100 lần gửi BT1
#define SLEEP_24H_MS (24 * 3600 * 1000) // 24 giờ (milliseconds)

#define SOIL_ADC_CHANNEL ADC2_CHANNEL_9 // GPIO 26 (ADC2_CH9)
#define TILT_PIN 15 // GPIO 15
#define BATTERY_ADC_CHANNEL ADC_CHANNEL_3 // GPIO 33 (ADC1_CH3)

static const char* TAG = "NodeSensor";

typedef struct {
    uint8_t node_id;
    uint16_t sequence;
    uint16_t soil_moisture;
    uint8_t tilt_status;
    uint16_t battery_level;
    uint16_t count; // Thêm lượt gửi (BT1)
} Packet;

typedef struct {
    uint8_t sync_id;
    uint8_t count_Sync;
    uint16_t count_Round;
    uint32_t timestamp;
} SyncPacket;

static soil_moisture_sensor_t soil_moisture;
static uint16_t sequence = 1;
static bool is_synced = false;
static uint32_t last_sync_time = 0;
static uint16_t broadcast_count = 0;
static adc_oneshot_unit_handle_t adc1_handle;

static void read_sensors(Packet* pkt) {
    int adc_val;
    if (soil_moisture_read_raw(&soil_moisture, &adc_val) == ESP_OK) {
        pkt->soil_moisture = adc_val;
        ESP_LOGI(TAG, "Read soil moisture (GPIO26/ADC2_CH9): %d", adc_val);
    } else {
        pkt->soil_moisture = 0;
        ESP_LOGE(TAG, "Failed to read soil moisture (GPIO26/ADC2_CH9)");
    }
    pkt->tilt_status = tilt_sensor_is_tilted() ? 1 : 0;
    ESP_LOGI(TAG, "Read tilt status (GPIO15): %d", pkt->tilt_status);

    int adc_raw;
    if (adc_oneshot_read(adc1_handle, BATTERY_ADC_CHANNEL, &adc_raw) == ESP_OK) {
        pkt->battery_level = (adc_raw * 3300) / 4095; // Chuyển ADC sang mV
        ESP_LOGI(TAG, "Read battery level (GPIO33/ADC1_CH3): %d mV", pkt->battery_level);
    } else {
        pkt->battery_level = 0;
        ESP_LOGE(TAG, "Failed to read battery voltage (GPIO33/ADC1_CH3)");
    }
}

static bool send_packet_with_ack(Packet* pkt, int retries) {
    for (int i = 0; i < retries; i++) {
        lora_send_packet((uint8_t*)pkt, sizeof(Packet));
        ESP_LOGI(TAG, "Sent packet: node_id=%d, sequence=%d, soil=%d, tilt=%d, battery=%d, count=%d",
                 pkt->node_id, pkt->sequence, pkt->soil_moisture, pkt->tilt_status, pkt->battery_level, pkt->count);
        uint64_t start_us = esp_timer_get_time();
        while ((esp_timer_get_time() - start_us) < 30000000ULL) { // Chờ 30s
            if (lora_received()) {
                uint8_t ack_buf[1];
                if (lora_receive_packet(ack_buf, sizeof(ack_buf)) == 1 && ack_buf[0] == NODE_ID) {
                    ESP_LOGI(TAG, "Received ACK for sequence %d from master", pkt->sequence);
                    return true;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(100)); // Tăng delay để giảm lỗi SPI
        }
        ESP_LOGW(TAG, "No ACK received for sequence %d, retry %d/%d", pkt->sequence, i + 1, retries);
        vTaskDelay(pdMS_TO_TICKS(1000 * (esp_random() % 10 + 1))); // Random 1-10s
    }
    ESP_LOGE(TAG, "Failed to send packet sequence %d after %d retries", pkt->sequence, retries);
    return false;
}

static void sync_with_master(void) {
    ESP_LOGI(TAG, "Waiting for Sync packet from master");

    esp_err_t ret = lora_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LoRa init failed: %s", esp_err_to_name(ret));
        vTaskDelay(pdMS_TO_TICKS(1000));
        return;
    }
    ESP_LOGI(TAG, "LoRa initialized successfully");

    lora_set_frequency(433E6);
    lora_set_spreading_factor(10);
    lora_set_bandwidth(125E3);
    lora_set_coding_rate(5);
    lora_enable_crc();
    lora_set_tx_power(17);

    uint64_t start_us = esp_timer_get_time();
    while ((esp_timer_get_time() - start_us) < SYNC_WINDOW_S * 1000000ULL) {
        if (lora_received()) {
            SyncPacket sync;
            int len = lora_receive_packet((uint8_t*)&sync, sizeof(sync));
            if (len == sizeof(sync) && sync.sync_id == SYNC_ID && sync.count_Sync >= 1 && sync.count_Sync <= SYNC_MAX_INDEX) {
                last_sync_time = sync.timestamp;
                is_synced = true;
                ESP_LOGI(TAG, "Received Sync: round=%d, count_Sync=%d, timestamp=%" PRIu32,
                         sync.count_Round, sync.count_Sync, sync.timestamp);

                uint32_t delay_s = 5 + (NODE_ID - 1) * 10 + 10 * (6 - sync.count_Sync);
                ESP_LOGI(TAG, "Waiting %" PRIu32 " seconds before sending BT1", delay_s);
                vTaskDelay(pdMS_TO_TICKS(delay_s * 1000));

                Packet pkt = {
                    .node_id = NODE_ID,
                    .sequence = sequence++,
                    .count = ++broadcast_count
                };
                read_sensors(&pkt);
                send_packet_with_ack(&pkt, 3);
                break;
            } else {
                ESP_LOGW(TAG, "Invalid Sync packet received: len=%d, sync_id=%d, count_Sync=%d",
                         len, sync.sync_id, sync.count_Sync);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); // Tăng delay để giảm lỗi SPI
    }
    ESP_LOGI(TAG, "Sync window closed");
}

void app_main(void) {
    ESP_LOGI(TAG, "Node %d starting with configuration: SOIL=GPIO26, TILT=GPIO15, BATTERY=GPIO33", NODE_ID);
    lora_init();
    // Khởi tạo ADC cho pin
    adc_oneshot_unit_init_cfg_t adc_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_cfg, &adc1_handle));
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, BATTERY_ADC_CHANNEL, &chan_cfg));

    // Khởi tạo cảm biến
    if (soil_moisture_init(&soil_moisture, SOIL_ADC_CHANNEL) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize soil moisture sensor (GPIO26/ADC2_CH9)");
        return;
    }
    tilt_sensor_init(TILT_PIN);
    if (battery_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize battery sensor (GPIO33/ADC1_CH3)");
    }

    while (1) {
        Packet pkt = {
            .node_id = NODE_ID,
            .sequence = sequence++,
            .count = 0 // BT0 không dùng count
        };
        read_sensors(&pkt);

        esp_err_t ret = lora_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "LoRa init failed: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        ESP_LOGI(TAG, "LoRa initialized for BT0");

        lora_set_frequency(433E6);
        lora_set_spreading_factor(10);
        lora_set_bandwidth(125E3);
        lora_set_coding_rate(5);
        lora_enable_crc();
        lora_set_tx_power(17);

        if (pkt.soil_moisture < MOISTURE_THRESHOLD) { // Đất khô
            ESP_LOGI(TAG, "Soil dry (%d < %d), sending BT0", pkt.soil_moisture, MOISTURE_THRESHOLD);
            if (send_packet_with_ack(&pkt, 3)) {
                ESP_LOGI(TAG, "Received ACK for BT0, waiting 24h before next cycle");
                vTaskDelay(pdMS_TO_TICKS(SLEEP_24H_MS)); // Đợi 24h thay vì ngủ sâu
            }
            continue;
        } else { // Đất ướt
            ESP_LOGI(TAG, "Soil wet (%d >= %d), sending BT0 and waiting for Sync", pkt.soil_moisture, MOISTURE_THRESHOLD);
            send_packet_with_ack(&pkt, 3); // Gửi 3 BT0
            sync_with_master(); // Chờ Sync

            while (is_synced && broadcast_count < MAX_BROADCAST_COUNT) {
                Packet pkt = {
                    .node_id = NODE_ID,
                    .sequence = sequence++,
                    .count = ++broadcast_count
                };
                read_sensors(&pkt);
                send_packet_with_ack(&pkt, 3);
                vTaskDelay(pdMS_TO_TICKS(1000)); // Gửi mỗi giây
            }

            if (broadcast_count >= MAX_BROADCAST_COUNT) {
                is_synced = false;
                broadcast_count = 0;
                ESP_LOGI(TAG, "Completed 100 BT1 transmissions, waiting for Sync again");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // Đợi 1s trước vòng lặp tiếp
    }
}