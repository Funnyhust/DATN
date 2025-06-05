#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

// Hàm đọc điện áp pin (mV)
esp_err_t battery_read(adc_channel_t adc_channel, uint16_t *battery_mv);

#endif // BATTERY_H