
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "buzzer/buzzer.h"

void app_main(void) {
    buzzer_init(); // Khởi tạo buzzer

    while (1) {
        // Điều khiển active buzzer
        buzzer_on();
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Bật 1 giây
        buzzer_off();
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Tắt 1 giây

        // Điều khiển passive buzzer (phát nốt C4, D4, E4)
        buzzer_play_note(261, 500); // Nốt C4 (261 Hz)
        buzzer_play_note(294, 500); // Nốt D4 (294 Hz)
        buzzer_play_note(330, 500); // Nốt E4 (330 Hz)
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Chờ trước khi lặp
    }
}