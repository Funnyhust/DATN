#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lora/lora.h"
#include "esp_timer.h"

#define SYNC_ID 0xFF
#define MAX_SLAVES 3
#define MAX_ROUNDS 100
#define SYNC_INTERVAL_MS 5000

typedef struct {
    uint8_t node_id;
    uint16_t soil_moisture;
    uint8_t tilt_status;
    uint16_t battery_level;
    uint32_t rain_count;
    bool is_sync;
} Packet;

typedef struct {
    uint8_t sync_id;
    uint32_t time_to_next_round;
} SyncPacket;
// Trạng thái của các slave
typedef struct {
    uint8_t node_id;
    bool active;
    uint16_t last_soil_moisture;
    uint8_t  last_tilt_status;
    uint32_t rain_count;
    uint32_t last_rain_count;
    uint32_t last_rain_update_time;
    uint16_t last_battery_level;
    uint8_t missed_rounds;
    uint32_t last_time_sync;
    int8_t ack_or_sync;
    uint32_t last_time_ack;
} SlaveStatus;
void communication_task(void *pvParameters);
void communication_init();
#endif // COMMUNICATION_H