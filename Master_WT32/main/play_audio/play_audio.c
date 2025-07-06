#include "play_audio.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_spiffs.h"



static i2s_chan_handle_t i2s_chan_out = NULL;

esp_err_t play_audio_init(void)
{
    if (i2s_chan_out) {
        // Đã init rồi thì không cần init lại
        return ESP_OK;
    }

    i2s_chan_config_t chan_cfg = {
        .id = I2S_PORT_OUT,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 8,
        .dma_frame_num = PLAY_BUFFER_LEN,
        .auto_clear = false,
    };

    esp_err_t err = i2s_new_channel(&chan_cfg, &i2s_chan_out, NULL);
    if (err != ESP_OK) {
        // ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(err));
        return err;
    }

    i2s_std_config_t i2s_config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(PLAY_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(PLAY_BITS_PER_SAMPLE, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = PLAY_I2S_BCK,
            .ws   = PLAY_I2S_WS,
            .dout = PLAY_I2S_DOUT,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {0}
        },
    };

    err = i2s_channel_init_std_mode(i2s_chan_out, &i2s_config);
    if (err != ESP_OK) {
        // ESP_LOGE(TAG, "i2s_channel_init_std_mode failed: %s", esp_err_to_name(err));
        return err;
    }

    err = i2s_channel_enable(i2s_chan_out);
    if (err != ESP_OK) {
        // ESP_LOGE(TAG, "i2s_channel_enable failed: %s", esp_err_to_name(err));
        return err;
    }

    // ESP_LOGI(TAG, "play_audio_init OK!");
    return ESP_OK;
}

esp_err_t play_audio_write(const int16_t *buffer, size_t samples)
{
    if (!i2s_chan_out) {
        // ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    size_t bytes_written = 0;
    esp_err_t err = i2s_channel_write(i2s_chan_out, (const void*)buffer, samples * sizeof(int16_t), &bytes_written, portMAX_DELAY);
    if (err != ESP_OK) {
        // ESP_LOGE(TAG, "i2s_channel_write failed: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

esp_err_t play_audio_deinit(void)
{
    if (i2s_chan_out) {
        i2s_channel_disable(i2s_chan_out);
        i2s_del_channel(i2s_chan_out);
        i2s_chan_out = NULL;
    }
    return ESP_OK;
}
esp_err_t mount_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    // Có thể thêm log ở đây nếu muốn
    // if (ret != ESP_OK) ESP_LOGE("SPIFFS", "Failed to mount SPIFFS (%d)", ret);

    return ret;
}
esp_err_t play_wav(const char *path, float gain)
{
    // Đảm bảo đã init I2S out trước khi gọi hàm này!
    FILE *f = fopen(path, "rb");
    if (!f) {
        return ESP_FAIL;
    }

    // Bỏ qua header WAV (44 byte)
    fseek(f, 44, SEEK_SET);

    uint8_t buffer[512];
    size_t bytes_read;
    size_t samples;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        // Xử lý gain digital nếu cần
        int16_t *samples16 = (int16_t *)buffer;
        samples = bytes_read / 2;
        if (gain > 1.01f || gain < 0.99f) {
            for (size_t i = 0; i < samples; i++) {
                int32_t temp = samples16[i] * gain;
                if (temp > 32767) temp = 32767;
                if (temp < -32768) temp = -32768;
                samples16[i] = (int16_t)temp;
            }
        }
        play_audio_write(samples16, samples);
    }

    fclose(f); // Đợi một chút để đảm bảo phát xong
    return ESP_OK;
}
void play_wav_with_fadein(const char *path, float gain) {
    FILE *f = fopen(path, "rb");
    if (!f) return;

    fseek(f, 44, SEEK_SET);
    uint8_t buffer[512];
    size_t bytes_read;
    int fade_block = 4; // Số block đầu tiên fade-in

    for (int block = 0; (bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0; block++) {
        int16_t *samples = (int16_t *)buffer;
        size_t ns = bytes_read / 2;
        float cur_gain = gain;
        if (block < fade_block) cur_gain *= (float)(block + 1) / fade_block;
        for (size_t i = 0; i < ns; i++) {
            int32_t tmp = samples[i] * cur_gain;
            if (tmp > 32767) tmp = 32767;
            if (tmp < -32768) tmp = -32768;
            samples[i] = (int16_t)tmp;
        }
        play_audio_write(samples, ns);
    }
    fclose(f);
}
