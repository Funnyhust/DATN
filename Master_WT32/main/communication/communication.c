#include "communication.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/semphr.h"
#include "common_data.h"

static const char* TAG = "Master";

static void send_ack(uint8_t node_id) {
    ESP_LOGI(TAG, "Sending ACK to node %d", node_id);
    lora_send_packet(&node_id, 1);
    ESP_LOGI(TAG, "Sent ACK to node %d", node_id);
}

// Gửi gói tin đồng bộ hóa
static void send_sync(uint8_t node_id, SlaveStatus *slaves) {
    ESP_LOGI(TAG, "Sending Sync to node %d", node_id);
SyncPacket sync = {
        .sync_id = SYNC_ID,
        .time_to_next_round = 0
    };

    int64_t current_time_ms = esp_timer_get_time() / 1000;
    //ktra nut truoc co hoat dong khong
    if (slaves[(node_id + 1) % MAX_SLAVES].active) {
        int64_t time_diff = (current_time_ms - slaves[(node_id + 1) % MAX_SLAVES].last_time_sync) % 30000;
        sync.time_to_next_round = (10000 - time_diff) >= 0 ? (10000 - time_diff) : 0;
    } 
    //ktra nut sau co hoat dong khong
    else if (slaves[node_id % MAX_SLAVES].active) {
        int64_t time_diff = (current_time_ms - slaves[node_id % MAX_SLAVES].last_time_sync) % 30000;
        sync.time_to_next_round = (20000 - time_diff) >= 0 ? (10000 - time_diff) : 0;
    }
    lora_send_packet((uint8_t*)&sync, sizeof(sync));
    ESP_LOGI(TAG, "Sent Sync");
}

void communication_init() {
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
        lora_set_tx_power(20);
        lora_receive();
    }
}

void communication_task(void *pvParameters) {

    SlaveStatus *slaves = (SlaveStatus *)pvParameters;
    while (1) {
       // ESP_LOGI(TAG, "Checking for LoRa packet");
            if (xSemaphoreTake(slaves_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                // update slaves

        if (lora_received()) {
            Packet pkt;
            int len = lora_receive_packet((uint8_t*)&pkt, sizeof(pkt));
            ESP_LOGI(TAG, "Received packet of length %d", len);
            if (len == sizeof(pkt) && pkt.node_id > 0 && pkt.node_id <= MAX_SLAVES) {
                ESP_LOGI(TAG, "Received from node %d: rain_count=%lu, soil=%d, tilt=%d, battery=%d",
                         pkt.node_id, pkt.rain_count, pkt.soil_moisture, pkt.tilt_status, pkt.battery_level);

                SlaveStatus* slave = &slaves[pkt.node_id - 1];
                slave->node_id = pkt.node_id;
                slave->rain_count += pkt.rain_count;
                slave->last_soil_moisture = pkt.soil_moisture;
                slave->last_tilt_status = pkt.tilt_status;
                slave->last_battery_level = pkt.battery_level;
                slave->missed_rounds = 0;
                // Kiểm tra độ ẩm đất và gửi ACK hoặc Sync
                if (pkt.soil_moisture < 3000) {
                    slave->active = true;// Giảm delay xuống 1 giây để test nhanh hơn
                    slave->ack_or_sync = 0;
                    send_ack(pkt.node_id);
                } else {
                    slave->active = true; // Giảm delay xuống 1 giây để test nhanh hơn
                    slave->last_time_sync = esp_timer_get_time()/1000;
                    slave->ack_or_sync =1;
                    if(!pkt.is_sync){
                    vTaskDelay(pdMS_TO_TICKS(1500));
                    send_sync(pkt.node_id,slaves);
                }
                }
                lora_receive(); // Đặt lại chế độ nhận sau khi xử lý gói tin hợp lệ
            } else {
                ESP_LOGW(TAG, "Invalid packet received: len=%d, node_id=%d", len, pkt.node_id);
                lora_receive(); // Đặt lại chế độ nhận nếu gói tin không hợp lệ
            }
        } 
                    xSemaphoreGive(slaves_mutex);
            }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}