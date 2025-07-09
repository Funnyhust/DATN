#include "rgb_led.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Hàm khởi tạo LED RGB
void rgb_led_init(RGB_LED *led, gpio_num_t red_pin, gpio_num_t green_pin, gpio_num_t blue_pin) {
    led->red_pin = red_pin;
    led->green_pin = green_pin;
    led->blue_pin = blue_pin;

    // Cấu hình các chân GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << red_pin) | (1ULL << green_pin) | (1ULL << blue_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // Tắt tất cả các LED ban đầu
    gpio_set_level(red_pin, 1);
    gpio_set_level(green_pin, 1);
    gpio_set_level(blue_pin, 1);
}

// Hàm bật đèn màu đỏ
void rgb_led_red(RGB_LED *led) {
    gpio_set_level(led->red_pin, 0);
    gpio_set_level(led->green_pin, 1);
    gpio_set_level(led->blue_pin, 1);
}

// Hàm bật đèn màu xanh lá
void rgb_led_green(RGB_LED *led) {
    gpio_set_level(led->red_pin, 1);
    gpio_set_level(led->green_pin, 0);
    gpio_set_level(led->blue_pin, 1);
}

// Hàm bật đèn màu vàng
void rgb_led_yellow(RGB_LED *led) {
    gpio_set_level(led->red_pin, 0);
    gpio_set_level(led->green_pin, 0);
    gpio_set_level(led->blue_pin, 1);
}

// Hàm nhấp nháy LED
void rgb_led_blink(RGB_LED *led, uint8_t red, uint8_t green, uint8_t blue, uint32_t delay_ms, uint8_t times) {
    for (uint8_t i = 0; i < times; i++) {
        // Bật LED với màu chỉ định
        gpio_set_level(led->red_pin, red);
        gpio_set_level(led->green_pin, green);
        gpio_set_level(led->blue_pin, blue);
        
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        
        // Tắt LED
        gpio_set_level(led->red_pin, 1);
        gpio_set_level(led->green_pin, 1);
        gpio_set_level(led->blue_pin, 1);
        
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}