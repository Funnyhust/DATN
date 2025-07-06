#ifndef COMMON_DATA_H
#define COMMON_DATA_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "communication/communication.h"  // file có struct SlaveStatus
#define MAX_SLAVES 3

extern SlaveStatus slaves[MAX_SLAVES];
extern SemaphoreHandle_t slaves_mutex;
#endif