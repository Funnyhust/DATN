#include "communication.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "Slave";

void communication_init(uint8_t node_id) {
    if (lora_init() != 1) {
        ESP_LOGE(TAG, "Khoi tao LoRa that bai");
    } else {
        ESP_LOGI(TAG, "Khoi tao LoRa thanh cong cho node %d", node_id);
        lora_set_frequency(433000000);
        lora_set_spreading_factor(7);
        lora_set_bandwidth(125E3);
        lora_set_coding_rate(5);
        lora_enable_crc();
        lora_set_preamble_length(12);
        lora_set_sync_word(0x34);
        lora_set_tx_power(20);
        lora_receive();
    }
}

bool send_packet_with_ack(Packet* pkt) {
    ESP_LOGI(TAG, "Sending ACK packet: node_id=%d, rain_count=%lu, soil_moisture=%d, tilt_status=%d, battery_level=%d",
             pkt->node_id, pkt->rain_count, pkt->soil_moisture,
             pkt->tilt_status, pkt->battery_level);

        lora_send_packet((uint8_t*)pkt, sizeof(Packet));
        lora_receive();

        uint32_t start_time = esp_timer_get_time() / 1000;
        while (esp_timer_get_time() / 1000 - start_time < 5000) {
            if (lora_received()) {
                uint8_t ack_id;
                int len = lora_receive_packet(&ack_id, sizeof(ack_id));
                if (len == 1 && ack_id == pkt->node_id) {
                    ESP_LOGI(TAG, "Received ACK for node %d", pkt->node_id);
                    lora_receive();
                    return true;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        ESP_LOGW(TAG, "No ACK received, retrying...");
    lora_receive();
    return false;
}
bool send_packet_with_sync(Packet* pkt, uint32_t *time_to_next_round) {
    ESP_LOGI(TAG, "Sending Sync packet: node_id=%d, rain_count=%lu, soil_moisture=%d, tilt_status=%d, battery_level=%d",
             pkt->node_id, pkt->rain_count, pkt->soil_moisture,
             pkt->tilt_status, pkt->battery_level);

        lora_send_packet((uint8_t*)pkt, sizeof(Packet));
        lora_receive();

        uint32_t start_time = esp_timer_get_time() / 1000;
        while (esp_timer_get_time() / 1000 - start_time < 7000) {
            if (lora_received()) {
                SyncPacket Sync_pkt;
                int len = lora_receive_packet((uint8_t*)&Sync_pkt, sizeof(Sync_pkt));
                if (len == sizeof(Sync_pkt) && Sync_pkt.sync_id ==  SYNC_ID) {
                    ESP_LOGI(TAG, "Received Sync for node %d", pkt->node_id);
                    *time_to_next_round = Sync_pkt.time_to_next_round;
                    lora_receive();

                    return true;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        ESP_LOGW(TAG, "No Sync received, retrying...");
    lora_receive();
    return false;
}

// void sync_with_master(Packet* initial_pkt, bool* is_synced, uint16_t* broadcast_count, uint32_t* last_sync_time) {
//     ESP_LOGI(TAG, "Sending initial packet and waiting for Sync");
//     if (send_packet_with_ack(initial_pkt)) {
//         uint32_t start_time = esp_timer_get_time() / 1000;
//         while (esp_timer_get_time() / 1000 - start_time < 20000) {
//             if (lora_received()) {
//                 SyncPacket sync;
//                 int len = lora_receive_packet((uint8_t*)&sync, sizeof(sync));
//                 if (len == sizeof(sync) && sync.sync_id == SYNC_ID && sync.count_Sync == 3) {
//                     ESP_LOGI(TAG, "Received Sync: count_Sync=%d, round=%d, timestamp=%" PRIu32,
//                              sync.count_Sync, sync.count_Round, sync.timestamp);
//                     *is_synced = true;
//                     *broadcast_count = sync.count_Round;
//                     *last_sync_time = esp_timer_get_time() / 1000;
//                     lora_receive();
//                     return;
//                 }
//             }
//             vTaskDelay(pdMS_TO_TICKS(10));
//         }
//         ESP_LOGW(TAG, "No Sync received within 20s");
//     } else {
//         ESP_LOGW(TAG, "Failed to send initial packet");
//     }
//     *is_synced = false;
//     lora_receive();
// }