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
    uint16_t sequence;
    uint16_t soil_moisture;
    uint8_t tilt_status;
    uint16_t battery_level;
    uint16_t count;
} Packet;

typedef struct {
    uint8_t sync_id;
    uint8_t count_Sync;
    uint16_t count_Round;
    uint32_t timestamp;
} SyncPacket;

typedef struct {
    uint8_t node_id;
    bool active;
    uint16_t last_soil_moisture;
    uint8_t last_tilt_status;
    uint16_t last_battery_level;
    uint16_t last_count;
    uint32_t last_seen;
    uint8_t missed_rounds;
} SlaveStatus;

extern SlaveStatus slaves[MAX_SLAVES];
extern uint16_t current_round;

// void send_ack(uint8_t node_id);
// void send_sync(uint8_t count_Sync, uint16_t current_round);
void communication_task(void *pvParameters);

#endif // COMMUNICATION_H