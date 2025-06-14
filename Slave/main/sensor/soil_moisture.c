#include "soil_moisture.h"
#include "esp_log.h"

static const char *TAG = "SOIL_MOISTURE";

// Hàm khởi tạo cảm biến
esp_err_t soil_moisture_init(soil_moisture_sensor_t *sensor, adc_channel_t adc_channel) {
    if (sensor == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    sensor->adc_channel = adc_channel;
    sensor->dry_value = 4095;  // Giá trị mặc định khi đất khô
    sensor->wet_value = 0;     // Giá trị mặc định khi đất ướt

    ESP_LOGI(TAG, "Soil moisture sensor initialized on ADC2 channel %d", adc_channel);
    return ESP_OK;
}

// Hàm đọc giá trị ADC thô
esp_err_t soil_moisture_read_raw(soil_moisture_sensor_t *sensor, int *adc_value) {
    if (sensor == NULL || adc_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Khởi tạo ADC2 oneshot
    adc_oneshot_unit_handle_t adc2_handle;
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_2,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc2_handle));
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, sensor->adc_channel, &chan_cfg));

    // Đọc giá trị ADC
    ESP_ERROR_CHECK(adc_oneshot_read(adc2_handle, sensor->adc_channel, adc_value));
   // ESP_LOGI(TAG, "Soil moisture ADC raw value: %d", *adc_value);

    // Giải phóng ADC
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc2_handle));
    return ESP_OK;
}

// Hàm đọc giá trị độ ẩm (phần trăm)
esp_err_t soil_moisture_read(soil_moisture_sensor_t *sensor, float *moisture_percentage) {
    if (sensor == NULL || moisture_percentage == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Khởi tạo ADC2 oneshot
    adc_oneshot_unit_handle_t adc2_handle;
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_2,  // Đã sửa lại chỗ này
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc2_handle));
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, sensor->adc_channel, &chan_cfg));

    // Đọc giá trị ADC
    int adc_value;
    ESP_ERROR_CHECK(adc_oneshot_read(adc2_handle, sensor->adc_channel, &adc_value));

    // Chuyển đổi thành phần trăm
    *moisture_percentage = ((float)(sensor->dry_value - adc_value) / (sensor->dry_value - sensor->wet_value)) * 100.0;
    if (*moisture_percentage < 0.0) *moisture_percentage = 0.0;
    else if (*moisture_percentage > 100.0) *moisture_percentage = 100.0;

    ESP_LOGI(TAG, "Soil moisture: %.2f%% (ADC raw: %d)", *moisture_percentage, adc_value);

    // Giải phóng ADC
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc2_handle));
    return ESP_OK;
}
