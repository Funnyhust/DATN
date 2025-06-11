#include "communication.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "Master";

static void send_ack(uint8_t node_id) {
    ESP_LOGI(TAG, "Sending ACK to node %d", node_id);
    lora_send_packet(&node_id, 1);
    ESP_LOGI(TAG, "Sent ACK to node %d", node_id);
}

static void send_sync(uint8_t count_Sync, uint16_t current_round) {
    SyncPacket sync = {
        .sync_id = SYNC_ID,
        .count_Sync = count_Sync,
        .count_Round = current_round,
        .timestamp = esp_timer_get_time() / 1000000
    };
    ESP_LOGI(TAG, "Sending Sync: round=%d, count_Sync=%d, timestamp=%" PRIu32,
             sync.count_Round, sync.count_Sync, sync.timestamp);
    lora_send_packet((uint8_t*)&sync, sizeof(sync));
    ESP_LOGI(TAG, "Sent Sync");
}

void communication_init(SlaveStatus* slaves, uint16_t* current_round) {
    if (lora_init() != 1) {
        ESP_LOGE(TAG, "Khoi tao LoRa that bai");
    } else {
        ESP_LOGI(TAG, "Khoi tao LoRa thanh cong");
        lora_set_frequency(433000000);
        lora_set_spreading_factor(7);
        lora_set_bandwidth(125E3);
        lora_set_coding_rate(5);
        lora_enable_crc();
        lora_set_preamble_length(12);
        lora_set_sync_word(0x34);
        lora_set_tx_power(17);
        lora_receive();
    }

    for (int i = 0; i < MAX_SLAVES; i++) {
        slaves[i].node_id = i + 1;
    }
    *current_round = 0;
}

void communication_task(void *pvParameters) {
    SlaveStatus* slaves = ((SlaveStatus**)pvParameters)[0];
    uint16_t* current_round = ((uint16_t**)pvParameters)[1];

    while (1) {
        if (lora_received()) {
            Packet pkt;
            int len = lora_receive_packet((uint8_t*)&pkt, sizeof(pkt));
            ESP_LOGI(TAG, "Received packet of length %d", len);
            if (len == sizeof(pkt) && pkt.node_id > 0 && pkt.node_id <= MAX_SLAVES) {
                ESP_LOGI(TAG, "Received from node %d: sequence=%d, soil=%d, tilt=%d, battery=%d, count=%d",
                         pkt.node_id, pkt.sequence, pkt.soil_moisture, pkt.tilt_status, pkt.battery_level, pkt.count);

                SlaveStatus* slave = &slaves[pkt.node_id - 1];
                slave->node_id = pkt.node_id;
                slave->active = true;
                slave->last_soil_moisture = pkt.soil_moisture;
                slave->last_tilt_status = pkt.tilt_status;
                slave->last_battery_level = pkt.battery_level;
                slave->last_count = pkt.count;
                slave->last_seen = esp_timer_get_time() / 1000000;
                slave->missed_rounds = 0;

                if (pkt.soil_moisture < 3276) {
                    vTaskDelay(pdMS_TO_TICKS(5000));
                    send_ack(pkt.node_id);
                } else {
                    if (pkt.sequence == 0) {
                        ESP_LOGI(TAG, "Received first packet from node %d, sending ACK", pkt.node_id);
                        send_ack(pkt.node_id);
                    } else {
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        for (int i = 1; i <= 3; i++) {
                            send_sync(i, *current_round);
                            vTaskDelay(pdMS_TO_TICKS(SYNC_INTERVAL_MS));
                        }
                    }
                }

                (*current_round)++;
                lora_receive();
            } else {
                ESP_LOGW(TAG, "Invalid packet received: len=%d, node_id=%d", len, pkt.node_id);
                lora_receive();
            }
        }

        if (*current_round >= MAX_ROUNDS) {
            ESP_LOGI(TAG, "Reached max rounds (%d), sending Sync for reset", MAX_ROUNDS);
            for (int i = 1; i <= 3; i++) {
                send_sync(i, *current_round);
                vTaskDelay(pdMS_TO_TICKS(SYNC_INTERVAL_MS));
            }
            *current_round = 0;
            lora_receive();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}