#ifndef ESP32_LCD_I2C_H
#define ESP32_LCD_I2C_H

#include <driver/i2c.h>
#include <stdint.h>

typedef struct {
    uint8_t addr;       // Địa chỉ I2C của LCD
    uint8_t cols;       // Số cột của LCD
    uint8_t rows;       // Số hàng của LCD
    uint8_t backlight;  // Trạng thái đèn nền
    i2c_port_t i2c_port; // Cổng I2C
} esp32_lcd_i2c_t;

// Khởi tạo LCD
esp_err_t esp32_lcd_i2c_init(esp32_lcd_i2c_t *lcd, uint8_t lcd_addr, uint8_t lcd_cols, uint8_t lcd_rows, i2c_port_t i2c_port, int sda_pin, int scl_pin);

// Xóa màn hình
void esp32_lcd_i2c_clear(esp32_lcd_i2c_t *lcd);

// Đặt vị trí con trỏ
void esp32_lcd_i2c_set_cursor(esp32_lcd_i2c_t *lcd, uint8_t col, uint8_t row);

// In chuỗi ký tự
void esp32_lcd_i2c_print(esp32_lcd_i2c_t *lcd, const char *str);

// In một ký tự
void esp32_lcd_i2c_print_char(esp32_lcd_i2c_t *lcd, char c);

// Bật đèn nền
void esp32_lcd_i2c_backlight_on(esp32_lcd_i2c_t *lcd);

// Tắt đèn nền
void esp32_lcd_i2c_backlight_off(esp32_lcd_i2c_t *lcd);

#endif