#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lora/lora.h"
#include "lcd/lcd.h"

// Dinh nghia ID cua goi sync (0xFF de nhan dien goi sync tu master)
#define SYNC_ID 0xFF
// So luong toi da cac node cam bien (slave) trong he thong
#define MAX_SLAVES 3
// So luong toi da cac vong (round) truoc khi reset
#define MAX_ROUNDS 100
// Khoang thoi gian gui goi sync (5 giay = 5000 ms)
#define SYNC_INTERVAL_MS 5000
// Dinh nghia muc dien ap pin toi thieu va toi da (mV)
#define BATTERY_MIN_MV 1000
#define BATTERY_MAX_MV 2000
// Dia chi I2C cua LCD (0x27 thuong la dia chi mac dinh)
#define LCD_ADDR 0x27
// So cot va hang cua LCD (16 cot, 4 hang)
#define LCD_COLS 16
#define LCD_ROWS 4
// Port I2C su dung (I2C_NUM_0)
#define I2C_PORT I2C_NUM_0
// Chan SDA (GPIO 46) va SCL (GPIO 2) cho I2C (da xac nhan hoat dong)
#define SDA_PIN 46
#define SCL_PIN 2

// Tag de ghi log thong bao
static const char* TAG = "Master";

// Cau truc goi du lieu (10 byte) nhan tu node cam bien
typedef struct {
    uint8_t node_id;        // 1 byte: ID cua node (1, 2, 3)
    uint16_t sequence;      // 2 byte: So thu tu goi tin
    uint16_t soil_moisture; // 2 byte: Gia tri ADC do am dat (0-4095)
    uint8_t tilt_status;    // 1 byte: Trang thai nghieng (0 = khong nghieng, 1 = nghieng)
    uint16_t battery_level; // 2 byte: Dien ap pin (mV)
    uint16_t count;         // 2 byte: Bien dem (co the dung de kiem tra goi tin)
} Packet;

// Cau truc goi sync (6 byte) gui tu master
typedef struct {
    uint8_t sync_id;      // 1 byte: ID dong bo (0xFF)
    uint8_t count_Sync;   // 1 byte: So lan gui sync (1, 2, 3)
    uint16_t count_Round; // 2 byte: So vong hien tai
    uint32_t timestamp;   // 4 byte: Thoi gian (giay)
} SyncPacket;

// Cau truc luu thong tin trang thai cua cac node cam bien
typedef struct {
    uint8_t node_id;           // ID cua node
    bool active;               // Trang thai ket noi (true = co ket noi, false = mat ket noi)
    uint16_t last_soil_moisture; // Gia tri do am dat cuoi cung
    uint8_t last_tilt_status;  // Trang thai nghieng cuoi cung
    uint16_t last_battery_level; // Muc pin cuoi cung
    uint16_t last_count;       // Bien dem cuoi cung
    uint32_t last_seen;        // Thoi gian cuoi cung nhan duoc goi tin (giay)
    uint8_t missed_rounds;     // So vong bi mat ket noi
} SlaveStatus;

// Mang luu trang thai cua cac node cam bien (3 node)
static SlaveStatus slaves[MAX_SLAVES] = {0};
// So vong (round) hien tai
static uint16_t current_round = 0;
// Muc canh bao (0: khong co, 1-3: muc do canh bao)
static uint8_t warning_level = 0;
// Bien cau truc cho LCD
static esp32_lcd_i2c_t lcd;

// Ham gui goi ACK cho node cam bien
static void send_ack(uint8_t node_id) {
    lora_send_packet(&node_id, 1);
    ESP_LOGI(TAG, "Da gui ACK cho node %d", node_id);
}

// Ham gui goi sync cho cac node cam bien
static void send_sync(uint8_t count_Sync) {
    SyncPacket sync = {
        .sync_id = SYNC_ID,
        .count_Sync = count_Sync,
        .count_Round = current_round,
        .timestamp = esp_timer_get_time() / 1000000
    };
    lora_send_packet((uint8_t*)&sync, sizeof(sync));
    ESP_LOGI(TAG, "Da gui goi Sync: round=%d, count_Sync=%d, timestamp=%" PRIu32,
             sync.count_Round, sync.count_Sync, sync.timestamp);
}

// Task giao tiep LoRa (nhan goi tu node cam bien va gui sync/ACK)
static void communication_task(void *pvParameters) {
    ESP_LOGI(TAG, "Khoi tao task giao tiep LoRa");
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
                ESP_LOGI(TAG, "Nhan duoc tu node %d: sequence=%d, soil=%d, tilt=%d, battery=%d, count=%d",
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
                ESP_LOGW(TAG, "Goi tin khong hop le: len=%d, node_id=%d", len, pkt.node_id);
            }
        }

        if (current_round >= MAX_ROUNDS) {
            ESP_LOGI(TAG, "Dat toi da %d vong, gui goi Sync de reset", MAX_ROUNDS);
            for (int i = 1; i <= 3; i++) {
                send_sync(i);
                vTaskDelay(pdMS_TO_TICKS(SYNC_INTERVAL_MS));
            }
            current_round = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Task xu ly du lieu (tinh toan muc canh bao)
static void data_processing_task(void *pvParameters) {
    ESP_LOGI(TAG, "Khoi tao task xu ly du lieu");
    while (1) {
        int warning_count = 0;
        for (int i = 0; i < MAX_SLAVES; i++) {
            if (slaves[i].active) {
                float soil_moisture_percent = ((4095.0 - slaves[i].last_soil_moisture) / 4095.0) * 100.0;
                float battery_percent = ((slaves[i].last_battery_level - BATTERY_MIN_MV) / (float)(BATTERY_MAX_MV - BATTERY_MIN_MV)) * 100.0;
                if (battery_percent < 0) battery_percent = 0;
                if (battery_percent > 100) battery_percent = 100;

                ESP_LOGI(TAG, "Node %d: Do am=%.2f%%, Nghieng=%d, Pin=%.2f%%",
                         slaves[i].node_id, soil_moisture_percent, slaves[i].last_tilt_status, battery_percent);

                uint32_t now_s = esp_timer_get_time() / 1000000;
                if (now_s - slaves[i].last_seen > 3 * 60) {
                    slaves[i].missed_rounds++;
                    if (slaves[i].missed_rounds >= 3) {
                        slaves[i].active = false;
                        ESP_LOGW(TAG, "Node %d mat ket noi", slaves[i].node_id);
                    }
                }

                if ((slaves[i].last_tilt_status == 1 || !slaves[i].active) && battery_percent > 30) {
                    warning_count++;
                }
            }
        }

        warning_level = (warning_count >= 3) ? 3 : (warning_count >= 2) ? 2 : (warning_count == 1) ? 1 : 0;
        if (warning_level > 0) {
            ESP_LOGW(TAG, "Muc canh bao %d: %d node co van de", warning_level, warning_count);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Task hien thi thong tin len LCD
static void lcd_display_task(void *pvParameters) {
    ESP_LOGI(TAG, "Khoi tao task hien thi LCD");
    char lcd_buffer[17]; // Buffer 16 ki tu + 1 null terminator

    while (1) {
        esp32_lcd_i2c_clear(&lcd); // Xoa man hinh LCD de tranh tren

        // Dong 1: Hien thi do am phan tram cua 3 node
        snprintf(lcd_buffer, sizeof(lcd_buffer), "Humi:%3.0f %3.0f %3.0f",
                 slaves[0].active ? ((4095.0 - slaves[0].last_soil_moisture) / 4095.0) * 100.0 : 0,
                 slaves[1].active ? ((4095.0 - slaves[1].last_soil_moisture) / 4095.0) * 100.0 : 0,
                 slaves[2].active ? ((4095.0 - slaves[2].last_soil_moisture) / 4095.0) * 100.0 : 0);
        esp32_lcd_i2c_set_cursor(&lcd, 0, 0); // Dat con tro o cot 0, hang 0
        esp32_lcd_i2c_print(&lcd, lcd_buffer); // In chuoi len LCD

        // Dong 2: Hien thi trang thai nghieng cua 3 node
        snprintf(lcd_buffer, sizeof(lcd_buffer), "Tilt:%1d %1d %1d",
                 slaves[0].active ? slaves[0].last_tilt_status : 0,
                 slaves[1].active ? slaves[1].last_tilt_status : 0,
                 slaves[2].active ? slaves[2].last_tilt_status : 0);
        esp32_lcd_i2c_set_cursor(&lcd, 0, 1); // Dat con tro o cot 0, hang 1
        esp32_lcd_i2c_print(&lcd, lcd_buffer); // In chuoi len LCD

        // Dong 3: Hien thi trang thai ket noi (0 = active, 1 = mat ket noi)
        snprintf(lcd_buffer, sizeof(lcd_buffer), "Status:%1d %1d %1d",
                 slaves[0].active ? 0 : 1,
                 slaves[1].active ? 0 : 1,
                 slaves[2].active ? 0 : 1);
        esp32_lcd_i2c_set_cursor(&lcd, 4, 2); // Dat con tro o cot 4, hang 2 (canh giua)
        esp32_lcd_i2c_print(&lcd, lcd_buffer); // In chuoi len LCD

        // Dong 4: Hien thi muc canh bao
        snprintf(lcd_buffer, sizeof(lcd_buffer), "Canh bao muc:%1d", warning_level);
        esp32_lcd_i2c_set_cursor(&lcd, 4, 3); // Dat con tro o cot 4, hang 3 (canh giua)
        esp32_lcd_i2c_print(&lcd, lcd_buffer); // In chuoi len LCD

        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay 1 giay truoc khi cap nhat lai
    }
}

// Ham chinh cua chuong trinh
void app_main(void) {
    ESP_LOGI(TAG, "Master dang khoi dong");

    // Khoi tao LCD
    esp_err_t ret = esp32_lcd_i2c_init(&lcd, LCD_ADDR, LCD_COLS, LCD_ROWS, I2C_PORT, SDA_PIN, SCL_PIN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Khoi tao LCD that bai: %s, dia chi I2C=0x%02x, SDA=%d, SCL=%d",
                 esp_err_to_name(ret), LCD_ADDR, SDA_PIN, SCL_PIN);
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay de ghi log
    } else {
        esp32_lcd_i2c_clear(&lcd); // Xoa man hinh
        esp32_lcd_i2c_backlight_on(&lcd); // Bat den nen
        esp32_lcd_i2c_print(&lcd, "Master Started"); // Hien thi thong bao
        vTaskDelay(pdMS_TO_TICKS(2000)); // Cho 2 giay
        esp32_lcd_i2c_clear(&lcd); // Xoa man hinh
    }

    // Khoi tao thong tin cac node cam bien
    for (int i = 0; i < MAX_SLAVES; i++) {
        slaves[i].node_id = i + 1;
    }

    // Tao cac task voi kiem tra loi
    if (xTaskCreate(communication_task, "CommTask", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Khoi tao task CommTask that bai");
    } else {
        ESP_LOGI(TAG, "Khoi tao task CommTask thanh cong");
    }
    if (xTaskCreate(data_processing_task, "DataTask", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Khoi tao task DataTask that bai");
    } else {
        ESP_LOGI(TAG, "Khoi tao task DataTask thanh cong");
    }
    if (xTaskCreate(lcd_display_task, "LcdTask", 4096, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Khoi tao task LcdTask that bai");
    } else {
        ESP_LOGI(TAG, "Khoi tao task LcdTask thanh cong");
    }

    // Vong lap vo han de ngan main task ket thuc
    ESP_LOGI(TAG, "Vao vong lap vo han trong app_main");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay 1 giay de giam tai nguyen CPU
    }
}