#include "warning.h"
#include "esp_log.h"

static const char* TAG = "Master";

void warning_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUZZER_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Khoi tao GPIO cho buzzer that bai: %s", esp_err_to_name(ret));
    } else {
        gpio_set_level(BUZZER_PIN, 0); // Tắt buzzer ban đầu
        ESP_LOGI(TAG, "Khoi tao buzzer thanh cong tai GPIO %d", BUZZER_PIN);
    }
}

void warning_task(void *pvParameters) {
    uint8_t* warning_level = (uint8_t*)pvParameters;

    ESP_LOGI(TAG, "Khoi tao task dieu khien buzzer");
    while (1) {
        switch (*warning_level) {
            case 1:
                gpio_set_level(BUZZER_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(1000));
                gpio_set_level(BUZZER_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
            case 2:
                gpio_set_level(BUZZER_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(500));
                gpio_set_level(BUZZER_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
            case 3:
                gpio_set_level(BUZZER_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(250));
                gpio_set_level(BUZZER_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(250));
                break;
            default:
                gpio_set_level(BUZZER_PIN, 0); // Tắt buzzer khi không có cảnh báo
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
        }
    }
}