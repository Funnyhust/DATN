#ifndef BUZZER_H
#define BUZZER_H

#include "driver/gpio.h"
#include "driver/ledc.h"

// Định nghĩa chân và cấu hình PWM
#define BUZZER_PIN GPIO_NUM_35       // Chân GPIO cho buzzer
#define PWM_CHANNEL LEDC_CHANNEL_0   // Kênh PWM
#define PWM_TIMER LEDC_TIMER_0      // Timer PWM
#define PWM_RESOLUTION LEDC_TIMER_8_BIT // Độ phân giải 8 bit

// Hàm khởi tạo buzzer
void buzzer_init(void);

// Hàm bật/tắt buzzer (cho active buzzer)
void buzzer_on(void);
void buzzer_off(void);

// Hàm phát một nốt nhạc (cho passive buzzer)
void buzzer_play_note(int freq, int duration_ms);

#endif // BUZZER_H