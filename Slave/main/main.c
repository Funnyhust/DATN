#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "sensor/soil_moisture.h"
#include "sensor/sw520.h"
#include "sensor/battery.h"
#include "driver/adc.h"
#include "communication/communication.h"
#include "lora/lora.h"
#include "esp_timer.h"


#define BATTERY_MIN_MV 3300
#define SLEEP_24H_US (24ULL * 3600 * 1000000ULL)

#define SOIL_ADC_CHANNEL ADC2_CHANNEL_9
#define TILT_PIN1 33
#define TILT_PIN2 15
#define TILT_PIN3 23
#define TILT_PIN4 22
#define BATTERY_ADC_CHANNEL ADC_CHANNEL_4
uint32_t last_time_send=0;

uint32_t time_to_next_round = 0;

static const char* TAG = "NodeSensor";

static soil_moisture_sensor_t soil_moisture;

uint16_t rainfall = 0; // Placeholder for rainfall data, if needed

static void read_sensors(Packet* pkt) {
    int adc_val;
    if (soil_moisture_read_raw(&soil_moisture, &adc_val) == ESP_OK) {
        pkt->soil_moisture = adc_val;
       // ESP_LOGI(TAG, "Read soil moisture: %d", adc_val);
    } else {
        pkt->soil_moisture = 0;
      //  ESP_LOGE(TAG, "Failed to read soil moisture");
    }

    pkt->tilt_status = tilt_sensor_is_tilted() ? 1 : 0;
   // ESP_LOGI(TAG, "Read tilt status: %d", pkt->tilt_status);

    if (battery_read(BATTERY_ADC_CHANNEL, &pkt->battery_level) == ESP_OK) {
       // ESP_LOGI(TAG, "Read battery level: %d mV", pkt->battery_level);
    } else {
        pkt->battery_level = 0;
       // ESP_LOGE(TAG, "Failed to read battery voltage");
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Node %d starting", NODE_ID);
    ESP_LOGI(TAG, "Khoi tao task giao tiep LoRa");
    communication_init(NODE_ID);

    if (soil_moisture_init(&soil_moisture, SOIL_ADC_CHANNEL) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize soil moisture sensor");
        return;
    }
    tilt_sensor_init(TILT_PIN1, TILT_PIN2, TILT_PIN3, TILT_PIN4);

    esp_sleep_enable_timer_wakeup(SLEEP_24H_US);

    while (1) {
        Packet pkt = {
            .node_id = NODE_ID,
            .is_sync =false,
        };
        read_sensors(&pkt);

//Nếu độ ẩm dưới ngưỡng, gửi gói tin và đi ngủ, lora ngủ 24h, esp32 ngủ, dậy mỗi 10s để đọc lại cảm biến
        if (pkt.soil_moisture < MOISTURE_THRESHOLD) {
            ESP_LOGI(TAG, "Soil dry (%d < %d), sending BT0", pkt.soil_moisture, MOISTURE_THRESHOLD);

            if(send_packet_with_ack(&pkt)){
                lora_sleep();
                ESP_LOGI(TAG, "Packet sent successfully, going to sleep for 24 hours");
                uint32_t start_time = esp_timer_get_time() / 1000;
                while (esp_timer_get_time() / 1000 - start_time < 86400) {
                    read_sensors(&pkt);
                    if (pkt.soil_moisture >= MOISTURE_THRESHOLD) {
                        lora_idle();
                        break; 
                    }
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_sleep_enable_timer_wakeup(10 * 1000000);  // Thiết lập wake-up sau 10 giây
                    esp_light_sleep_start(); 
                    ESP_LOGI(TAG, "Woke up from light sleep, checking soil moisture again");
                    continue;
                }
            }
            continue;
        } 

// Nếu độ ẩm trên ngưỡng, gửi gói tin và đợi đồng bộ với master
        else {
         if(send_packet_with_sync(&pkt,&time_to_next_round)) {
            pkt.is_sync=true;
            vTaskDelay(time_to_next_round); //
            uint32_t current_time = esp_timer_get_time() / 1000;
                while(1){
                read_sensors(&pkt);
                if (pkt.soil_moisture < MOISTURE_THRESHOLD) {
                     break;
                }
            
                else {
                    // ESP_LOGI(TAG, "Sending packet: node_id=%d, rainfall=%d, soil_moisture=%d, tilt_status=%d, battery_level=%d",
                    // pkt->node_id, pkt->rainfall, pkt->soil_moisture,
                    // pkt->tilt_status, pkt->battery_level);
                     if(pkt.tilt_status == 1){
                    ESP_LOGI(TAG, "Slave");
                    lora_send_packet((uint8_t*)&pkt, sizeof(Packet));
                    lora_receive();
                     }
                    else {
                        if(esp_timer_get_time()/1000 -last_time_send>30000){
                                ESP_LOGI(TAG, "Slave");
                                lora_send_packet((uint8_t*)&pkt, sizeof(Packet));
                                lora_receive();
                                last_time_send = esp_timer_get_time() / 1000;
                        }
                }
         }
         vTaskDelay(10);
        }
     
         Packet pkt = {
            .node_id = NODE_ID,
        };

        }
     }
 }
}
/*         while(1){
            ESP_LOGI(TAG, "Node %d is awake, reading sensors", NODE_ID);
            read_sensors(&pkt);
            if (lora_send_packet((uint8_t*)&pkt, sizeof(Packet)) == ESP_OK) {
        ESP_LOGI(TAG, "Packet sent successfully");
    } else {
        ESP_LOGE(TAG, "Failed to send packet");
    }       
            vTaskDelay(10); // Delay 10s + (NODE_ID - 1) * 10s
            lora_sleep();
            vTaskDelay(10);
            esp_sleep_enable_timer_wakeup(1 *5 * 100000); 
            vTaskDelay(10); // Thiết lập wake-up sau 10 giây
            esp_light_sleep_start(); 
         }*/
