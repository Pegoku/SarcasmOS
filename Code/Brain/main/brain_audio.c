#include "brain_audio.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define AUDIO_SAMPLE_RATE 32000
#define AUDIO_UPLOAD_SAMPLE_RATE 16000
#define AUDIO_BLOCK_FRAMES 128
#define AUDIO_TIMEOUT_MS 250
#define AUDIO_PREROLL_SAMPLES 8000

#define PIN_I2S_BCLK GPIO_NUM_39
#define PIN_I2S_LRCLK GPIO_NUM_40
#define PIN_I2S_DOUT GPIO_NUM_41
#define PIN_I2S_DIN GPIO_NUM_47

static i2s_chan_handle_t s_tx_channel;
static i2s_chan_handle_t s_rx_channel;
static SemaphoreHandle_t s_audio_mutex;

static uint16_t magnitude16(int16_t sample)
{
    return sample == INT16_MIN ? INT16_MAX :
           (uint16_t)(sample < 0 ? -sample : sample);
}

static esp_err_t audio_lock(void)
{
    if (s_audio_mutex == NULL || s_tx_channel == NULL ||
        s_rx_channel == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return xSemaphoreTake(s_audio_mutex, portMAX_DELAY) == pdTRUE
               ? ESP_OK : ESP_FAIL;
}

esp_err_t brain_audio_init(void)
{
    if (s_tx_channel != NULL || s_rx_channel != NULL) {
        return ESP_OK;
    }
    s_audio_mutex = xSemaphoreCreateMutex();
    if (s_audio_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    i2s_chan_config_t channel_config =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    esp_err_t err = i2s_new_channel(
        &channel_config, &s_tx_channel, &s_rx_channel);
    if (err != ESP_OK) {
        return err;
    }
    i2s_std_config_t standard_config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = PIN_I2S_BCLK,
            .ws = PIN_I2S_LRCLK,
            .dout = PIN_I2S_DOUT,
            .din = PIN_I2S_DIN,
            .invert_flags = { 0 },
        },
    };
    err = i2s_channel_init_std_mode(s_tx_channel, &standard_config);
    if (err == ESP_OK) {
        err = i2s_channel_init_std_mode(s_rx_channel, &standard_config);
    }
    if (err == ESP_OK) {
        err = i2s_channel_enable(s_rx_channel);
    }
    if (err == ESP_OK) {
        err = i2s_channel_enable(s_tx_channel);
    }
    return err;
}

static esp_err_t write_pcm16(const int16_t *samples, size_t sample_count,
                             uint16_t gain_q8)
{
    if (samples == NULL || sample_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    int32_t output[AUDIO_BLOCK_FRAMES * 2];
    size_t offset = 0;
    while (offset < sample_count) {
        size_t count = sample_count - offset;
        if (count > AUDIO_BLOCK_FRAMES) {
            count = AUDIO_BLOCK_FRAMES;
        }
        for (size_t i = 0; i < count; ++i) {
            int32_t amplified =
                ((int32_t)samples[offset + i] * gain_q8) / 256;
            if (amplified > INT16_MAX) amplified = INT16_MAX;
            if (amplified < INT16_MIN) amplified = INT16_MIN;
            int32_t frame = amplified << 16;
            output[i * 2] = frame;
            output[i * 2 + 1] = frame;
        }
        size_t bytes_written = 0;
        esp_err_t err = i2s_channel_write(
            s_tx_channel, output, count * 2 * sizeof(int32_t),
            &bytes_written, pdMS_TO_TICKS(AUDIO_TIMEOUT_MS));
        if (err != ESP_OK ||
            bytes_written != count * 2 * sizeof(int32_t)) {
            return err == ESP_OK ? ESP_ERR_INVALID_SIZE : err;
        }
        offset += count;
    }
    return ESP_OK;
}

esp_err_t brain_audio_play_pcm16(const int16_t *samples, size_t sample_count,
                                 uint16_t gain_q8)
{
    if (samples == NULL || sample_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = audio_lock();
    if (err == ESP_OK) {
        err = write_pcm16(samples, sample_count, gain_q8);
        xSemaphoreGive(s_audio_mutex);
    }
    return err;
}

esp_err_t brain_audio_play_tone(uint16_t frequency_hz, uint16_t duration_ms,
                                uint16_t level)
{
    if (frequency_hz == 0 || duration_ms == 0 || level == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = audio_lock();
    if (err != ESP_OK) {
        return err;
    }
    int16_t samples[AUDIO_BLOCK_FRAMES];
    uint32_t phase = 0;
    const uint32_t step =
        (uint32_t)(((uint64_t)frequency_hz << 32) / AUDIO_SAMPLE_RATE);
    uint32_t remaining =
        ((uint32_t)duration_ms * AUDIO_SAMPLE_RATE) / 1000;
    while (remaining > 0 && err == ESP_OK) {
        size_t count = remaining > AUDIO_BLOCK_FRAMES
                           ? AUDIO_BLOCK_FRAMES : remaining;
        for (size_t i = 0; i < count; ++i) {
            samples[i] = (int16_t)(
                sin((double)phase * 6.283185307179586 / 4294967296.0) *
                level);
            phase += step;
        }
        err = write_pcm16(samples, count, 256);
        remaining -= count;
    }
    memset(samples, 0, sizeof(samples));
    write_pcm16(samples, AUDIO_BLOCK_FRAMES, 256);
    xSemaphoreGive(s_audio_mutex);
    return err;
}

static size_t extract_mic_samples(const int32_t *input, size_t frames,
                                  int16_t *output, bool decimate)
{
    uint64_t left_energy = 0;
    uint64_t right_energy = 0;
    for (size_t i = 0; i < frames; ++i) {
        int32_t left = input[i * 2] >> 16;
        int32_t right = input[i * 2 + 1] >> 16;
        left_energy += (uint32_t)(left < 0 ? -left : left);
        right_energy += (uint32_t)(right < 0 ? -right : right);
    }
    size_t channel = right_energy > left_energy ? 1 : 0;
    size_t output_count = 0;
    size_t step = decimate ? 2 : 1;
    for (size_t i = 0; i < frames; i += step) {
        output[output_count++] =
            (int16_t)(input[i * 2 + channel] >> 16);
    }
    return output_count;
}

static void update_metrics(const int16_t *samples, size_t count,
                           uint64_t *energy, uint32_t *sample_count,
                           uint16_t *peak)
{
    for (size_t i = 0; i < count; ++i) {
        uint16_t magnitude = magnitude16(samples[i]);
        if (magnitude > *peak) {
            *peak = magnitude;
        }
        *energy += (uint64_t)magnitude * magnitude;
        ++*sample_count;
    }
}

esp_err_t brain_audio_measure(uint16_t duration_ms,
                              brain_audio_capture_result_t *result)
{
    if (duration_ms == 0 || result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = audio_lock();
    if (err != ESP_OK) {
        return err;
    }
    memset(result, 0, sizeof(*result));
    int32_t input[AUDIO_BLOCK_FRAMES * 2];
    int16_t samples[AUDIO_BLOCK_FRAMES];
    uint64_t energy = 0;
    uint32_t wanted = (uint32_t)duration_ms * AUDIO_SAMPLE_RATE / 1000;
    while (result->sample_count < wanted) {
        size_t bytes_read = 0;
        err = i2s_channel_read(
            s_rx_channel, input, sizeof(input), &bytes_read,
            pdMS_TO_TICKS(AUDIO_TIMEOUT_MS));
        if (err != ESP_OK) {
            break;
        }
        size_t frames = bytes_read / (sizeof(int32_t) * 2);
        size_t count = extract_mic_samples(input, frames, samples, false);
        update_metrics(samples, count, &energy, &result->sample_count,
                       &result->peak);
    }
    if (result->sample_count > 0) {
        result->rms =
            (uint16_t)sqrt((double)energy / result->sample_count);
        result->duration_ms =
            result->sample_count * 1000 / AUDIO_SAMPLE_RATE;
    }
    xSemaphoreGive(s_audio_mutex);
    return err;
}

esp_err_t brain_audio_capture(const brain_audio_capture_config_t *config,
                              brain_audio_sink_t sink, void *context,
                              brain_audio_capture_result_t *result)
{
    if (config == NULL || sink == NULL || result == NULL ||
        config->vad_threshold == 0 || config->silence_ms == 0 ||
        config->max_recording_ms == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = audio_lock();
    if (err != ESP_OK) {
        return err;
    }

    memset(result, 0, sizeof(*result));
    int32_t input[AUDIO_BLOCK_FRAMES * 2];
    int16_t samples[AUDIO_BLOCK_FRAMES / 2];
    int16_t *preroll = malloc(
        AUDIO_PREROLL_SAMPLES * sizeof(*preroll));
    if (preroll == NULL) {
        xSemaphoreGive(s_audio_mutex);
        return ESP_ERR_NO_MEM;
    }
    size_t preroll_count = 0;
    size_t preroll_write = 0;
    uint64_t energy = 0;
    uint32_t silence_samples = 0;
    uint32_t waited_samples = 0;
    uint32_t max_wait_samples =
        (uint32_t)config->wait_for_voice_ms * AUDIO_UPLOAD_SAMPLE_RATE / 1000;
    uint32_t max_samples =
        (uint32_t)config->max_recording_ms * AUDIO_UPLOAD_SAMPLE_RATE / 1000;
    uint32_t silence_limit =
        (uint32_t)config->silence_ms * AUDIO_UPLOAD_SAMPLE_RATE / 1000;

    while (err == ESP_OK && result->sample_count < max_samples) {
        size_t bytes_read = 0;
        err = i2s_channel_read(
            s_rx_channel, input, sizeof(input), &bytes_read,
            pdMS_TO_TICKS(AUDIO_TIMEOUT_MS));
        if (err != ESP_OK) {
            break;
        }
        size_t frames = bytes_read / (sizeof(int32_t) * 2);
        size_t count = extract_mic_samples(input, frames, samples, true);
        uint16_t block_peak = 0;
        for (size_t i = 0; i < count; ++i) {
            uint16_t magnitude = magnitude16(samples[i]);
            if (magnitude > block_peak) block_peak = magnitude;
        }

        if (!result->speech_detected) {
            for (size_t i = 0; i < count; ++i) {
                preroll[preroll_write++] = samples[i];
                if (preroll_write == AUDIO_PREROLL_SAMPLES) {
                    preroll_write = 0;
                }
                if (preroll_count < AUDIO_PREROLL_SAMPLES) {
                    ++preroll_count;
                }
            }
            waited_samples += count;
            if (block_peak < config->vad_threshold) {
                if (max_wait_samples > 0 &&
                    waited_samples >= max_wait_samples) {
                    err = ESP_ERR_TIMEOUT;
                }
                continue;
            }
            result->speech_detected = true;
            size_t start =
                (preroll_write + AUDIO_PREROLL_SAMPLES - preroll_count) %
                AUDIO_PREROLL_SAMPLES;
            while (preroll_count > 0 && err == ESP_OK) {
                size_t contiguous = AUDIO_PREROLL_SAMPLES - start;
                if (contiguous > preroll_count) contiguous = preroll_count;
                err = sink(&preroll[start], contiguous, context);
                update_metrics(&preroll[start], contiguous, &energy,
                               &result->sample_count, &result->peak);
                preroll_count -= contiguous;
                start = 0;
            }
            continue;
        }

        err = sink(samples, count, context);
        update_metrics(samples, count, &energy, &result->sample_count,
                       &result->peak);
        silence_samples =
            block_peak < config->vad_threshold
                ? silence_samples + count : 0;
        if (silence_samples >= silence_limit) {
            break;
        }
    }
    if (result->sample_count > 0) {
        result->rms =
            (uint16_t)sqrt((double)energy / result->sample_count);
        result->duration_ms =
            result->sample_count * 1000 / AUDIO_UPLOAD_SAMPLE_RATE;
    }
    free(preroll);
    xSemaphoreGive(s_audio_mutex);
    return err;
}
