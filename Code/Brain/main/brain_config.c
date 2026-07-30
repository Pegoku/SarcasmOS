#include "brain_config.h"

#include <string.h>

#include "nvs.h"
#include "sdkconfig.h"

#define CONFIG_NAMESPACE "brain_cfg"

typedef struct {
    const char *nvs_key;
    char *destination;
    size_t capacity;
} config_string_t;

static void copy_default(char *destination, size_t capacity,
                         const char *value)
{
    if (capacity == 0) {
        return;
    }
    strlcpy(destination, value != NULL ? value : "", capacity);
}

static config_string_t config_string(brain_config_t *config,
                                     brain_config_string_key_t key)
{
    switch (key) {
    case BRAIN_CONFIG_WIFI_SSID:
        return (config_string_t){
            "wifi_ssid", config->wifi_ssid, sizeof(config->wifi_ssid)
        };
    case BRAIN_CONFIG_WIFI_PASSWORD:
        return (config_string_t){
            "wifi_pass", config->wifi_password, sizeof(config->wifi_password)
        };
    case BRAIN_CONFIG_WORKFLOW_URL:
        return (config_string_t){
            "workflow_url", config->workflow_url, sizeof(config->workflow_url)
        };
    case BRAIN_CONFIG_WORKFLOW_TOKEN:
        return (config_string_t){
            "workflow_tok", config->workflow_token,
            sizeof(config->workflow_token)
        };
    case BRAIN_CONFIG_WAKE_PHRASE:
        return (config_string_t){
            "wake_phrase", config->wake_phrase, sizeof(config->wake_phrase)
        };
    case BRAIN_CONFIG_AI_TOKEN:
        return (config_string_t){
            "ai_token", config->ai_token, sizeof(config->ai_token)
        };
    case BRAIN_CONFIG_LLM_TOKEN:
        return (config_string_t){
            "llm_token", config->llm_token, sizeof(config->llm_token)
        };
    case BRAIN_CONFIG_REPLICATE_TOKEN:
        return (config_string_t){
            "repl_token", config->replicate_token,
            sizeof(config->replicate_token)
        };
    case BRAIN_CONFIG_LLM_URL:
        return (config_string_t){
            "llm_url", config->llm_url, sizeof(config->llm_url)
        };
    case BRAIN_CONFIG_REPLICATE_URL:
        return (config_string_t){
            "repl_url", config->replicate_url,
            sizeof(config->replicate_url)
        };
    case BRAIN_CONFIG_LLM_MODEL:
        return (config_string_t){
            "llm_model", config->llm_model, sizeof(config->llm_model)
        };
    case BRAIN_CONFIG_STT_MODEL:
        return (config_string_t){
            "stt_model", config->stt_model, sizeof(config->stt_model)
        };
    case BRAIN_CONFIG_TTS_MODEL:
        return (config_string_t){
            "tts_model", config->tts_model, sizeof(config->tts_model)
        };
    case BRAIN_CONFIG_VOICE_ID:
        return (config_string_t){
            "voice_id", config->voice_id, sizeof(config->voice_id)
        };
    case BRAIN_CONFIG_GOOGLE_CALENDAR_TOKEN:
        return (config_string_t){
            "gcal_token", config->google_calendar_token,
            sizeof(config->google_calendar_token)
        };
    case BRAIN_CONFIG_TIMEZONE:
        return (config_string_t){
            "timezone", config->timezone, sizeof(config->timezone)
        };
    default:
        return (config_string_t){ NULL, NULL, 0 };
    }
}

static void load_string(nvs_handle_t handle, config_string_t entry)
{
    if (entry.nvs_key == NULL || entry.destination == NULL) {
        return;
    }
    size_t size = entry.capacity;
    if (nvs_get_str(handle, entry.nvs_key, entry.destination, &size) != ESP_OK) {
        return;
    }
    entry.destination[entry.capacity - 1] = '\0';
}

esp_err_t brain_config_load(brain_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(config, 0, sizeof(*config));
    copy_default(config->wifi_ssid, sizeof(config->wifi_ssid),
                 CONFIG_SARCASMOS_WIFI_SSID);
    copy_default(config->wifi_password, sizeof(config->wifi_password),
                 CONFIG_SARCASMOS_WIFI_PASSWORD);
    copy_default(config->workflow_url, sizeof(config->workflow_url),
                 CONFIG_SARCASMOS_WORKFLOW_URL);
    copy_default(config->workflow_token, sizeof(config->workflow_token),
                 CONFIG_SARCASMOS_WORKFLOW_TOKEN);
    copy_default(config->wake_phrase, sizeof(config->wake_phrase),
                 CONFIG_SARCASMOS_WAKE_PHRASE);
    copy_default(config->ai_token, sizeof(config->ai_token),
                 CONFIG_SARCASMOS_AI_TOKEN);
    copy_default(config->llm_token, sizeof(config->llm_token),
                 CONFIG_SARCASMOS_LLM_TOKEN);
    copy_default(config->replicate_token, sizeof(config->replicate_token),
                 CONFIG_SARCASMOS_REPLICATE_TOKEN);
    copy_default(config->llm_url, sizeof(config->llm_url),
                 CONFIG_SARCASMOS_LLM_URL);
    copy_default(config->replicate_url, sizeof(config->replicate_url),
                 CONFIG_SARCASMOS_REPLICATE_URL);
    copy_default(config->llm_model, sizeof(config->llm_model),
                 CONFIG_SARCASMOS_LLM_MODEL);
    copy_default(config->stt_model, sizeof(config->stt_model),
                 CONFIG_SARCASMOS_STT_MODEL);
    copy_default(config->tts_model, sizeof(config->tts_model),
                 CONFIG_SARCASMOS_TTS_MODEL);
    copy_default(config->voice_id, sizeof(config->voice_id),
                 CONFIG_SARCASMOS_VOICE_ID);
    copy_default(config->google_calendar_token,
                 sizeof(config->google_calendar_token),
                 CONFIG_SARCASMOS_GOOGLE_CALENDAR_TOKEN);
    copy_default(config->timezone, sizeof(config->timezone),
                 CONFIG_SARCASMOS_TIMEZONE);
#ifdef CONFIG_SARCASMOS_WAKE_ENABLED
    config->wake_enabled = true;
#else
    config->wake_enabled = false;
#endif
    config->silence_ms = CONFIG_SARCASMOS_LISTEN_SILENCE_MS;
    config->vad_threshold = CONFIG_SARCASMOS_VAD_THRESHOLD;

    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(CONFIG_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    for (brain_config_string_key_t key = BRAIN_CONFIG_WIFI_SSID;
         key < BRAIN_CONFIG_STRING_COUNT; ++key) {
        load_string(handle, config_string(config, key));
    }
    uint8_t wake_enabled = config->wake_enabled;
    uint16_t silence_ms = config->silence_ms;
    uint16_t vad_threshold = config->vad_threshold;
    if (nvs_get_u8(handle, "wake_enabled", &wake_enabled) == ESP_OK) {
        config->wake_enabled = wake_enabled != 0;
    }
    if (nvs_get_u16(handle, "silence_ms", &silence_ms) == ESP_OK) {
        config->silence_ms = silence_ms;
    }
    if (nvs_get_u16(handle, "vad_threshold", &vad_threshold) == ESP_OK) {
        config->vad_threshold = vad_threshold;
    }
    nvs_close(handle);
    return ESP_OK;
}

esp_err_t brain_config_set_string(brain_config_t *config,
                                  brain_config_string_key_t key,
                                  const char *value)
{
    if (config == NULL || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    config_string_t entry = config_string(config, key);
    if (entry.nvs_key == NULL || strlen(value) >= entry.capacity) {
        return ESP_ERR_INVALID_SIZE;
    }

    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, entry.nvs_key, value);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    if (err == ESP_OK) {
        copy_default(entry.destination, entry.capacity, value);
    }
    if (handle != 0) {
        nvs_close(handle);
    }
    return err;
}

static esp_err_t set_u16(uint16_t *destination, const char *key,
                         uint16_t value)
{
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_u16(handle, key, value);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    if (err == ESP_OK) {
        *destination = value;
    }
    if (handle != 0) {
        nvs_close(handle);
    }
    return err;
}

esp_err_t brain_config_set_wake_enabled(brain_config_t *config, bool enabled)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, "wake_enabled", enabled ? 1 : 0);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    if (err == ESP_OK) {
        config->wake_enabled = enabled;
    }
    if (handle != 0) {
        nvs_close(handle);
    }
    return err;
}

esp_err_t brain_config_set_silence_ms(brain_config_t *config,
                                      uint16_t silence_ms)
{
    if (config == NULL || silence_ms < 500 || silence_ms > 15000) {
        return ESP_ERR_INVALID_ARG;
    }
    return set_u16(&config->silence_ms, "silence_ms", silence_ms);
}

esp_err_t brain_config_set_vad_threshold(brain_config_t *config,
                                         uint16_t threshold)
{
    if (config == NULL || threshold == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return set_u16(&config->vad_threshold, "vad_threshold", threshold);
}

esp_err_t brain_config_reset(void)
{
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_erase_all(handle);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    if (handle != 0) {
        nvs_close(handle);
    }
    return err;
}
