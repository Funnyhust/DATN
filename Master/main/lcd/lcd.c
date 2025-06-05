#include "lcd.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00

#define LCD_BACKLIGHT 0x08
#define LCD_NOBACKLIGHT 0x00

#define En 0b00000100
#define Rw 0b00000010
#define Rs 0b00000001

static const char *TAG = "ESP32_LCD_I2C";

static void lcd_write4bits(esp32_lcd_i2c_t *lcd, uint8_t value);
static void lcd_pulse_enable(esp32_lcd_i2c_t *lcd, uint8_t data);
static void lcd_send(esp32_lcd_i2c_t *lcd, uint8_t value, uint8_t mode);
static void lcd_init(esp32_lcd_i2c_t *lcd);

esp_err_t esp32_lcd_i2c_init(esp32_lcd_i2c_t *lcd, uint8_t lcd_addr, uint8_t lcd_cols, uint8_t lcd_rows, i2c_port_t i2c_port, int sda_pin, int scl_pin) {
    lcd->addr = lcd_addr;
    lcd->cols = lcd_cols;
    lcd->rows = lcd_rows;
    lcd->backlight = LCD_BACKLIGHT;
    lcd->i2c_port = i2c_port;

    // Cấu hình I2C với tần số 50kHz để tăng độ tương thích
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_pin,
        .scl_io_num = scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 50000 // 50kHz
    };
    esp_err_t ret = i2c_param_config(lcd->i2c_port, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(lcd->i2c_port, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    lcd_init(lcd);
    return ESP_OK;
}

static void lcd_init(esp32_lcd_i2c_t *lcd) {
    vTaskDelay(2000 / portTICK_PERIOD_MS); // Tăng thời gian chờ lên 2 giây

    lcd_write4bits(lcd, 0x03 << 4);
    vTaskDelay(10 / portTICK_PERIOD_MS); // Tăng từ 5ms lên 10ms
    lcd_write4bits(lcd, 0x03 << 4);
    vTaskDelay(10 / portTICK_PERIOD_MS); // Tăng từ 5ms lên 10ms
    lcd_write4bits(lcd, 0x03 << 4);
    vTaskDelay(200 / portTICK_PERIOD_MS); // Tăng từ 150ms lên 200ms
    lcd_write4bits(lcd, 0x02 << 4);

    lcd_send(lcd, LCD_FUNCTIONSET | LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS, 0);
    lcd_send(lcd, LCD_DISPLAYCONTROL | LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF, 0);
    esp32_lcd_i2c_clear(lcd);
    lcd_send(lcd, LCD_ENTRYMODESET | LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT, 0);
}

void esp32_lcd_i2c_clear(esp32_lcd_i2c_t *lcd) {
    lcd_send(lcd, LCD_CLEARDISPLAY, 0);
    vTaskDelay(2 / portTICK_PERIOD_MS);
}

void esp32_lcd_i2c_set_cursor(esp32_lcd_i2c_t *lcd, uint8_t col, uint8_t row) {
    int row_offsets[] = {0x00, 0x40, 0x14, 0x54};
    if (row >= lcd->rows) row = lcd->rows - 1;
    lcd_send(lcd, LCD_SETDDRAMADDR | (col + row_offsets[row]), 0);
}

void esp32_lcd_i2c_print(esp32_lcd_i2c_t *lcd, const char *str) {
    while (*str) {
        esp32_lcd_i2c_print_char(lcd, *str++);
    }
}

void esp32_lcd_i2c_print_char(esp32_lcd_i2c_t *lcd, char c) {
    lcd_send(lcd, (uint8_t)c, Rs);
}

void esp32_lcd_i2c_backlight_on(esp32_lcd_i2c_t *lcd) {
    lcd->backlight = LCD_BACKLIGHT;
    lcd_send(lcd, 0, 0);
}

void esp32_lcd_i2c_backlight_off(esp32_lcd_i2c_t *lcd) {
    lcd->backlight = LCD_NOBACKLIGHT;
    lcd_send(lcd, 0, 0);
}

static void lcd_send(esp32_lcd_i2c_t *lcd, uint8_t value, uint8_t mode) {
    uint8_t highnib = value & 0xF0;
    uint8_t lownib = (value << 4) & 0xF0;
    lcd_write4bits(lcd, (highnib) | mode);
    lcd_write4bits(lcd, (lownib) | mode);
}

static void lcd_write4bits(esp32_lcd_i2c_t *lcd, uint8_t value) {
    // Nếu module LCD có ánh xạ chân khác, điều chỉnh tại đây
    // Ví dụ: Nếu D4-D7 đảo ngược, dùng: uint8_t data = ((value >> 4) & 0x0F) | ((value << 4) & 0xF0) | lcd->backlight;
    uint8_t data = value | lcd->backlight;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (lcd->addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(lcd->i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C write failed: %s, data: 0x%02X", esp_err_to_name(ret), data);
    } 
    i2c_cmd_link_delete(cmd);
    lcd_pulse_enable(lcd, data);
}

static void lcd_pulse_enable(esp32_lcd_i2c_t *lcd, uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (lcd->addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, (data | En) | lcd->backlight, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(lcd->i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C pulse enable high failed: %s", esp_err_to_name(ret));
    }
    i2c_cmd_link_delete(cmd);

    vTaskDelay(2 / portTICK_PERIOD_MS); // Tăng thời gian chờ lên 2ms

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (lcd->addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, (data & ~En) | lcd->backlight, true);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(lcd->i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C pulse enable low failed: %s", esp_err_to_name(ret));
    }
    i2c_cmd_link_delete(cmd);

    vTaskDelay(2 / portTICK_PERIOD_MS); // Tăng thời gian chờ lên 2ms
}