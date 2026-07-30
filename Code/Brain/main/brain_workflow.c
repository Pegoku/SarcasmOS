#include "brain_workflow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "brain_audio.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define WORKFLOW_SAMPLE_RATE 16000
#define WORKFLOW_HTTP_TIMEOUT_MS 120000
#define WORKFLOW_LINE_CAPACITY 12288
#define WORKFLOW_MAX_URL 384
#define WORKFLOW_AUDIO_BUFFER 2048

typedef struct {
    esp_http_client_handle_t client;
    const brain_config_t *config;
    const char *wake_phrase;
    const char *robot_status_json;
    char endpoint[WORKFLOW_MAX_URL];
    bool opened;
} upload_context_t;

static brain_workflow_event_handler_t s_event_handler;
static void *s_event_context;
static SemaphoreHandle_t s_workflow_mutex;

static void emit_event(brain_workflow_event_type_t type, const char *message,
                       const char *tool, const char *expression,
                       bool has_temperature, int8_t temperature)
{
    if (s_event_handler == NULL) {
        return;
    }
    brain_workflow_event_t event = {
        .type = type,
        .message = message,
        .tool = tool,
        .expression = expression,
        .has_temperature = has_temperature,
        .temperature_c = temperature,
    };
    s_event_handler(&event, s_event_context);
}

static void emit_audio_level(uint8_t level)
{
    if (s_event_handler == NULL) return;
    brain_workflow_event_t event = {
        .type = BRAIN_WORKFLOW_EVENT_AUDIO_LEVEL,
        .audio_level = level,
    };
    s_event_handler(&event, s_event_context);
}

static esp_err_t play_audio_samples(const int16_t *samples,
                                    size_t sample_count)
{
    uint16_t peak = 0;
    for (size_t i = 0; i < sample_count; ++i) {
        int32_t sample = samples[i];
        uint16_t magnitude = (uint16_t)(
            sample == INT16_MIN ? INT16_MAX :
            (sample < 0 ? -sample : sample));
        if (magnitude > peak) peak = magnitude;
    }
    uint32_t scaled = (uint32_t)peak * 3 / 128;
    if (scaled > UINT8_MAX) scaled = UINT8_MAX;
    emit_audio_level((uint8_t)scaled);
    return brain_audio_play_pcm16(samples, sample_count, 256);
}

static esp_err_t build_url(char *output, size_t capacity,
                           const char *base, const char *path)
{
    if (output == NULL || base == NULL || path == NULL || base[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    size_t base_length = strlen(base);
    int written = snprintf(
        output, capacity, "%.*s%s", (int)(base_length -
            (base_length > 0 && base[base_length - 1] == '/')), base, path);
    return written > 0 && (size_t)written < capacity
               ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

static esp_http_client_handle_t create_http_client(const char *url)
{
    esp_http_client_config_t http_config = {
        .url = url,
        .timeout_ms = WORKFLOW_HTTP_TIMEOUT_MS,
        .buffer_size = WORKFLOW_AUDIO_BUFFER,
        .buffer_size_tx = WORKFLOW_AUDIO_BUFFER,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
    };
    return esp_http_client_init(&http_config);
}

static esp_err_t set_common_headers(esp_http_client_handle_t client,
                                    const brain_config_t *config)
{
    esp_err_t err = esp_http_client_set_header(
        client, "Content-Type", "application/octet-stream");
    if (err == ESP_OK) {
        err = esp_http_client_set_header(
            client, "X-Audio-Sample-Rate", "16000");
    }
    if (err == ESP_OK && config->workflow_token[0] != '\0') {
        char authorization[BRAIN_CONFIG_TOKEN_SIZE + 8];
        int length = snprintf(authorization, sizeof(authorization),
                              "Bearer %s", config->workflow_token);
        if (length <= 0 || (size_t)length >= sizeof(authorization)) {
            return ESP_ERR_INVALID_SIZE;
        }
        err = esp_http_client_set_header(
            client, "Authorization", authorization);
    }
    return err;
}

static esp_err_t open_upload(upload_context_t *upload)
{
    upload->client = create_http_client(upload->endpoint);
    if (upload->client == NULL) {
        return ESP_ERR_NO_MEM;
    }
    esp_http_client_set_method(upload->client, HTTP_METHOD_POST);
    esp_err_t err = set_common_headers(upload->client, upload->config);
    if (err == ESP_OK && upload->wake_phrase != NULL) {
        err = esp_http_client_set_header(
            upload->client, "X-Wake-Phrase", upload->wake_phrase);
    }
    if (err == ESP_OK && upload->robot_status_json != NULL &&
        upload->robot_status_json[0] != '\0') {
        err = esp_http_client_set_header(
            upload->client, "X-Robot-Status",
            upload->robot_status_json);
    }
    if (err == ESP_OK) {
        err = esp_http_client_open(upload->client, -1);
    }
    if (err == ESP_OK) {
        upload->opened = true;
    }
    return err;
}

static esp_err_t http_write_all(esp_http_client_handle_t client,
                                const void *data, size_t length)
{
    const char *bytes = data;
    while (length > 0) {
        int written = esp_http_client_write(client, bytes, (int)length);
        if (written <= 0) {
            return ESP_ERR_HTTP_WRITE_DATA;
        }
        bytes += written;
        length -= written;
    }
    return ESP_OK;
}

static esp_err_t upload_pcm_chunk(const int16_t *samples,
                                  size_t sample_count, void *context)
{
    upload_context_t *upload = context;
    if (!upload->opened) {
        esp_err_t err = open_upload(upload);
        if (err != ESP_OK) {
            return err;
        }
    }
    size_t byte_count = sample_count * sizeof(*samples);
    char chunk_header[20];
    int header_length = snprintf(
        chunk_header, sizeof(chunk_header), "%x\r\n", (unsigned)byte_count);
    if (header_length <= 0 || (size_t)header_length >= sizeof(chunk_header)) {
        return ESP_ERR_INVALID_SIZE;
    }
    esp_err_t err = http_write_all(
        upload->client, chunk_header, (size_t)header_length);
    if (err == ESP_OK) {
        err = http_write_all(upload->client, samples, byte_count);
    }
    if (err == ESP_OK) {
        err = http_write_all(upload->client, "\r\n", 2);
    }
    return err;
}

static esp_err_t finish_upload(upload_context_t *upload)
{
    return upload->opened
               ? http_write_all(upload->client, "0\r\n\r\n", 5)
               : ESP_ERR_INVALID_STATE;
}

static uint16_t read_u16_le(const uint8_t *bytes)
{
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

static uint32_t read_u32_le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static esp_err_t play_audio_url(const brain_config_t *config,
                                const char *path)
{
    char url[WORKFLOW_MAX_URL];
    esp_err_t err = build_url(url, sizeof(url), config->workflow_url, path);
    if (err != ESP_OK) {
        return err;
    }
    esp_http_client_handle_t client = create_http_client(url);
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (config->workflow_token[0] != '\0') {
        char authorization[BRAIN_CONFIG_TOKEN_SIZE + 8];
        snprintf(authorization, sizeof(authorization), "Bearer %s",
                 config->workflow_token);
        esp_http_client_set_header(client, "Authorization", authorization);
    }
    err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return err;
    }
    if (esp_http_client_fetch_headers(client) < 0 ||
        esp_http_client_get_status_code(client) != 200) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_HTTP_FETCH_HEADER;
    }

    uint8_t header[512];
    size_t header_length = 0;
    size_t data_offset = 0;
    uint16_t channels = 0;
    uint16_t bits = 0;
    uint32_t sample_rate = 0;
    while (header_length < sizeof(header) && data_offset == 0) {
        int received = esp_http_client_read(
            client, (char *)&header[header_length],
            sizeof(header) - header_length);
        if (received <= 0) {
            err = ESP_ERR_INVALID_RESPONSE;
            break;
        }
        header_length += received;
        if (header_length < 12 || memcmp(header, "RIFF", 4) != 0 ||
            memcmp(&header[8], "WAVE", 4) != 0) {
            continue;
        }
        size_t offset = 12;
        while (offset + 8 <= header_length) {
            uint32_t chunk_size = read_u32_le(&header[offset + 4]);
            if (memcmp(&header[offset], "fmt ", 4) == 0 &&
                chunk_size >= 16 && offset + 24 <= header_length) {
                if (read_u16_le(&header[offset + 8]) != 1) {
                    err = ESP_ERR_NOT_SUPPORTED;
                    break;
                }
                channels = read_u16_le(&header[offset + 10]);
                sample_rate = read_u32_le(&header[offset + 12]);
                bits = read_u16_le(&header[offset + 22]);
            } else if (memcmp(&header[offset], "data", 4) == 0) {
                data_offset = offset + 8;
                break;
            }
            size_t next = offset + 8 + chunk_size + (chunk_size & 1U);
            if (next > header_length) break;
            offset = next;
        }
    }
    if (err == ESP_OK &&
        (data_offset == 0 || channels != 1 || bits != 16 ||
         sample_rate != 32000)) {
        err = ESP_ERR_NOT_SUPPORTED;
    }
    union {
        int16_t samples[WORKFLOW_AUDIO_BUFFER / sizeof(int16_t)];
        uint8_t bytes[WORKFLOW_AUDIO_BUFFER];
    } audio;
    size_t pending = 0;
    if (err == ESP_OK && header_length > data_offset) {
        pending = header_length - data_offset;
        memcpy(audio.bytes, &header[data_offset], pending);
        size_t playable = pending & ~(size_t)1;
        if (playable > 0) {
            err = play_audio_samples(
                audio.samples, playable / sizeof(int16_t));
        }
        pending -= playable;
        if (pending) audio.bytes[0] = audio.bytes[playable];
    }
    while (err == ESP_OK) {
        int received = esp_http_client_read(
            client, (char *)&audio.bytes[pending],
            sizeof(audio.bytes) - pending);
        if (received == 0) break;
        if (received < 0) {
            err = ESP_FAIL;
            break;
        }
        size_t available = pending + received;
        size_t playable = available & ~(size_t)1;
        if (playable > 0) {
            err = play_audio_samples(
                audio.samples, playable / sizeof(int16_t));
        }
        pending = available - playable;
        if (pending) audio.bytes[0] = audio.bytes[playable];
    }
    if (err == ESP_OK && pending != 0) {
        err = ESP_ERR_INVALID_RESPONSE;
    }
    emit_audio_level(0);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return err;
}

static const char *json_value_start(const char *json, const char *key)
{
    char pattern[64];
    if (snprintf(pattern, sizeof(pattern), "\"%s\"", key) >=
        (int)sizeof(pattern)) {
        return NULL;
    }
    const char *position = strstr(json, pattern);
    if (position == NULL) return NULL;
    position += strlen(pattern);
    while (*position == ' ' || *position == '\t') ++position;
    if (*position++ != ':') return NULL;
    while (*position == ' ' || *position == '\t') ++position;
    return position;
}

static bool json_get_string(const char *json, const char *key,
                            char *output, size_t capacity)
{
    const char *position = json_value_start(json, key);
    if (position == NULL || *position++ != '"' || capacity == 0) return false;
    size_t length = 0;
    while (*position && *position != '"') {
        char value = *position++;
        if (value == '\\' && *position) {
            char escaped = *position++;
            value = escaped == 'n' ? '\n' : escaped == 'r' ? '\r' :
                    escaped == 't' ? '\t' : escaped;
        }
        if (length + 1 < capacity) output[length++] = value;
    }
    output[length] = '\0';
    return *position == '"';
}

static bool json_get_number(const char *json, const char *key, int *value)
{
    const char *position = json_value_start(json, key);
    if (position == NULL) return false;
    char *end = NULL;
    long parsed = strtol(position, &end, 10);
    if (end == position) return false;
    *value = (int)parsed;
    return true;
}

static bool json_get_bool(const char *json, const char *key, bool *value)
{
    const char *position = json_value_start(json, key);
    if (position == NULL) return false;
    if (strncmp(position, "true", 4) == 0) {
        *value = true;
        return true;
    }
    if (strncmp(position, "false", 5) == 0) {
        *value = false;
        return true;
    }
    return false;
}

static void handle_workflow_json(const brain_config_t *config,
                                 const char *event)
{
    char type[32] = "";
    char audio_url[WORKFLOW_MAX_URL] = "";
    char message[512] = "";
    char tool[64] = "";
    char expression[64] = "";
    json_get_string(event, "type", type, sizeof(type));
    json_get_string(event, "audio_url", audio_url, sizeof(audio_url));
    json_get_string(event, "message", message, sizeof(message));
    json_get_string(event, "tool", tool, sizeof(tool));
    json_get_string(event, "expression", expression, sizeof(expression));
    if (strcmp(type, "stage") == 0) {
        char stage[64] = "";
        json_get_string(event, "stage", stage, sizeof(stage));
        emit_event(
            strcmp(stage, "synthesizing") == 0
                ? BRAIN_WORKFLOW_EVENT_SYNTHESIZING
                : BRAIN_WORKFLOW_EVENT_TRANSCRIBING,
            stage, NULL, NULL, false, 0);
    } else if (strcmp(type, "transcript") == 0) {
        char transcript[1024] = "";
        json_get_string(event, "transcript", transcript, sizeof(transcript));
        emit_event(BRAIN_WORKFLOW_EVENT_TRANSCRIPT,
                   transcript, NULL, NULL, false, 0);
    } else if (strcmp(type, "tool_start") == 0) {
        emit_event(BRAIN_WORKFLOW_EVENT_TOOL_START, message, tool, NULL,
                   false, 0);
    } else if (strcmp(type, "tool_result") == 0) {
        int value = 0;
        bool has_temperature =
            json_get_number(event, "temperature_c", &value);
        if (value < -127) value = -127;
        if (value > 127) value = 127;
        emit_event(BRAIN_WORKFLOW_EVENT_TOOL_RESULT, NULL, tool, expression,
                   has_temperature, (int8_t)value);
    } else if (strcmp(type, "done") == 0) {
        char answer[2048] = "";
        json_get_string(event, "answer", answer, sizeof(answer));
        int value = 0;
        bool has_temperature =
            json_get_number(event, "temperature_c", &value);
        if (value < -127) value = -127;
        if (value > 127) value = 127;
        emit_event(BRAIN_WORKFLOW_EVENT_SPEAKING,
                   answer, NULL, expression,
                   has_temperature, (int8_t)value);
    } else if (strcmp(type, "error") == 0) {
        char error[512] = "";
        json_get_string(event, "error", error, sizeof(error));
        emit_event(BRAIN_WORKFLOW_EVENT_ERROR,
                   error, NULL, NULL, false, 0);
    }
    if (audio_url[0] != '\0') {
        esp_err_t err = play_audio_url(config, audio_url);
        if (err != ESP_OK) {
            emit_event(BRAIN_WORKFLOW_EVENT_ERROR,
                       esp_err_to_name(err), NULL, NULL, false, 0);
        }
    }
}

static esp_err_t read_ndjson_response(upload_context_t *upload)
{
    if (esp_http_client_fetch_headers(upload->client) < 0) {
        return ESP_ERR_HTTP_FETCH_HEADER;
    }
    int status = esp_http_client_get_status_code(upload->client);
    if (status < 200 || status >= 300) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    char *line = malloc(WORKFLOW_LINE_CAPACITY);
    if (line == NULL) {
        return ESP_ERR_NO_MEM;
    }
    size_t length = 0;
    esp_err_t err = ESP_OK;
    char buffer[768];
    while (err == ESP_OK) {
        int received = esp_http_client_read(
            upload->client, buffer, sizeof(buffer));
        if (received == 0) break;
        if (received < 0) {
            err = ESP_FAIL;
            break;
        }
        for (int i = 0; i < received; ++i) {
            if (buffer[i] == '\n') {
                line[length] = '\0';
                if (length > 0) {
                    handle_workflow_json(upload->config, line);
                }
                length = 0;
            } else if (length + 1 < WORKFLOW_LINE_CAPACITY) {
                line[length++] = buffer[i];
            } else {
                err = ESP_ERR_INVALID_SIZE;
                break;
            }
        }
    }
    free(line);
    return err;
}

static esp_err_t capture_to_endpoint(const brain_config_t *config,
                                     const char *path, uint16_t silence_ms,
                                     uint16_t wait_ms,
                                     const char *robot_status_json,
                                     brain_audio_capture_result_t *capture,
                                     upload_context_t *upload)
{
    memset(upload, 0, sizeof(*upload));
    upload->config = config;
    upload->wake_phrase =
        strcmp(path, "/api/device/wake") == 0 ? config->wake_phrase : NULL;
    upload->robot_status_json = robot_status_json;
    ESP_RETURN_ON_ERROR(
        build_url(upload->endpoint, sizeof(upload->endpoint),
                  config->workflow_url, path),
        "brain-workflow", "invalid Workflow URL");
    brain_audio_capture_config_t capture_config = {
        .vad_threshold = config->vad_threshold,
        .silence_ms = silence_ms,
        .max_recording_ms = 45000,
        .wait_for_voice_ms = wait_ms,
    };
    esp_err_t err = brain_audio_capture(
        &capture_config, upload_pcm_chunk, upload, capture);
    if (err == ESP_OK) {
        err = finish_upload(upload);
    }
    return err;
}

static void close_upload(upload_context_t *upload)
{
    if (upload->client != NULL) {
        esp_http_client_close(upload->client);
        esp_http_client_cleanup(upload->client);
        upload->client = NULL;
    }
}

esp_err_t brain_workflow_init(brain_workflow_event_handler_t handler,
                              void *context)
{
    s_event_handler = handler;
    s_event_context = context;
    if (s_workflow_mutex == NULL) {
        s_workflow_mutex = xSemaphoreCreateMutex();
    }
    return s_workflow_mutex != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t brain_workflow_run_voice(const brain_config_t *config,
                                   const char *robot_status_json)
{
    if (config == NULL || config->workflow_url[0] == '\0') {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_workflow_mutex, 0) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    upload_context_t upload;
    brain_audio_capture_result_t capture;
    emit_event(BRAIN_WORKFLOW_EVENT_LISTENING,
               "listening", NULL, NULL, false, 0);
    esp_err_t err = capture_to_endpoint(
        config, "/api/device/voice", config->silence_ms, 15000,
        robot_status_json,
        &capture, &upload);
    if (err == ESP_OK) {
        err = read_ndjson_response(&upload);
    }
    close_upload(&upload);
    if (err == ESP_OK) {
        emit_event(BRAIN_WORKFLOW_EVENT_COMPLETE,
                   "voice interaction complete", NULL, NULL, false, 0);
    } else {
        emit_event(BRAIN_WORKFLOW_EVENT_ERROR,
                   esp_err_to_name(err), NULL, NULL, false, 0);
    }
    xSemaphoreGive(s_workflow_mutex);
    return err;
}

esp_err_t brain_workflow_wait_for_wake(const brain_config_t *config,
                                       bool *detected)
{
    if (config == NULL || detected == NULL ||
        config->workflow_url[0] == '\0' ||
        config->wake_phrase[0] == '\0') {
        return ESP_ERR_INVALID_STATE;
    }
    *detected = false;
    if (xSemaphoreTake(s_workflow_mutex, 0) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    upload_context_t upload;
    brain_audio_capture_result_t capture;
    esp_err_t err = capture_to_endpoint(
        config, "/api/device/wake", 900, 60000, NULL,
        &capture, &upload);
    if (err == ESP_OK) {
        if (esp_http_client_fetch_headers(upload.client) < 0 ||
            esp_http_client_get_status_code(upload.client) != 200) {
            err = ESP_ERR_INVALID_RESPONSE;
        }
    }
    char response[2048];
    size_t length = 0;
    while (err == ESP_OK && length + 1 < sizeof(response)) {
        int received = esp_http_client_read(
            upload.client, &response[length],
            sizeof(response) - length - 1);
        if (received == 0) break;
        if (received < 0) {
            err = ESP_FAIL;
            break;
        }
        length += received;
    }
    if (err == ESP_OK) {
        response[length] = '\0';
        char audio_url[WORKFLOW_MAX_URL] = "";
        if (!json_get_bool(response, "detected", detected)) {
            err = ESP_ERR_INVALID_RESPONSE;
        }
        json_get_string(response, "audio_url", audio_url, sizeof(audio_url));
        if (err == ESP_OK && *detected && audio_url[0] != '\0') {
            err = play_audio_url(config, audio_url);
        }
    }
    close_upload(&upload);
    xSemaphoreGive(s_workflow_mutex);
    return err;
}
