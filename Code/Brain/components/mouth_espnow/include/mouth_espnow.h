#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t mac[6];
    uint8_t peer_channel;
    uint32_t ack_timeout_ms;
    uint8_t retries;
} mouth_espnow_config_t;

typedef struct {
    uint8_t mac[6];
    bool initialized;
    bool present;
    uint8_t sequence;
    uint8_t result;
    uint8_t command;
    uint8_t application_version;
    uint8_t firmware_major;
    uint8_t firmware_minor;
    uint8_t current_animation;
    uint8_t brightness;
    uint8_t speaking_intensity;
    uint8_t channel;
    uint8_t transition_token;
    bool transition_active;
    uint8_t transition_progress;
    uint8_t consecutive_failures;
    uint32_t retry_count;
    uint32_t timeout_count;
    uint32_t last_ack_ms;
} mouth_espnow_status_t;

bool mouth_espnow_parse_mac(const char *text, uint8_t mac[6]);
esp_err_t mouth_espnow_init(const mouth_espnow_config_t *config);
esp_err_t mouth_espnow_send(uint8_t command, const uint8_t *payload,
                            uint8_t payload_length, bool wait_for_ack);
bool mouth_espnow_is_present(void);
void mouth_espnow_get_status(mouth_espnow_status_t *status);

#ifdef __cplusplus
}
#endif
