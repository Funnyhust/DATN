#include "sw520.h"
#include "driver/gpio.h"

static uint8_t tilt_pins[4]; // Mảng lưu 4 chân GPIO cho 4 cảm biến

void tilt_sensor_init(uint8_t gpio_pin1, uint8_t gpio_pin2, uint8_t gpio_pin3, uint8_t gpio_pin4) {
    // Lưu 4 chân GPIO vào mảng
    tilt_pins[0] = gpio_pin1;
    tilt_pins[1] = gpio_pin2;
    tilt_pins[2] = gpio_pin3;
    tilt_pins[3] = gpio_pin4;

    // Cấu hình từng chân GPIO
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    for (int i = 0; i < 4; i++) {
        io_conf.pin_bit_mask = (1ULL << tilt_pins[i]);
        gpio_config(&io_conf);
    }
}

bool tilt_sensor_is_tilted(void) {
    // SW-520D: mức thấp (0) khi nghiêng, mức cao (1) khi không nghiêng
    // Kiểm tra từng cảm biến, nếu có 1 cảm biến nào nghiêng (0) thì trả về true
    for (int i = 2; i < 4; i++) {
        if (gpio_get_level(tilt_pins[i]) == 0) {
            return true;
        }
    }
    return false; // Nếu không cảm biến nào nghiêng, trả về false
}