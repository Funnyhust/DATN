#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "communication/communication.h"
#include "warning/warning.h"
#include "lcd/lcd.h"

#define BATTERY_MIN_MV 1000
#define BATTERY_MAX_MV 2000
#define LCD_ADDR 0x27
#define LCD_COLS 16
#define LCD_ROWS 4
#define I2C_PORT I2C_NUM_0
#define SDA_PIN 46
#define SCL_PIN 2

static const char* TAG = "Master";

static SlaveStatus slaves[MAX_SLAVES] = {0};
static uint16_t current_round = 0;
static uint8_t warning_level = 0;
static esp32_lcd_i2c_t lcd;

static void data_processing_task(void *pvParameters) {
    SlaveStatus* slaves = ((SlaveStatus**)pvParameters)[0];
    uint8_t* warning_level = ((uint8_t**)pvParameters)[1];
    uint16_t* current_round = ((uint16_t**)pvParameters)[2];

    ESP_LOGI(TAG, "Khoi tao task xu ly du lieu");
    while (1) {
        int warning_count = 0;
        for (int i = 0; i < MAX_SLAVES; i++) {
            if (slaves[i].active) {
                float soil_moisture_percent = ((slaves[i].last_soil_moisture) / 4095.0) * 100.0;
                float battery_percent = ((slaves[i].last_battery_level - BATTERY_MIN_MV) / (float)(BATTERY_MAX_MV - BATTERY_MIN_MV)) * 100.0;
                battery_percent = battery_percent < 0 ? 0 : (battery_percent > 100 ? 100 : battery_percent);

                if (slaves[i].last_count > 0 && *current_round > slaves[i].last_count + 3) {
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

        *warning_level = (warning_count >= 3) ? 3 : (warning_count >= 2) ? 2 : (warning_count == 1) ? 1 : 0;
        if (*warning_level > 0) {
            ESP_LOGW(TAG, "Muc canh bao %d: %d node co van de", *warning_level, warning_count);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void lcd_display_task(void *pvParameters) {
    SlaveStatus* slaves = ((SlaveStatus**)pvParameters)[0];
    uint8_t* warning_level = ((uint8_t**)pvParameters)[1];
    esp32_lcd_i2c_t* lcd = ((esp32_lcd_i2c_t**)pvParameters)[2];

    ESP_LOGI(TAG, "Khoi tao task hien thi LCD");
    char lcd_buffer[17];

    while (1) {
        esp32_lcd_i2c_clear(lcd);

        snprintf(lcd_buffer, sizeof(lcd_buffer), "Humi:%3.0f %3.0f %3.0f",
                 slaves[0].active ? ((4095.0 - slaves[0].last_soil_moisture) / 4095.0) * 100.0 : 0,
                 slaves[1].active ? ((4095.0 - slaves[1].last_soil_moisture) / 4095.0) * 100.0 : 0,
                 slaves[2].active ? ((4095.0 - slaves[2].last_soil_moisture) / 4095.0) * 100.0 : 0);
        esp32_lcd_i2c_set_cursor(lcd, 0, 0);
        esp32_lcd_i2c_print(lcd, lcd_buffer);

        snprintf(lcd_buffer, sizeof(lcd_buffer), "Tilt:%1d %1d %1d",
                 slaves[0].active ? slaves[0].last_tilt_status : 0,
                 slaves[1].active ? slaves[1].last_tilt_status : 0,
                 slaves[2].active ? slaves[2].last_tilt_status : 0);
        esp32_lcd_i2c_set_cursor(lcd, 0, 1);
        esp32_lcd_i2c_print(lcd, lcd_buffer);

        snprintf(lcd_buffer, sizeof(lcd_buffer), "Status:%1d %1d %1d",
                 slaves[0].active ? 0 : 1,
                 slaves[1].active ? 0 : 1,
                 slaves[2].active ? 0 : 1);
        esp32_lcd_i2c_set_cursor(lcd, -4, 2);
        esp32_lcd_i2c_print(lcd, lcd_buffer);

        snprintf(lcd_buffer, sizeof(lcd_buffer), "Canh bao muc:%1d", *warning_level);
        esp32_lcd_i2c_set_cursor(lcd, -4, 3);
        esp32_lcd_i2c_print(lcd, lcd_buffer);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Master dang khoi dong");

    communication_init(slaves, &current_round);
    warning_init();

    void* comm_params[] = {slaves, &current_round};
    void* data_params[] = {slaves, &warning_level, &current_round};
    void* lcd_params[] = {slaves, &warning_level, &lcd};
    void* warning_params = &warning_level;

    xTaskCreate(communication_task, "CommTask", 8192, comm_params, 5, NULL);
    xTaskCreate(data_processing_task, "DataTask", 8192, data_params, 5, NULL);
    xTaskCreate(lcd_display_task, "LcdTask", 8192, lcd_params, 4, NULL);
    xTaskCreate(warning_task, "WarningTask", 4096, warning_params, 4, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}