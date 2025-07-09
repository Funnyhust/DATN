#ifndef WARNING_H
#define WARNING_H

#include "communication/communication.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "rgb_led/rgb_led.h"

typedef struct {
    uint8_t *warning_level;
    RGB_LED *led;
} warning_param_t;

void warning_init(void);
void warning_task(void *pvParameters);

#endif // WARNING_H