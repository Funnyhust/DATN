#ifndef RGB_H
#define RGB_H

#include <stdint.h>
#include "driver/gpio.h"

// Định nghĩa cấu trúc LED RGB
typedef struct {
    gpio_num_t red_pin;
    gpio_num_t green_pin;
    gpio_num_t blue_pin;
} RGB_LED;

// Hàm khởi tạo LED RGB
void rgb_led_init(RGB_LED *led, gpio_num_t red_pin, gpio_num_t green_pin, gpio_num_t blue_pin);

// Hàm bật đèn màu đỏ
void rgb_led_red(RGB_LED *led);

// Hàm bật đèn màu xanh lá
void rgb_led_green(RGB_LED *led);

// Hàm bật đèn màu vàng
void rgb_led_yellow(RGB_LED *led);

// Hàm nhấp nháy LED với màu và thời gian chỉ định
void rgb_led_blink(RGB_LED *led, uint8_t red, uint8_t green, uint8_t blue, uint32_t delay_ms, uint8_t times);

#endif // RGB_LED_H