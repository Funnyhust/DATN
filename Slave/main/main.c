#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_random.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#include "lora/lora.h"
#include "sensor/soil_moisture.h"
#include "sensor/sw520.h"

#define NODE_ID 1
#define SLOT_OFFSET_MS ((NODE_ID - 1) * 10000)
#define CYCLE_PERIOD_MS 30000

#define SOIL_ADC_CHANNEL ADC_CHANNEL_0 // Sửa từ ADC1_CHANNEL_0
#define TILT_PIN 15
#define BATTERY_ADC_CHANNEL ADC_CHANNEL_3 // Sửa từ ADC1_CHANNEL_3
#define BATTERY_MIN_MV 3300
#define BATTERY_MAX_MV 4200

#define SYNC_WINDOW_S 90
#define SYNC_MAX_INDEX 10
#define SYNC_ID 0xFF

static const char* TAG = "NodeSensor";

typedef struct {
    uint8_t node_id;
    uint16_t sequence;
    uint16_t soil_moisture;
    uint8_t tilt_status;
    uint16_t battery_level;
} Packet;

typedef struct {
    uint8_t sync_id;
    uint8_t packet_index;
    uint32_t timestamp;
} SyncPacket;

static soil_moisture_sensor_t soil_moisture;
static uint16_t sequence = 0;
static bool is_synced = false;
static uint32_t last_sync_time = 0;

static esp_err_t read_battery(uint16_t* battery_mv) {
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc1_handle));
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, BATTERY_ADC_CHANNEL, &chan_cfg));

    int adc_val;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, BATTERY_ADC_CHANNEL, &adc_val));
    *battery_mv = (adc_val * 3300 / 4095) * 2;
    ESP_LOGI(TAG, "Battery ADC raw: %d, mV: %d", adc_val, *battery_mv);
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
    return ESP_OK;
}

static void read_sensors(Packet* pkt) {
    int adc_val;
    if (soil_moisture_read_raw(&soil_moisture, &adc_val) == ESP_OK) {
        pkt->soil_moisture = adc_val;
    } else {
        pkt->soil_moisture = 0;
        ESP_LOGE(TAG, "Không đọc được độ ẩm đất");
    }
    pkt->tilt_status = tilt_sensor_is_tilted() ? 1 : 0;
    if (read_battery(&pkt->battery_level) != ESP_OK) {
        pkt->battery_level = 0;
    }
}

static bool send_packet_with_ack(Packet* pkt, int retries) {
    for (int i = 0; i < retries; i++) {
        lora_send_packet((uint8_t*)pkt, sizeof(Packet));
        uint32_t start_tick = xTaskGetTickCount();
        while ((xTaskGetTickCount() - start_tick) * portTICK_PERIOD_MS < 1000) {
            if (lora_received()) {
                uint8_t ack_buf[1];
                if (lora_receive_packet(ack_buf, 1) == 1 && ack_buf[0] == NODE_ID) {
                    ESP_LOGI(TAG, "Nhận ACK gói %d", pkt->sequence);
                    return true;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        vTaskDelay(pdMS_TO_TICKS(esp_random() % 500));
    }
    ESP_LOGE(TAG, "Gửi gói %d thất bại", pkt->sequence);
    return false;
}

static void sync_time_from_master(void) {
    ESP_LOGI(TAG, "Bắt đầu chế độ đồng bộ thời gian");

    lora_init();
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
            if (len == sizeof(sync) && sync.sync_id == SYNC_ID && sync.packet_index >= 1 && sync.packet_index <= SYNC_MAX_INDEX) {
                last_sync_time = sync.timestamp;
                is_synced = true;

                uint32_t delay_ms = (SYNC_MAX_INDEX - sync.packet_index) * 10000 + SLOT_OFFSET_MS;
                ESP_LOGI(TAG, "Đồng bộ thời gian: %" PRIu32 ", gói sync %d, delay gửi: %" PRIu32 " ms", last_sync_time, sync.packet_index, delay_ms);

                vTaskDelay(pdMS_TO_TICKS(delay_ms));

                Packet pkt = {
                    .node_id = NODE_ID,
                    .sequence = sequence++,
                };
                read_sensors(&pkt);
                send_packet_with_ack(&pkt, 3);
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    lora_sleep();
}

void app_main(void) {
    ESP_LOGI(TAG, "Node %d khởi động", NODE_ID);

    if (soil_moisture_init(&soil_moisture, SOIL_ADC_CHANNEL) != ESP_OK) {
        ESP_LOGE(TAG, "Khởi tạo cảm biến độ ẩm đất thất bại");
        return;
    }

    tilt_sensor_init(TILT_PIN);

    esp_sleep_enable_timer_wakeup(1000000);

    while (1) {
        uint64_t now_s = esp_timer_get_time() / 1000000ULL;

        if (!is_synced || (now_s - last_sync_time >= 86400)) {
            sync_time_from_master();
            last_sync_time = now_s;
        }

        if (is_synced) {
            Packet pkt;
            read_sensors(&pkt);

            if (pkt.soil_moisture > 3276 || pkt.tilt_status == 1 || pkt.battery_level < BATTERY_MIN_MV) {
                pkt.node_id = NODE_ID;
                pkt.sequence = sequence++;
                send_packet_with_ack(&pkt, 3);
            }
        }

        lora_sleep();
        esp_light_sleep_start();
    }
}