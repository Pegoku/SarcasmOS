#include "mouth_espnow.h"

#include <stdio.h>
#include <string.h>

#include "display_protocol.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define MOUTH_ACK_BIT BIT0
#define MOUTH_RX_QUEUE_LENGTH 8

typedef struct {
    uint8_t source[6];
    uint8_t data[DISPLAY_PROTOCOL_MAX_PACKET_SIZE];
    uint8_t length;
} mouth_rx_message_t;

static mouth_espnow_config_t g_config;
static mouth_espnow_status_t g_status;
static QueueHandle_t g_rx_queue;
static EventGroupHandle_t g_ack_events;
static SemaphoreHandle_t g_command_mutex;
static SemaphoreHandle_t g_status_mutex;
static uint8_t g_next_sequence = 1;
static volatile uint8_t g_expected_sequence;
static volatile uint8_t g_expected_command;

static bool mac_is_valid_unicast(const uint8_t mac[6])
{
    static const uint8_t zero[6] = { 0 };
    return mac != NULL && memcmp(mac, zero, sizeof(zero)) != 0 &&
           (mac[0] & 0x01) == 0;
}

bool mouth_espnow_parse_mac(const char *text, uint8_t mac[6])
{
    if (text == NULL || mac == NULL) {
        return false;
    }

    unsigned values[6];
    char tail;
    int parsed = sscanf(text, "%2x:%2x:%2x:%2x:%2x:%2x%c",
                        &values[0], &values[1], &values[2],
                        &values[3], &values[4], &values[5], &tail);
    if (parsed != 6) {
        return false;
    }
    for (size_t i = 0; i < 6; ++i) {
        mac[i] = (uint8_t)values[i];
    }
    return mac_is_valid_unicast(mac);
}

static void record_timeout(void)
{
    xSemaphoreTake(g_status_mutex, portMAX_DELAY);
    ++g_status.timeout_count;
    xSemaphoreGive(g_status_mutex);
}

static void mark_command_unanswered(void)
{
    xSemaphoreTake(g_status_mutex, portMAX_DELAY);
    if (g_status.consecutive_failures < UINT8_MAX) {
        ++g_status.consecutive_failures;
    }
    if (g_status.consecutive_failures >= 3) {
        g_status.present = false;
    }
    xSemaphoreGive(g_status_mutex);
}

static void receive_callback(const esp_now_recv_info_t *info,
                             const uint8_t *data, int length)
{
    if (info == NULL || info->src_addr == NULL || data == NULL ||
        length < 1 || length > DISPLAY_PROTOCOL_MAX_PACKET_SIZE ||
        memcmp(info->src_addr, g_config.mac, 6) != 0 || g_rx_queue == NULL) {
        return;
    }

    mouth_rx_message_t message = { .length = (uint8_t)length };
    memcpy(message.source, info->src_addr, sizeof(message.source));
    memcpy(message.data, data, length);
    xQueueSend(g_rx_queue, &message, 0);
}

static void send_callback(const wifi_tx_info_t *info,
                          esp_now_send_status_t status)
{
    (void)info;
    (void)status;
}

static void receive_task(void *argument)
{
    (void)argument;
    mouth_rx_message_t message;
    while (true) {
        if (xQueueReceive(g_rx_queue, &message, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        display_protocol_packet_t packet;
        if (!display_protocol_decode(
                message.data, message.length, DISPLAY_PROTOCOL_TYPE_STATUS,
                DISPLAY_PROTOCOL_ROLE_MOUTH, &packet)) {
            continue;
        }
        bool current_status =
            packet.payload_length == DISPLAY_PROTOCOL_STATUS_PAYLOAD_SIZE &&
            packet.payload[0] == DISPLAY_PROTOCOL_APPLICATION_VERSION;
        bool legacy_status =
            packet.payload_length == DISPLAY_PROTOCOL_STATUS_PAYLOAD_LEGACY_SIZE &&
            packet.payload[0] == DISPLAY_PROTOCOL_APPLICATION_VERSION_LEGACY;
        if ((!current_status && !legacy_status) ||
            packet.payload[1] != DISPLAY_PROTOCOL_ROLE_MOUTH ||
            packet.payload[5] != packet.sequence) {
            continue;
        }

        xSemaphoreTake(g_status_mutex, portMAX_DELAY);
        g_status.present = true;
        g_status.sequence = packet.sequence;
        g_status.result = packet.payload[6];
        g_status.command = packet.command;
        g_status.application_version = packet.payload[0];
        g_status.firmware_major = packet.payload[2];
        g_status.firmware_minor = packet.payload[3];
        g_status.current_animation = packet.payload[4];
        g_status.brightness = packet.payload[7];
        g_status.speaking_intensity = packet.payload[8];
        g_status.channel = packet.payload[9];
        if (packet.payload_length == DISPLAY_PROTOCOL_STATUS_PAYLOAD_SIZE) {
            g_status.transition_token = packet.payload[10];
            g_status.transition_active = packet.payload[11] != 0;
            g_status.transition_progress = packet.payload[12];
        } else {
            g_status.transition_token = 0;
            g_status.transition_active = false;
            g_status.transition_progress = 255;
        }
        g_status.consecutive_failures = 0;
        g_status.last_ack_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
        xSemaphoreGive(g_status_mutex);

        if (packet.sequence == g_expected_sequence &&
            packet.command == g_expected_command) {
            xEventGroupSetBits(g_ack_events, MOUTH_ACK_BIT);
        }
    }
}

esp_err_t mouth_espnow_init(const mouth_espnow_config_t *config)
{
    if (config == NULL || !mac_is_valid_unicast(config->mac) ||
        config->ack_timeout_ms == 0 || config->retries == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (g_status.initialized) {
        return memcmp(config->mac, g_config.mac, 6) == 0
                   ? ESP_OK : ESP_ERR_INVALID_STATE;
    }

    g_rx_queue = xQueueCreate(MOUTH_RX_QUEUE_LENGTH, sizeof(mouth_rx_message_t));
    g_ack_events = xEventGroupCreate();
    g_command_mutex = xSemaphoreCreateMutex();
    g_status_mutex = xSemaphoreCreateMutex();
    if (g_rx_queue == NULL || g_ack_events == NULL ||
        g_command_mutex == NULL || g_status_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    memcpy(&g_config, config, sizeof(g_config));
    memcpy(g_status.mac, config->mac, sizeof(g_status.mac));

    esp_err_t err = esp_now_init();
    if (err != ESP_OK) {
        return err;
    }
    err = esp_now_register_recv_cb(receive_callback);
    if (err == ESP_OK) {
        err = esp_now_register_send_cb(send_callback);
    }

    esp_now_peer_info_t peer = {
        .channel = config->peer_channel,
        .ifidx = WIFI_IF_STA,
        .encrypt = false,
    };
    memcpy(peer.peer_addr, config->mac, sizeof(peer.peer_addr));
    if (err == ESP_OK) {
        err = esp_now_add_peer(&peer);
        if (err == ESP_ERR_ESPNOW_EXIST) {
            err = ESP_OK;
        }
    }
    if (err != ESP_OK) {
        return err;
    }

    if (xTaskCreate(receive_task, "mouth_espnow_rx", 4096, NULL, 6, NULL) !=
        pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    g_status.initialized = true;
    return ESP_OK;
}

esp_err_t mouth_espnow_send(uint8_t command, const uint8_t *payload,
                            uint8_t payload_length, bool wait_for_ack)
{
    if (!g_status.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(g_command_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }

    uint8_t sequence = g_next_sequence++;
    if (g_next_sequence == 0) {
        g_next_sequence = 1;
    }
    uint8_t packet[DISPLAY_PROTOCOL_MAX_PACKET_SIZE];
    size_t length = display_protocol_encode(
        packet, sizeof(packet), DISPLAY_PROTOCOL_TYPE_COMMAND,
        DISPLAY_PROTOCOL_ROLE_MOUTH, sequence, command, payload, payload_length);
    if (length == 0) {
        xSemaphoreGive(g_command_mutex);
        return ESP_ERR_INVALID_ARG;
    }

    g_expected_sequence = sequence;
    g_expected_command = command;
    xEventGroupClearBits(g_ack_events, MOUTH_ACK_BIT);
    esp_err_t result = ESP_ERR_TIMEOUT;
    uint8_t attempts = wait_for_ack ? g_config.retries : 1;
    for (uint8_t attempt = 0; attempt < attempts; ++attempt) {
        esp_err_t err = esp_now_send(g_config.mac, packet, length);
        if (err != ESP_OK) {
            result = err;
        } else if (!wait_for_ack) {
            result = ESP_OK;
            break;
        } else {
            EventBits_t bits = xEventGroupWaitBits(
                g_ack_events, MOUTH_ACK_BIT, pdTRUE, pdFALSE,
                pdMS_TO_TICKS(g_config.ack_timeout_ms));
            if ((bits & MOUTH_ACK_BIT) != 0) {
                mouth_espnow_status_t status;
                mouth_espnow_get_status(&status);
                result = status.result == DISPLAY_ERROR_NONE
                             ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
                break;
            }
            record_timeout();
        }

        if (attempt + 1 < attempts) {
            xSemaphoreTake(g_status_mutex, portMAX_DELAY);
            ++g_status.retry_count;
            xSemaphoreGive(g_status_mutex);
        }
    }

    if (result != ESP_OK && result != ESP_ERR_INVALID_RESPONSE) {
        mark_command_unanswered();
    }
    g_expected_sequence = 0;
    g_expected_command = 0;
    xSemaphoreGive(g_command_mutex);
    return result;
}

bool mouth_espnow_is_present(void)
{
    if (!g_status.initialized) {
        return false;
    }
    xSemaphoreTake(g_status_mutex, portMAX_DELAY);
    bool present = g_status.present;
    xSemaphoreGive(g_status_mutex);
    return present;
}

void mouth_espnow_get_status(mouth_espnow_status_t *status)
{
    if (status == NULL) {
        return;
    }
    if (g_status_mutex == NULL) {
        memset(status, 0, sizeof(*status));
        return;
    }
    xSemaphoreTake(g_status_mutex, portMAX_DELAY);
    memcpy(status, &g_status, sizeof(*status));
    xSemaphoreGive(g_status_mutex);
}
