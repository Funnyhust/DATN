#include "buzzer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void buzzer_init(void) {
    // Cấu hình GPIO cho buzzer
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUZZER_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // Cấu hình timer PWM cho passive buzzer
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = PWM_RESOLUTION,
        .freq_hz = 1000, // Tần số ban đầu
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = PWM_TIMER
    };
    ledc_timer_config(&ledc_timer);

    // Cấu hình kênh PWM
    ledc_channel_config_t ledc_channel = {
        .channel = PWM_CHANNEL,
        .duty = 0,
        .gpio_num = BUZZER_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = PWM_TIMER
    };
    ledc_channel_config(&ledc_channel);
}

void buzzer_on(void) {
    gpio_set_level(BUZZER_PIN, 1); // Bật buzzer (active buzzer)
}

void buzzer_off(void) {
    gpio_set_level(BUZZER_PIN, 0); // Tắt buzzer
    ledc_set_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL, 0); // Đảm bảo PWM tắt
    ledc_update_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL);
}

void buzzer_play_note(int freq, int duration_ms) {
    ledc_set_freq(LEDC_LOW_SPEED_MODE, PWM_TIMER, freq); // Đặt tần số
    ledc_set_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL, 128); // Duty cycle 50%
    ledc_update_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL); // Cập nhật PWM
    vTaskDelay(duration_ms / portTICK_PERIOD_MS); // Chờ thời gian phát nốt
    ledc_set_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL, 0); // Tắt PWM
    ledc_update_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL);
    vTaskDelay(100 / portTICK_PERIOD_MS); // Nghỉ giữa các nốt
}