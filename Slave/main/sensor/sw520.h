#ifndef SW520_H
#define SW520_H

#include <stdint.h>
#include <stdbool.h>

// Hàm khởi tạo cảm biến nghiêng
void tilt_sensor_init(uint8_t gpio_pin1, uint8_t gpio_pin2, uint8_t gpio_pin3, uint8_t gpio_pin4);

// Hàm đọc trạng thái nghiêng
// Trả về: true nếu nghiêng, false nếu không
bool tilt_sensor_is_tilted(void);

#endif // TILT_SENSOR_H