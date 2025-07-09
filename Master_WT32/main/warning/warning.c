#include "warning.h"
#include "rgb_led/rgb_led.h"
#include "play_audio/play_audio.h"
#include "esp_log.h"

static const char* TAG = "Master";

void warning_task(void *pvParameters) {
    warning_param_t *param = (warning_param_t *)pvParameters;
    uint8_t *warning_level = param->warning_level;
    RGB_LED *led = param->led;

    uint8_t last_level = 0;

    ESP_LOGI(TAG, "Task canh bao dang chay (khong buzzer)");

    while (1) {


            switch (*warning_level) {
                case 1:
                    ESP_LOGI(TAG, "Cảnh báo 1: Phát canh_bao1.wav và chớp đèn xanh lá");
                    rgb_led_green(led);
                    play_wav("/spiffs/canh_bao1.wav", 1.0f);
  //                  rgb_led_blink(led, 1, 0, 1, 500, 3); // xanh lá
                    break;

                case 2:
                    ESP_LOGI(TAG, "Cảnh báo 2: Phát canh_bao2.wav và chớp đèn vàng");
                    rgb_led_yellow(led);
                    play_wav("/spiffs/canh_bao2.wav", 1.0f);
  //                  rgb_led_blink(led, 0, 0, 1, 300, 4); // vàng
                    break;

                case 3:
                    ESP_LOGI(TAG, "Cảnh báo 3: Phát canh_bao3.wav và chớp đèn đỏ");
                    rgb_led_red(led);
                    play_wav("/spiffs/canh_bao3.wav", 1.0f);
                    rgb_led_blink(led, 0, 1, 1, 200, 5); // đỏ
                    break;

                default:
                    // không cảnh báo, tắt đèn
                    gpio_set_level(led->red_pin, 1);
                    gpio_set_level(led->green_pin, 1);
                    gpio_set_level(led->blue_pin, 1);
                   // ESP_LOGI(TAG, "Không có cảnh báo, tắt đèn RGB");
                    break;
            }

        // Giảm tải CPU
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
