#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <stdint.h>
#include "lora/lora.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define NODE_ID 2
#define SYNC_ID 0xFF
#define MAX_BROADCAST_COUNT 100
#define SYNC_INTERVAL_MS 5000
#define MOISTURE_THRESHOLD 3276

typedef struct {
    uint8_t node_id;
    uint16_t rainfall;
    uint16_t soil_moisture;
    uint8_t tilt_status;
    uint16_t battery_level;
} Packet;

typedef struct {
    uint8_t sync_id;
    uint32_t time_to_next_sync;
} SyncPacket;

void communication_init(uint8_t node_id);
bool send_packet_with_ack(Packet* pkt);
bool send_packet_with_sync(Packet* pkt, uint32_t *time_to_next_sync);
// void sync_with_master(Packet* initial_pkt, bool* is_synced, uint16_t* broadcast_count, uint32_t* last_sync_time);

#endif // COMMUNICATION_H