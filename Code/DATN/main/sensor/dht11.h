#ifndef DHT11_H
#define DHT11_H

#include <stdint.h>
#include <stdbool.h>

// Hàm khởi tạo cảm biến độ ẩm
void dht11_init(uint8_t gpio_pin);

// Hàm đọc giá trị độ ẩm
// Trả về: giá trị độ ẩm (%), hoặc -1 nếu lỗi
float dht11_read_humidity(bool *error, char *error_msg, size_t msg_len);

#endif // DHT11_H