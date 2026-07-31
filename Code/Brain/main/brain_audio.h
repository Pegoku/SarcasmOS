#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef esp_err_t (*brain_audio_sink_t)(const int16_t *samples,
                                        size_t sample_count,
                                        void *context);
typedef bool (*brain_audio_continue_t)(void *context);

typedef struct {
    uint16_t vad_threshold;
    uint16_t silence_ms;
    uint16_t max_recording_ms;
    uint16_t wait_for_voice_ms;
} brain_audio_capture_config_t;

typedef struct {
    uint32_t sample_count;
    uint32_t duration_ms;
    uint16_t peak;
    uint16_t rms;
    bool speech_detected;
} brain_audio_capture_result_t;

esp_err_t brain_audio_init(void);
esp_err_t brain_audio_set_mic_gain_q8(uint16_t gain_q8);
uint16_t brain_audio_get_mic_gain_q8(void);
esp_err_t brain_audio_set_speaker_gain_q8(uint16_t gain_q8);
uint16_t brain_audio_get_speaker_gain_q8(void);
esp_err_t brain_audio_play_tone(uint16_t frequency_hz, uint16_t duration_ms,
                                uint16_t speaker_gain_q8);
esp_err_t brain_audio_measure(uint16_t duration_ms,
                              brain_audio_capture_result_t *result);
esp_err_t brain_audio_capture(const brain_audio_capture_config_t *config,
                              brain_audio_sink_t sink, void *context,
                              brain_audio_capture_result_t *result);
esp_err_t brain_audio_stream(brain_audio_sink_t sink,
                             brain_audio_continue_t should_continue,
                             void *context);
esp_err_t brain_audio_play_pcm16(const int16_t *samples, size_t sample_count,
                                 uint16_t gain_q8);
