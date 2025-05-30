#ifndef BUTTON_H
#define BUTTON_H

#include <esp_err.h>
#include <driver/gpio.h>

typedef enum {
    PRESSED,
    RELEASED
} btn_event_e;

// Callback nhận sự kiện và ID nút
typedef void (*button_event_callback_t)(btn_event_e, uint8_t button_id);

esp_err_t button_init(void);
esp_err_t button_add(gpio_num_t gpio, button_event_callback_t callback, uint8_t *button_id);
esp_err_t button_remove(uint8_t button_id);

#endif // BUTTON_H