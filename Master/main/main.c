#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lora/lora.h"
#include "lcd/lcd.h"
#include "esp_timer.h"

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

//Packet nhận từ các slave
typedef struct {
    uint8_t node_id;
    uint16_t sequence;
    uint16_t soil_moisture;
    uint8_t tilt_status;
    uint16_t battery_level;
} Packet;
// Gói tin đồng bộ hóa
typedef struct {
    uint8_t sync_id;
    uint8_t count_Sync;
    uint16_t count_Round;
} SyncPacket;
// Trạng thái của các slave
typedef struct {
    uint8_t node_id;
    bool active;
    uint16_t last_soil_moisture;
    uint8_t last_tilt_status;
    uint16_t last_battery_level;
    uint32_t last_seen;
    uint8_t missed_rounds;
} SlaveStatus;

static SlaveStatus slaves[MAX_SLAVES] = {0};
static uint16_t current_round = 0;
static uint8_t warning_level = 0;
static esp32_lcd_i2c_t lcd;

// Gửi ACK cho slave
static void send_ack(uint8_t node_id) {
    ESP_LOGI(TAG, "Sending ACK to node %d", node_id);
    lora_send_packet(&node_id, 1);
    ESP_LOGI(TAG, "Sent ACK to node %d", node_id);
}

// Gửi gói tin đồng bộ hóa
static void send_sync(uint8_t count_Sync) {
    SyncPacket sync = {
        .sync_id = SYNC_ID,
        .count_Sync = count_Sync,
        .count_Round = current_round,
    };
    ESP_LOGI(TAG, "Sending Sync: round=%d, count_Sync=%d" PRIu32,
             sync.count_Round, sync.count_Sync);
    lora_send_packet((uint8_t*)&sync, sizeof(sync));
    ESP_LOGI(TAG, "Sent Sync");
}

// Task xử lý giao tiếp với các slave
static void communication_task(void *pvParameters) {
    while (1) {
        // ESP_LOGI(TAG, "Checking for LoRa packet");
        if (lora_received()) {
            Packet pkt;
            int len = lora_receive_packet((uint8_t*)&pkt, sizeof(pkt));
            ESP_LOGI(TAG, "Received packet of length %d", len);
            if (len == sizeof(pkt) && pkt.node_id > 0 && pkt.node_id <= MAX_SLAVES) {
                ESP_LOGI(TAG, "Received from node %d: sequence=%d, soil=%d, tilt=%d, battery=%d",
                         pkt.node_id, pkt.sequence, pkt.soil_moisture, pkt.tilt_status, pkt.battery_level);

                SlaveStatus* slave = &slaves[pkt.node_id - 1];
                slave->node_id = pkt.node_id;
                slave->active = true;
                slave->last_soil_moisture = pkt.soil_moisture;
                slave->last_tilt_status = pkt.tilt_status;
                slave->last_battery_level = pkt.battery_level;
                slave->last_seen = esp_timer_get_time() / 1000000;
                slave->missed_rounds = 0;

                // Kiểm tra độ ẩm đất và gửi ACK hoặc Sync
                if (pkt.soil_moisture < 3276) {
                    vTaskDelay(pdMS_TO_TICKS(5000)); // Giảm delay xuống 1 giây để test nhanh hơn
                    send_ack(pkt.node_id);
                } else {
                    vTaskDelay(pdMS_TO_TICKS(1000)); // Giảm delay xuống 1 giây để test nhanh hơn
                    for (int i = 1; i <= 3; i++) {
                        send_sync(i);
                        vTaskDelay(pdMS_TO_TICKS(SYNC_INTERVAL_MS));
                    }
                }

                current_round++;
                lora_receive(); // Đặt lại chế độ nhận sau khi xử lý gói tin hợp lệ
            } else {
                ESP_LOGW(TAG, "Invalid packet received: len=%d, node_id=%d", len, pkt.node_id);
                lora_receive(); // Đặt lại chế độ nhận nếu gói tin không hợp lệ
            }
        } 
        // else {
        //     ESP_LOGI(TAG, "No packet received"); // Thêm log khi không nhận được gói tin
        // }

        if (current_round >= MAX_ROUNDS) {
            ESP_LOGI(TAG, "Reached max rounds (%d), sending Sync for reset", MAX_ROUNDS);
            for (int i = 1; i <= 3; i++) {
                send_sync(i);
                vTaskDelay(pdMS_TO_TICKS(SYNC_INTERVAL_MS));
            }
            current_round = 0;
            lora_receive(); // Đặt lại chế độ nhận sau khi reset
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Task xử lý dữ liệu từ các slave
static void data_processing_task(void *pvParameters) {
    ESP_LOGI(TAG, "Khoi tao task xu ly du lieu");
    while (1) {
        int warning_count = 0;
        for (int i = 0; i < MAX_SLAVES; i++) {
            if (slaves[i].active) {
                float soil_moisture_percent = (( slaves[i].last_soil_moisture) / 4095.0) * 100.0;
                float battery_percent = ((slaves[i].last_battery_level - BATTERY_MIN_MV) / (float)(BATTERY_MAX_MV - BATTERY_MIN_MV)) * 100.0;
                battery_percent = battery_percent < 0 ? 0 : (battery_percent > 100 ? 100 : battery_percent);

                // ESP_LOGI(TAG, "Node %d: Do am=%.2f%%, Nghieng=%d, Pin=%.2f%%",
                //          slaves[i].node_id, soil_moisture_percent, slaves[i].last_tilt_status, battery_percent);

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

// Task hiển thị thông tin lên LCD
static void lcd_display_task(void *pvParameters) {
    ESP_LOGI(TAG, "Khoi tao task hien thi LCD");
    char lcd_buffer[17];

    while (1) {
        esp32_lcd_i2c_clear(&lcd);

        snprintf(lcd_buffer, sizeof(lcd_buffer), "Humi:%3.0f %3.0f %3.0f",
                 slaves[0].active ? ((slaves[0].last_soil_moisture) / 4095.0) * 100.0 : 0,
                 slaves[1].active ? ((slaves[1].last_soil_moisture) / 4095.0) * 100.0 : 0,
                 slaves[2].active ? ((slaves[2].last_soil_moisture) / 4095.0) * 100.0 : 0);
        esp32_lcd_i2c_set_cursor(&lcd, 0, 0);
        esp32_lcd_i2c_print(&lcd, lcd_buffer);

        snprintf(lcd_buffer, sizeof(lcd_buffer), "Tilt:%1d %1d %1d",
                 slaves[0].active ? slaves[0].last_tilt_status : 0,
                 slaves[1].active ? slaves[1].last_tilt_status : 0,
                 slaves[2].active ? slaves[2].last_tilt_status : 0);
        esp32_lcd_i2c_set_cursor(&lcd, 0, 1);
        esp32_lcd_i2c_print(&lcd, lcd_buffer);

        snprintf(lcd_buffer, sizeof(lcd_buffer), "Status:%1d %1d %1d",
                 slaves[0].active ? 0 : 1,
                 slaves[1].active ? 0 : 1,
                 slaves[2].active ? 0 : 1);
        esp32_lcd_i2c_set_cursor(&lcd, -4, 2);
        esp32_lcd_i2c_print(&lcd, lcd_buffer);

        snprintf(lcd_buffer, sizeof(lcd_buffer), "Canh bao muc:%1d", warning_level);
        esp32_lcd_i2c_set_cursor(&lcd, -4, 3);
        esp32_lcd_i2c_print(&lcd, lcd_buffer);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Master dang khoi dong");

    // Khởi tạo LoRa
    ESP_LOGI(TAG, "Khoi tao task giao tiep LoRa");
    if (lora_init() != 1) { // Kiểm tra giá trị trả về của lora_init()
        ESP_LOGE(TAG, "Khoi tao LoRa that bai");
        // Tiếp tục chạy để debug LCD và các tác vụ khác
    } else {
        ESP_LOGI(TAG, "Khoi tao LoRa thanh cong");
        lora_set_frequency(433000000);
        lora_set_spreading_factor(7); // Giữ nguyên SF 10 như ban đầu
        lora_set_bandwidth(125E3);
        lora_set_coding_rate(5);
        lora_enable_crc();
        lora_set_preamble_length(12);
        lora_set_sync_word(0x34);
        lora_set_tx_power(17);
        lora_receive(); // Đặt chế độ nhận ngay sau khi cấu hình
    }

    // Khởi tạo LCD
    esp_err_t ret = esp32_lcd_i2c_init(&lcd, LCD_ADDR, LCD_COLS, LCD_ROWS, I2C_PORT, SDA_PIN, SCL_PIN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Khoi tao LCD that bai: %s, dia chi I2C=0x%02x, SDA=%d, SCL=%d",
                 esp_err_to_name(ret), LCD_ADDR, SDA_PIN, SCL_PIN);
    } else {
        esp32_lcd_i2c_clear(&lcd);
        esp32_lcd_i2c_backlight_on(&lcd);
        esp32_lcd_i2c_print(&lcd, "Master Started");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp32_lcd_i2c_clear(&lcd);
    }

    // Khởi tạo mảng slaves
    for (int i = 0; i < MAX_SLAVES; i++) {
        slaves[i].node_id = i + 1;
    }

    // Tạo các tác vụ
    xTaskCreate(communication_task, "CommTask", 8192, NULL, 5, NULL);
    xTaskCreate(data_processing_task, "DataTask", 8192, NULL, 5, NULL);
    xTaskCreate(lcd_display_task, "LcdTask", 8192, NULL, 4, NULL);

    while (1) {
    //     ESP_LOGI(TAG, "Vong lap chinh app_main");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}