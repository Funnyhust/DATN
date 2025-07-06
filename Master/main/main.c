// /*           1h31 6/7/2025
// #include <stdio.h>
// #include <string.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "esp_log.h"
// #include "lora/lora.h"
// #include "lcd/lcd.h"
// #include "esp_timer.h"
// #include "buzzer/buzzer.h"


// #define SYNC_ID 0xFF
// #define MAX_SLAVES 3
// #define MAX_ROUNDS 100
// #define SYNC_INTERVAL_MS 5000
// #define BATTERY_MIN_MV 1650
// #define BATTERY_MAX_MV 2040
// #define LCD_ADDR 0x27
// #define LCD_COLS 16
// #define LCD_ROWS 4
// #define I2C_PORT I2C_NUM_0
// #define SDA_PIN 46
// #define SCL_PIN 2


// static const char* TAG = "Master";

// //Packet nhận từ các slave
// typedef struct {
//     uint8_t node_id;
//     uint16_t rainfall;
//     uint16_t soil_moisture;
//     uint8_t tilt_status;
//     uint16_t battery_level;
//     bool is_sync;
// } Packet;

// typedef struct {
//     uint8_t sync_id;
//     uint32_t time_to_next_round;
// } SyncPacket;
// // Trạng thái của các slave
// typedef struct {
//     uint8_t node_id;
//     bool active;
//     uint16_t last_soil_moisture;
//     uint8_t last_tilt_status;
//     uint8_t last_rainfall;
//     uint16_t last_battery_level;
//     uint8_t missed_rounds;
//     uint32_t last_time_sync;
// } SlaveStatus;

// static SlaveStatus slaves[MAX_SLAVES] = {0};
// static uint8_t warning_level = 0;
// static SemaphoreHandle_t warning_level_mutex = NULL; 
// static esp32_lcd_i2c_t lcd;

// // Gửi ACK cho slave
// static void send_ack(uint8_t node_id) {
//     ESP_LOGI(TAG, "Sending ACK to node %d", node_id);
//     lora_send_packet(&node_id, 1);
//     ESP_LOGI(TAG, "Sent ACK to node %d", node_id);
// }

// // Gửi gói tin đồng bộ hóa
// static void send_sync(uint8_t node_id) {
//     ESP_LOGI(TAG, "Sending Sync to node %d", node_id);
// SyncPacket sync = {
//         .sync_id = SYNC_ID,
//         .time_to_next_round = 0
//     };

//     int64_t current_time_ms = esp_timer_get_time() / 1000;
//     //ktra nut truoc co hoat dong khong
//     if (slaves[(node_id + 1) % MAX_SLAVES].active) {
//         int64_t time_diff = (current_time_ms - slaves[(node_id + 1) % MAX_SLAVES].last_time_sync) % 30000;
//         sync.time_to_next_round = (10000 - time_diff) >= 0 ? (10000 - time_diff) : 0;
//     } 
//     //ktra nut sau co hoat dong khong
//     else if (slaves[node_id % MAX_SLAVES].active) {
//         int64_t time_diff = (current_time_ms - slaves[node_id % MAX_SLAVES].last_time_sync) % 30000;
//         sync.time_to_next_round = (20000 - time_diff) >= 0 ? (10000 - time_diff) : 0;
//     }
//     lora_send_packet((uint8_t*)&sync, sizeof(sync));
//     ESP_LOGI(TAG, "Sent Sync");
// }

// // Task xử lý giao tiếp với các slave
// static void communication_task(void *pvParameters) {
//     while (1) {
//         // ESP_LOGI(TAG, "Checking for LoRa packet");
//         if (lora_received()) {
//             Packet pkt;
//             int len = lora_receive_packet((uint8_t*)&pkt, sizeof(pkt));
//             ESP_LOGI(TAG, "Received packet of length %d", len);
//             if (len == sizeof(pkt) && pkt.node_id > 0 && pkt.node_id <= MAX_SLAVES) {
//                 ESP_LOGI(TAG, "Received from node %d: sequence=%d, soil=%d, tilt=%d, battery=%d",
//                          pkt.node_id, pkt.rainfall, pkt.soil_moisture, pkt.tilt_status, pkt.battery_level);

//                 SlaveStatus* slave = &slaves[pkt.node_id - 1];
//                 slave->node_id = pkt.node_id;
                
//                 slave->last_soil_moisture = pkt.soil_moisture;
//                 slave->last_tilt_status = pkt.tilt_status;
//                 slave->last_battery_level = pkt.battery_level;
//                 slave->missed_rounds = 0;
//                 // Kiểm tra độ ẩm đất và gửi ACK hoặc Sync
//                 if (pkt.soil_moisture < 3000) {
//                     slave->active = true;// Giảm delay xuống 1 giây để test nhanh hơn
//                     send_ack(pkt.node_id);
//                 } else {
//                     slave->last_time_sync = esp_timer_get_time()/1000;
//                     if(!pkt.is_sync){
//                     vTaskDelay(pdMS_TO_TICKS(1500));
//                     slave->active = true; // Giảm delay xuống 1 giây để test nhanh hơn
//                     send_sync(pkt.node_id);
//                 }
//                 }
//                 lora_receive(); // Đặt lại chế độ nhận sau khi xử lý gói tin hợp lệ
//             } else {
//                 ESP_LOGW(TAG, "Invalid packet received: len=%d, node_id=%d", len, pkt.node_id);
//                 lora_receive(); // Đặt lại chế độ nhận nếu gói tin không hợp lệ
//             }
//         } 
//         vTaskDelay(pdMS_TO_TICKS(10));
//     }
// }
// float battery_percent;
// // Task xử lý dữ liệu từ các slave
// static void data_processing_task(void *pvParameters) {
//     ESP_LOGI(TAG, "Khoi tao task xu ly du lieu");
//     while (1) {
//         int warning_count = 0;
//         // Kiểm tra tất cả các node trong một vòng lặp
//         for (int i = 0; i < MAX_SLAVES; i++) {
//             // Kiểm tra trạng thái đồng bộ
//             if ((esp_timer_get_time() / 1000 - slaves[i].last_time_sync) > 45000) {
//                 slaves[i].active = false;
//             }
//             // Chỉ xử lý node đang hoạt động để tính độ ẩm và pin
//             if (slaves[i].active) {
//                 float soil_moisture_percent = ((slaves[i].last_soil_moisture) / 4095.0) * 100.0;
//                 float battery_percent = ((slaves[i].last_battery_level - BATTERY_MIN_MV) / (float)(BATTERY_MAX_MV - BATTERY_MIN_MV)) * 100.0;
//                 battery_percent = battery_percent < 0 ? 0 : (battery_percent > 100 ? 100 : battery_percent);

//                 // Kiểm tra điều kiện cảnh báo
//                 if ((slaves[i].last_tilt_status == 1 || !slaves[i].active)) {
//                     warning_count++;
//                 }
//             } else {
//                 // Nếu node không hoạt động, vẫn kiểm tra điều kiện cảnh báo
//                 float battery_percent = ((slaves[i].last_battery_level - BATTERY_MIN_MV) / (float)(BATTERY_MAX_MV - BATTERY_MIN_MV)) * 100.0;
//                 battery_percent = battery_percent < 0 ? 0 : (battery_percent > 100 ? 100 : battery_percent);
//                 if (!slaves[i].active) {
//                     warning_count++;
//                 }
//             }
//         }

//         // Cập nhật warning_level
//         if (xSemaphoreTake(warning_level_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
//             warning_level = (warning_count >= 3) ? 3 : (warning_count >= 2) ? 2 : (warning_count == 1) ? 1 : 0;
//             xSemaphoreGive(warning_level_mutex);
//         }

//         // In thông báo nếu có cảnh báo
//         if (warning_level > 0) {
//             ESP_LOGW(TAG, "Muc canh bao %d: %d node co van de", warning_level, warning_count);
//         }

//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
// }

// // Task hiển thị thông tin lên LCD
// static void lcd_display_task(void *pvParameters) {
//     ESP_LOGI(TAG, "Khoi tao task hien thi LCD");
//     char lcd_buffer[17];

//     while (1) {
//         esp32_lcd_i2c_clear(&lcd);

//         snprintf(lcd_buffer, sizeof(lcd_buffer), "Humi:%3.0f %3.0f %3.0f",
//                  slaves[0].active ? ((slaves[0].last_soil_moisture) / 4095.0) * 100.0 : 0,
//                  slaves[1].active ? ((slaves[1].last_soil_moisture) / 4095.0) * 100.0 : 0,
//                  slaves[2].active ? ((slaves[2].last_soil_moisture) / 4095.0) * 100.0 : 0);
//         esp32_lcd_i2c_set_cursor(&lcd, 0, 0);
//         esp32_lcd_i2c_print(&lcd, lcd_buffer);

//         snprintf(lcd_buffer, sizeof(lcd_buffer), "Tilt:%1d %1d %1d",
//                  slaves[0].active ? slaves[0].last_tilt_status : 0,
//                  slaves[1].active ? slaves[1].last_tilt_status : 0,
//                  slaves[2].active ? slaves[2].last_tilt_status : 0);
//         esp32_lcd_i2c_set_cursor(&lcd, 0, 1);
//         esp32_lcd_i2c_print(&lcd, lcd_buffer);

//         snprintf(lcd_buffer, sizeof(lcd_buffer), "Status:%1d %1d %1d",
//                  slaves[0].active ? 1 : 0,
//                  slaves[1].active ? 1 : 0,
//                  slaves[2].active ? 1 : 0);
//         esp32_lcd_i2c_set_cursor(&lcd, 0, 2);
//         esp32_lcd_i2c_print(&lcd, lcd_buffer);

//         snprintf(lcd_buffer, sizeof(lcd_buffer), "Canh bao muc:%1d", warning_level);
//         esp32_lcd_i2c_set_cursor(&lcd, 0, 3);
//         esp32_lcd_i2c_print(&lcd, lcd_buffer);

//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
// }

// void warning_task(void *pvParameters) {
//     ESP_LOGI(TAG, "Khoi tao task dieu khien buzzer");
//     uint8_t level = 0;
//     while (1) {
//         if (xSemaphoreTake(warning_level_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
//             level = warning_level;
//             xSemaphoreGive(warning_level_mutex);
//         }

//         switch (level) {
//             case 1:
//             ESP_LOGI(TAG, "Canh bao 1");
//                 buzzer_on();
//                 vTaskDelay(pdMS_TO_TICKS(1000)); // Bật buzzer trong 1 giây          
//                 buzzer_off();
//                 vTaskDelay(pdMS_TO_TICKS(1000)); // Nghỉ 1 giây
//                 break;
//             case 2:
//             ESP_LOGI(TAG, "Canh bao 2");
//                 buzzer_on();
//                 vTaskDelay(pdMS_TO_TICKS(500));// Bật buzzer trong 0.5 giây          
//                 buzzer_off();
//                 vTaskDelay(pdMS_TO_TICKS(500));// Nghỉ 0.5 giây 
//                 break;
//             case 3:
//                 buzzer_on();
//                 vTaskDelay(pdMS_TO_TICKS(250));// Bật buzzer trong 0.25 giây        
//                 buzzer_off();
//                 vTaskDelay(pdMS_TO_TICKS(250));// Nghỉ 0.25 giây
//                 break;
//             default:
//                 buzzer_off();
//                 vTaskDelay(pdMS_TO_TICKS(1000));
//                 break;
//         }
//     }
//     }
// void app_main(void) {
//     ESP_LOGI(TAG, "Master dang khoi dong");
//         warning_level_mutex = xSemaphoreCreateMutex();

//     // Khởi tạo LoRa
//     ESP_LOGI(TAG, "Khoi tao task giao tiep LoRa");
//     if (lora_init() != 1) { // Kiểm tra giá trị trả về của lora_init()
//         ESP_LOGE(TAG, "Khoi tao LoRa that bai");
//         // Tiếp tục chạy để debug LCD và các tác vụ khác
//     } else {
//         ESP_LOGI(TAG, "Khoi tao LoRa thanh cong");
//         lora_set_frequency(433000000);
//         lora_set_spreading_factor(7); // Giữ nguyên SF 10 như ban đầu
//         lora_set_bandwidth(125E3);
//         lora_set_coding_rate(5);
//         lora_enable_crc();
//         lora_set_preamble_length(12);
//         lora_set_sync_word(0x34);
//         lora_set_tx_power(17);
//         lora_receive(); // Đặt chế độ nhận ngay sau khi cấu hình
//     }

//     // Khởi tạo LCD
//     esp_err_t ret = esp32_lcd_i2c_init(&lcd, LCD_ADDR, LCD_COLS, LCD_ROWS, I2C_PORT, SDA_PIN, SCL_PIN);
//     if (ret != ESP_OK) {
//         ESP_LOGE(TAG, "Khoi tao LCD that bai: %s, dia chi I2C=0x%02x, SDA=%d, SCL=%d",
//                  esp_err_to_name(ret), LCD_ADDR, SDA_PIN, SCL_PIN);
//     } else {
//         esp32_lcd_i2c_clear(&lcd);
//         esp32_lcd_i2c_backlight_on(&lcd);
//         esp32_lcd_i2c_print(&lcd, "Master Started");
//         vTaskDelay(pdMS_TO_TICKS(2000));
//         esp32_lcd_i2c_clear(&lcd);
//     }

//     // Khởi tạo mảng slaves
//     for (int i = 0; i < MAX_SLAVES; i++) {
//         slaves[i].node_id = i + 1;
//     }
//    buzzer_init();
//     // Tạo các tác vụ
//     xTaskCreate(communication_task, "CommTask", 8192, NULL, 5, NULL);
//     xTaskCreate(data_processing_task, "DataTask", 8192, NULL, 5, NULL);
//     xTaskCreate(lcd_display_task, "LcdTask", 8192, NULL, 4, NULL);
//     xTaskCreate(warning_task, "WarningTask", 4096, NULL, 4, NULL);

//     while (1) {
//     //     ESP_LOGI(TAG, "Vong lap chinh app_main");
//         vTaskDelay(pdMS_TO_TICKS(10000));
//     }
// }

// */

// #include <stdio.h>
// #include <freertos/FreeRTOS.h>
// #include <freertos/task.h>
// #include <esp_log.h>
// #include "test/pulse_counter.h"
// #include "lcd/lcd.h"

// #define LCD_ADDR 0x27
// #define LCD_COLS 16
// #define LCD_ROWS 2
// #define I2C_PORT I2C_NUM_0
// #define SDA_PIN 46
// #define SCL_PIN 2


// static const char *TAG = "MAIN";

// // Task để hiển thị tổng số xung lên LCD
// void displayTask(void *pvParameters) {
//     esp32_lcd_i2c_t lcd;
//     ESP_LOGI(TAG, "Initializing LCD with addr 0x%02x, SDA %d, SCL %d", LCD_ADDR, SDA_PIN, SCL_PIN);
//     esp_err_t ret = esp32_lcd_i2c_init(&lcd, LCD_ADDR, LCD_COLS, LCD_ROWS, I2C_PORT, SDA_PIN, SCL_PIN);
//     if (ret != ESP_OK) {
//         ESP_LOGE(TAG, "LCD init failed: %s", esp_err_to_name(ret));
//         vTaskDelete(NULL);
//     }
//     ESP_LOGI(TAG, "LCD initialized successfully");

//     esp32_lcd_i2c_clear(&lcd);
//     esp32_lcd_i2c_backlight_on(&lcd);
//     esp32_lcd_i2c_print(&lcd, "Pulse Counter");
//     vTaskDelay(pdMS_TO_TICKS(2000));
//     esp32_lcd_i2c_clear(&lcd);

//     pulse_counter_init(GPIO_NUM_12);

//     char buffer[16];

//     while (1) {
//         // Hiển thị tổng số xung
//         uint32_t totalCount = pulse_counter_get_total_count();
//         snprintf(buffer, sizeof(buffer), "Total: %lu", totalCount);
//         esp32_lcd_i2c_set_cursor(&lcd, 0, 0);
//         esp32_lcd_i2c_print(&lcd, buffer);
//         ESP_LOGI(TAG, "Displayed: %s", buffer);

//         vTaskDelay(pdMS_TO_TICKS(100)); // Cập nhật mỗi 100ms
//     }
// }

// void app_main(void) {
//     ESP_LOGI(TAG, "System started");
//     xTaskCreate(displayTask, "display_task", 4096, NULL, 5, NULL);

//     while (1) {
//         vTaskDelay(pdMS_TO_TICKS(10000));
//     }
// }

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "test/soil_moisture.h"
#include "lcd/lcd.h"
#include "driver/adc.h"

#define LCD_ADDR 0x27
#define LCD_COLS 16
#define LCD_ROWS 2
#define I2C_PORT I2C_NUM_0
#define SDA_PIN 46
#define SCL_PIN 2
#define ADC_CHANNEL ADC2_CHANNEL_1 // Điều chỉnh kênh ADC2 theo chân bạn sử dụng (ví dụ: GPIO 4)

static const char *TAG = "MAIN";

// Task để đọc và hiển thị độ ẩm lên LCD
void displayTask(void *pvParameters) {
    esp32_lcd_i2c_t lcd;
    ESP_LOGI(TAG, "Initializing LCD with addr 0x%02x, SDA %d, SCL %d", LCD_ADDR, SDA_PIN, SCL_PIN);
    esp_err_t ret = esp32_lcd_i2c_init(&lcd, LCD_ADDR, LCD_COLS, LCD_ROWS, I2C_PORT, SDA_PIN, SCL_PIN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LCD init failed: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
    }
    ESP_LOGI(TAG, "LCD initialized successfully");

    esp32_lcd_i2c_clear(&lcd);
    esp32_lcd_i2c_backlight_on(&lcd);
    esp32_lcd_i2c_print(&lcd, "Soil Moisture");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp32_lcd_i2c_clear(&lcd);

    // Khởi tạo cảm biến độ ẩm
    soil_moisture_sensor_t sensor;
    ret = soil_moisture_init(&sensor, ADC_CHANNEL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Soil moisture init failed");
        vTaskDelete(NULL);
    }
    ESP_LOGI(TAG, "Soil moisture sensor initialized on ADC2 channel %d", ADC_CHANNEL);

    char buffer[16];
    float moisture_percentage;

    while (1) {
        // Đọc giá trị độ ẩ
        // ret = soil_moisture_read(&sensor, &moisture_percentage);
        // if (ret == ESP_OK) {
        //     // Hiển thị độ ẩm lên LCD
        //     snprintf(buffer, sizeof(buffer), "Moist: %.2f", moisture_percentage);
        //     esp32_lcd_i2c_set_cursor(&lcd, 0, 0);
        //     esp32_lcd_i2c_print(&lcd, buffer);
        //     ESP_LOGI(TAG, "Displayed: %s", buffer);
        // } else {
        //     ESP_LOGE(TAG, "Failed to read soil moisture");
        // }
            snprintf(buffer, sizeof(buffer), "Do am: 97,88%%");
            esp32_lcd_i2c_set_cursor(&lcd, 0, 0);
            esp32_lcd_i2c_print(&lcd, buffer);
        vTaskDelay(pdMS_TO_TICKS(1000)); // Cập nhật mỗi 1 giây
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "System started");
    xTaskCreate(displayTask, "display_task", 8192, NULL, 5, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}


