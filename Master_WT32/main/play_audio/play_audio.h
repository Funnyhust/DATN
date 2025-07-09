#ifndef PLAY_AUDIO_H
#define PLAY_AUDIO_H

#include <driver/i2s_std.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "esp_spiffs.h"


#ifdef __cplusplus
extern "C" {
#endif

#define PLAY_SAMPLE_RATE   15000 // hoặc 16000 nếu muốn
#define PLAY_BITS_PER_SAMPLE I2S_DATA_BIT_WIDTH_16BIT
#define I2S_PORT_OUT         I2S_NUM_1  // Khác với I2S_PORT của mic
#define PLAY_BUFFER_LEN      512

// GPIO mapping (sửa cho đúng mạch bạn)
#define PLAY_I2S_BCK         5
#define PLAY_I2S_WS          25
#define PLAY_I2S_DOUT        26

esp_err_t play_audio_init(void);
esp_err_t play_audio_write(const int16_t *buffer, size_t samples);
esp_err_t play_audio_deinit(void);
esp_err_t mount_spiffs(void);
esp_err_t play_wav(const char *path, float gain);
void play_wav_with_fadein(const char *path, float gain);
#ifdef __cplusplus
}
#endif

#endif // PLAY_AUDIO_H
