#ifndef PULSE_COUNTER_H
#define PULSE_COUNTER_H

#include <stdint.h>
#include <driver/gpio.h>

// Khai báo hàm khởi tạo
void pulse_counter_init(gpio_num_t pin);

// Lấy tổng số xung đã đếm
uint32_t pulse_counter_get_total_count(void);

// Đặt lại tổng số xung về 0
void pulse_counter_reset(void);

#endif // PULSE_COUNTER_H