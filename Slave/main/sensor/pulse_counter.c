#include "pulse_counter.h"
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

// Biến toàn cục để đếm tổng số xung
static volatile uint32_t totalPulseCount = 0;
static const char *TAG = "PULSE_COUNTER";

// Hàm ngắt đếm xung
static void IRAM_ATTR pulseCounter(void *arg) {
    totalPulseCount++;
}

void pulse_counter_init(gpio_num_t pin) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << pin),
        .pull_down_en = 0,
        .pull_up_en = 1
    };
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(pin, pulseCounter, NULL);
    ESP_LOGI(TAG, "Pulse counter initialized on GPIO %d", pin);
}

uint32_t pulse_counter_get_total_count(void) {
    return totalPulseCount;
}

void pulse_counter_reset(void) {
    totalPulseCount = 0;
}