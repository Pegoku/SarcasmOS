#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define BRAIN_CONFIG_WIFI_SSID_SIZE 33
#define BRAIN_CONFIG_WIFI_PASSWORD_SIZE 65
#define BRAIN_CONFIG_URL_SIZE 192
#define BRAIN_CONFIG_TOKEN_SIZE 192
#define BRAIN_CONFIG_WAKE_PHRASE_SIZE 64

typedef struct {
    char wifi_ssid[BRAIN_CONFIG_WIFI_SSID_SIZE];
    char wifi_password[BRAIN_CONFIG_WIFI_PASSWORD_SIZE];
    char workflow_url[BRAIN_CONFIG_URL_SIZE];
    char workflow_token[BRAIN_CONFIG_TOKEN_SIZE];
    char wake_phrase[BRAIN_CONFIG_WAKE_PHRASE_SIZE];
    bool wake_enabled;
    uint16_t silence_ms;
    uint16_t vad_threshold;
} brain_config_t;

typedef enum {
    BRAIN_CONFIG_WIFI_SSID,
    BRAIN_CONFIG_WIFI_PASSWORD,
    BRAIN_CONFIG_WORKFLOW_URL,
    BRAIN_CONFIG_WORKFLOW_TOKEN,
    BRAIN_CONFIG_WAKE_PHRASE,
} brain_config_string_key_t;

esp_err_t brain_config_load(brain_config_t *config);
esp_err_t brain_config_set_string(brain_config_t *config,
                                  brain_config_string_key_t key,
                                  const char *value);
esp_err_t brain_config_set_wake_enabled(brain_config_t *config, bool enabled);
esp_err_t brain_config_set_silence_ms(brain_config_t *config,
                                      uint16_t silence_ms);
esp_err_t brain_config_set_vad_threshold(brain_config_t *config,
                                         uint16_t threshold);
esp_err_t brain_config_reset(void);

