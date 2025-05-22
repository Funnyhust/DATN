#include "dht11.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static uint8_t dht11_pin;

void dht11_init(uint8_t gpio_pin) {
    dht11_pin = gpio_pin;
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_pin),
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

static bool dht11_start_signal(void) {
    gpio_set_direction(dht11_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(dht11_pin, 0);
    vTaskDelay(20 / portTICK_PERIOD_MS); // Giữ thấp 18ms
    gpio_set_level(dht11_pin, 1);
    ets_delay_us(40); // Giữ cao 20-40us
    gpio_set_direction(dht11_pin, GPIO_MODE_INPUT);
    
    // Chờ phản hồi từ cảm biến
    uint32_t timeout = 10000;
    while (gpio_get_level(dht11_pin) == 1 && timeout--) ets_delay_us(1);
    if (timeout == 0) return false;
    
    timeout = 10000;
    while (gpio_get_level(dht11_pin) == 0 && timeout--) ets_delay_us(1);
    if (timeout == 0) return false;
    
    timeout = 10000;
    while (gpio_get_level(dht11_pin) == 1 && timeout--) ets_delay_us(1);
    if (timeout == 0) return false;
    
    return true;
}

static uint8_t dht11_read_byte(void) {
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        uint32_t timeout = 10000;
        while (gpio_get_level(dht11_pin) == 0 && timeout--) ets_delay_us(1);
        ets_delay_us(40); // Chờ giữa bit
        if (gpio_get_level(dht11_pin) == 1) {
            byte |= (1 << (7 - i));
            timeout = 10000;
            while (gpio_get_level(dht11_pin) == 1 && timeout--) ets_delay_us(1);
        }
    }
    return byte;
}

float dht11_read_humidity(bool *error, char *error_msg, size_t msg_len) {
    *error = false;
    uint8_t data[5] = {0};
    
    if (!dht11_start_signal()) {
        *error = true;
        snprintf(error_msg, msg_len, "Lỗi khởi tạo tín hiệu DHT11");
        return -1.0f;
    }
    
    for (int i = 0; i < 5; i++) {
        data[i] = dht11_read_byte();
    }
    
    // Kiểm tra checksum
    if (data[4] != (data[0] + data[1] + data[2] + data[3])) {
        *error = true;
        snprintf(error_msg, msg_len, "Lỗi checksum dữ liệu DHT11");
        return -1.0f;
    }
    
    // Trả về độ ẩm
    return (float)data[0];
}