#ifndef SOIL_MOISTURE_H
#define SOIL_MOISTURE_H

#include <stdint.h>
#include "esp_adc/adc_oneshot.h"

// Định nghĩa cấu trúc cho cảm biến độ ẩm đất
typedef struct {
    adc_channel_t adc_channel;  // Kênh ADC được sử dụng
    int dry_value;              // Giá trị ADC khi đất khô hoàn toàn (mặc định: 4095)
    int wet_value;              // Giá trị ADC khi đất ướt hoàn toàn (mặc định: 0)
} soil_moisture_sensor_t;

// Hàm khởi tạo cảm biến
esp_err_t soil_moisture_init(soil_moisture_sensor_t *sensor, adc_channel_t adc_channel);

// Hàm đọc giá trị độ ẩm (phần trăm)
esp_err_t soil_moisture_read(soil_moisture_sensor_t *sensor, float *moisture_percentage);

// Hàm đọc giá trị ADC thô
esp_err_t soil_moisture_read_raw(soil_moisture_sensor_t *sensor, int *adc_value);

#endif // SOIL_MOISTURE_H