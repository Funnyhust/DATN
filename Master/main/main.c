#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lora/lora.h"
#include "lcd/lcd.h"

#define SYNC_ID 0xFF
#define MAX_SLAVES 3
#define MAX_ROUNDS 100
#define SYNC_INTERVAL_MS 5000
#define BATTERY_MIN_MV 1000
#define BATTERY_MAX_MV 2000
#define LCD_ADDR 0x27
#define LCD_COLS 16
#define LCD_ROWS 4
#define I2C_PORT I2C_NUM_0
#define SDA_PIN 46
#define SCL_PIN 2

static const char* TAG = "Master";

typedef struct {
    uint8_t node_id;
    uint16_t sequence;
    uint16_t soil_moisture;
    uint8_t tilt_status;
    uint16_t battery_level;
    uint16_t count;
} Packet;

typedef struct {
    uint8_t sync_id;
    uint8_t count_Sync;
    uint16_t count_Round;
    uint32_t timestamp;
} SyncPacket;

typedef struct {
    uint8_t node_id;
    bool active;
    uint16_t last_soil_moisture;
    uint8_t last_tilt_status;
    uint16_t last_battery_level;
    uint16_t last_count;
    uint32_t last_seen;
    uint8_t missed_rounds;
} SlaveStatus;

static SlaveStatus slaves[MAX_SLAVES] = {0};
static uint16_t current_round = 0;
static uint8_t warning_level = 0;
static esp32_lcd_i2c_t lcd;

static void send_ack(uint8_t node_id) {
    lora_send_packet(&node_id, 1);
    ESP_LOGI(TAG, "Sent ACK to node %d", node_id);
}

static void send_sync(uint8_t count_Sync) {
    SyncPacket sync = {
        .sync_id = SYNC_ID,
        .count_Sync = count_Sync,
        .count_Round = current_round,
        .timestamp = esp_timer_get_time() / 1000000
    };
    lora_send_packet((uint8_t*)&sync, sizeof(sync));
    ESP_LOGI(TAG, "Sent Sync: round=%d, count_Sync=%d, timestamp=%" PRIu32,
             sync.count_Round, sync.count_Sync, sync.timestamp);
}

static void communication_task(void *pvParameters) {
    lora_init();
    lora_set_frequency(433E6);
    lora_set_spreading_factor(10);
    lora_set_bandwidth(125E3);
    lora_set_coding_rate(5);
    lora_enable_crc();
    lora_set_tx_power(17);

    while (1) {
        if (lora_received()) {
            Packet pkt;
            int len = lora_receive_packet((uint8_t*)&pkt, sizeof(pkt));
            if (len == sizeof(pkt) && pkt.node_id > 0 && pkt.node_id <= MAX_SLAVES) {
                ESP_LOGI(TAG, "Received from node %d: sequence=%d, soil=%d, tilt=%d, battery=%d, count=%d",
                         pkt.node_id, pkt.sequence, pkt.soil_moisture, pkt.tilt_status, pkt.battery_level, pkt.count);

                slaves[pkt.node_id - 1].node_id = pkt.node_id;
                slaves[pkt.node_id - 1].active = true;
                slaves[pkt.node_id - 1].last_soil_moisture = pkt.soil_moisture;
                slaves[pkt.node_id - 1].last_tilt_status = pkt.tilt_status;
                slaves[pkt.node_id - 1].last_battery_level = pkt.battery_level;
                slaves[pkt.node_id - 1].last_count = pkt.count;
                slaves[pkt.node_id - 1].last_seen = esp_timer_get_time() / 1000000;
                slaves[pkt.node_id - 1].missed_rounds = 0;

                if (pkt.soil_moisture < 3276) {
                    vTaskDelay(pdMS_TO_TICKS(20000));
                    send_ack(pkt.node_id);
                } else {
                    vTaskDelay(pdMS_TO_TICKS(20000));
                    for (int i = 1; i <= 3; i++) {
                        send_sync(i);
                        vTaskDelay(pdMS_TO_TICKS(SYNC_INTERVAL_MS));
                    }
                }
            } else {
                ESP_LOGW(TAG, "Invalid packet received: len=%d, node_id=%d", len, pkt.node_id);
            }
        }

        if (current_round >= MAX_ROUNDS) {
            ESP_LOGI(TAG, "Reached max rounds (%d), sending Sync for reset", MAX_ROUNDS);
            for (int i = 1; i <= 3; i++) {
                send_sync(i);
                vTaskDelay(pdMS_TO_TICKS(SYNC_INTERVAL_MS));
            }
            current_round = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void data_processing_task(void *pvParameters) {
    while (1) {
        int warning_count = 0;
        for (int i = 0; i < MAX_SLAVES; i++) {
            if (slaves[i].active) {
                float soil_moisture_percent = ((4095.0 - slaves[i].last_soil_moisture) / 4095.0) * 100.0;
                float battery_percent = ((slaves[i].last_battery_level - BATTERY_MIN_MV) / (float)(BATTERY_MAX_MV - BATTERY_MIN_MV)) * 100.0;
                if (battery_percent < 0) battery_percent = 0;
                if (battery_percent > 100) battery_percent = 100;

                ESP_LOGI(TAG, "Node %d: Soil=%.2f%%, Tilt=%d, Battery=%.2f%%",
                         slaves[i].node_id, soil_moisture_percent, slaves[i].last_tilt_status, battery_percent);

                uint32_t now_s = esp_timer_get_time() / 1000000;
                if (now_s - slaves[i].last_seen > 3 * 60) {
                    slaves[i].missed_rounds++;
                    if (slaves[i].missed_rounds >= 3) {
                        slaves[i].active = false;
                        ESP_LOGW(TAG, "Node %d lost connection", slaves[i].node_id);
                    }
                }

                if ((slaves[i].last_tilt_status == 1 || !slaves[i].active) && battery_percent > 30) {
                    warning_count++;
                }
            }
        }

        warning_level = (warning_count >= 3) ? 3 : (warning_count >= 2) ? 2 : (warning_count == 1) ? 1 : 0;
        if (warning_level > 0) {
            ESP_LOGW(TAG, "Warning level %d: %d node(s) with issues", warning_level, warning_count);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void lcd_display_task(void *pvParameters) {
    char lcd_buffer[17]; // 15 chars + null to avoid truncation

    while (1) {
        esp32_lcd_i2c_clear(&lcd); // Clear screen to avoid artifacts

        // Line 1: Humi
        snprintf(lcd_buffer, sizeof(lcd_buffer), "Humi:%3.0f %3.0f %3.0f",
                 slaves[0].active ? ((4095.0 - slaves[0].last_soil_moisture) / 4095.0) * 100.0 : 0,
                 slaves[1].active ? ((4095.0 - slaves[1].last_soil_moisture) / 4095.0) * 100.0 : 0,
                 slaves[2].active ? ((4095.0 - slaves[2].last_soil_moisture) / 4095.0) * 100.0 : 0);
        esp32_lcd_i2c_set_cursor(&lcd, 0, 0);
        esp32_lcd_i2c_print(&lcd, lcd_buffer);

        // Line 2: Tilt
        snprintf(lcd_buffer, sizeof(lcd_buffer), "Tilt:%1d %1d %1d",
                 slaves[0].active ? slaves[0].last_tilt_status : 0,
                 slaves[1].active ? slaves[1].last_tilt_status : 0,
                 slaves[2].active ? slaves[2].last_tilt_status : 0);
        esp32_lcd_i2c_set_cursor(&lcd, 0, 1);
        esp32_lcd_i2c_print(&lcd, lcd_buffer);

        // Line 3: Status
        snprintf(lcd_buffer, sizeof(lcd_buffer), "Status:%1d %1d %1d",
                 slaves[0].active ? 0 : 1,
                 slaves[1].active ? 0 : 1,
                 slaves[2].active ? 0 : 1);
        esp32_lcd_i2c_set_cursor(&lcd, -4, 2);
        esp32_lcd_i2c_print(&lcd, lcd_buffer);

        // Line 4: Warning
        snprintf(lcd_buffer, sizeof(lcd_buffer), "Canh bao muc:%1d", warning_level);
        esp32_lcd_i2c_set_cursor(&lf(lcd_buffer), "Canh bao muc:%1d", warning_level);
        esp32_lcd_i2c_set_cursor(&lcd, -4, 3);
        esp32_lcd_i2c_print(&lcd, lcd_buffer);

        // Backlight control
        // if (warning_level > 0) {
        //     esp32_lcd_i2c_backlight_on(&lcd);
        // } else {
        //     esp32_lcd_i2c_backlight_off(&lcd);
        // }

        vTaskDelay(pdMS_TO_TICKS(1000)); // Update every 5s
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Master starting");

    // Initialize LCD
    esp_err_t ret = esp32_lcd_i2c_init(&lcd, LCD_ADDR, LCD_COLS, LCD_ROWS, I2C_PORT, SDA_PIN, SCL_PIN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LCD init failed: %s", esp_err_to_name(ret));
        return;
    }
    esp32_lcd_i2c_clear(&lcd);
    esp32_lcd_i2c_backlight_on(&lcd);
    esp32_lcd_i2c_print(&lcd, "Master Started");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp32_lcd_i2c_clear(&lcd);

    // Initialize slaves
    for (int i = 0; i < MAX_SLAVES; i++) {
        slaves[i].node_id = i + 1;
    }

    // Create tasks
    xTaskCreate(communication_task, "CommTask", 4096, NULL, 5, NULL);
    xTaskCreate(data_processing_task, "DataTask", 4096, NULL, 5, NULL);
    xTaskCreate(lcd_display_task, "LcdTask", 4096, NULL, 4, NULL);
}