#ifndef TILT_SENSOR_H
#define TILT_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

// Hàm khởi tạo cảm biến nghiêng
void tilt_sensor_init(uint8_t gpio_pin);

// Hàm đọc trạng thái nghiêng
// Trả về: true nếu nghiêng, false nếu không
bool tilt_sensor_is_tilted(void);

#endif // TILT_SENSOR_H