#include "button.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define MAX_BUTTONS 4 // Tối đa 4 nút
static const char *TAG = "BUTTON";

// Cấu trúc cho một nút
typedef struct {
    gpio_num_t gpio;                  // GPIO của nút
    button_event_callback_t callback;  // Callback
    bool last_state;                  // Trạng thái trước (true: thả, false: nhấn)
    uint8_t id;                       // ID nút (0, 1, 2, 3)
    bool active;                      // Trạng thái nút (true: đang dùng)
} button_t;

// Mảng tĩnh lưu nút
static button_t buttons[MAX_BUTTONS];
static TaskHandle_t button_task_handle = NULL;

static void button_task(void *pvParam) {
    while (1) {
        for (int i = 0; i < MAX_BUTTONS; i++) {
            if (buttons[i].active) {
                bool current_state = gpio_get_level(buttons[i].gpio);
                if (buttons[i].last_state && !current_state) { // Nhấn (falling edge)
                    if (buttons[i].callback) {
                        buttons[i].callback(PRESSED, buttons[i].id); // Gọi callback với ID
                    }
                    vTaskDelay(pdMS_TO_TICKS(100)); // Debounce 100ms
                }
                buttons[i].last_state = current_state;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Đọc mỗi 10ms
    }
}
// Khởi tạo hệ thống nút
esp_err_t button_init(void) {
    // Khởi tạo mảng nút
    for (int i = 0; i < MAX_BUTTONS; i++) {
        buttons[i].active = false;
        buttons[i].gpio = GPIO_NUM_MAX; // Không dùng
        buttons[i].callback = NULL;
        buttons[i].last_state = true;
        buttons[i].id = i;
    }

    // Tạo task
    if (button_task_handle == NULL) {
        BaseType_t task_created = xTaskCreate(button_task, "Button Task", 2048, NULL, 5, &button_task_handle);
        if (task_created != pdPASS) {
            ESP_LOGE(TAG, "Failed to create button task");
            return ESP_FAIL;
        }
    }
    ESP_LOGI(TAG, "Button system initialized");
    return ESP_OK;
}

// Thêm nút mới
esp_err_t button_add(gpio_num_t gpio, button_event_callback_t callback, uint8_t *button_id) {
    if (callback == NULL) {
        ESP_LOGE(TAG, "Callback is NULL");
        return ESP_FAIL;
    }

    // Tìm slot trống
    int slot = -1;
    for (int i = 0; i < MAX_BUTTONS; i++) {
        if (!buttons[i].active) {
            slot = i;
            break;
        }
    }
    if (slot == -1) {
        ESP_LOGE(TAG, "No free button slots");
        return ESP_FAIL;
    }

    // Kiểm tra GPIO trùng
    for (int i = 0; i < MAX_BUTTONS; i++) {
        if (buttons[i].active && buttons[i].gpio == gpio) {
            ESP_LOGE(TAG, "GPIO %d already in use", gpio);
            return ESP_FAIL;
        }
    }

    // Cấu hình GPIO
    esp_err_t err = gpio_reset_pin(gpio);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset GPIO %d: %d", gpio, err);
        return err;
    }
    err = gpio_set_direction(gpio, GPIO_MODE_INPUT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set GPIO %d direction: %d", gpio, err);
        return err;
    }
    err = gpio_set_pull_mode(gpio, GPIO_PULLUP_ONLY);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set GPIO %d pull mode: %d", gpio, err);
        return err;
    }

    // Cập nhật nút
    buttons[slot].gpio = gpio;
    buttons[slot].callback = callback;
    buttons[slot].last_state = true;
    buttons[slot].active = true;
    *button_id = buttons[slot].id; // Trả về ID nút

    ESP_LOGI(TAG, "Button %d added on GPIO %d", buttons[slot].id, gpio);
    return ESP_OK;
}

// Xóa nút
esp_err_t button_remove(uint8_t button_id) {
    if (button_id >= MAX_BUTTONS) {
        ESP_LOGE(TAG, "Invalid button ID %d", button_id);
        return ESP_FAIL;
    }

    if (!buttons[button_id].active) {
        ESP_LOGE(TAG, "Button %d not active", button_id);
        return ESP_FAIL;
    }

    buttons[button_id].active = false;
    buttons[button_id].gpio = GPIO_NUM_MAX;
    buttons[button_id].callback = NULL;
    ESP_LOGI(TAG, "Button %d removed", button_id);
    return ESP_OK;
}

// Task xử lý nút
