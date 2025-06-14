#include "battery.h"
#include "esp_log.h"

static const char *TAG = "BATTERY";

// Hàm đọc điện áp pin qua ADC
esp_err_t battery_read(adc_channel_t adc_channel, uint16_t *battery_mv) {
    if (battery_mv == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Khởi tạo ADC oneshot
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc1_handle));
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, adc_channel, &chan_cfg));

    // Đọc giá trị ADC
    int adc_val;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, adc_channel, &adc_val));

    // Chuyển đổi sang mV: (ADC * Vref / max_adc) * 2 (do mạch phân áp)
    *battery_mv = (adc_val * 3300 / 4095) * 2;
    //ESP_LOGI(TAG, "Battery ADC raw: %d, mV: %d", adc_val, *battery_mv);

    // Giải phóng ADC
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
    return ESP_OK;
}
