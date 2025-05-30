#include "sw520.h"
#include "driver/gpio.h"

static uint8_t tilt_pin;

void tilt_sensor_init(uint8_t gpio_pin) {
    tilt_pin = gpio_pin;
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

bool tilt_sensor_is_tilted(void) {
    // SW-520D: mức thấp (0) khi nghiêng, mức cao (1) khi không nghiêng
    return gpio_get_level(tilt_pin) == 0;
}