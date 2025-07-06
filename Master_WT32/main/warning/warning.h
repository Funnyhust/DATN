#ifndef WARNING_H
#define WARNING_H

#include "communication/communication.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define BUZZER_PIN GPIO_NUM_32

void warning_init(void);
void warning_task(void *pvParameters);

#endif // WARNING_H