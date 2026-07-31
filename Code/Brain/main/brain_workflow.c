#include "brain_workflow.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "brain_audio.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"

#define TAG "brain-workflow"
#define AUDIO_SAMPLE_RATE 16000
#define HTTP_TIMEOUT_MS 300000
#define HTTP_BUFFER_SIZE 2048
#define HTTP_MAX_RESPONSE (64 * 1024)
#define MAX_URL 1024
#define MAX_TRANSCRIPT 4096
#define MAX_ANSWER 8192
#define MAX_TOOL_RESULT 12288
#define MAX_TOOL_ROUNDS 6
#define AUDIO_BUFFER_SIZE 2048
#define AUDIO_PLAYBACK_BUFFER_SIZE (32 * 1024)
#define AUDIO_PLAYBACK_PREBUFFER_SIZE (16 * 1024)
#define AUDIO_PLAYBACK_QUEUE_LENGTH \
    (AUDIO_PLAYBACK_BUFFER_SIZE / AUDIO_BUFFER_SIZE)
#define AUDIO_PLAYBACK_PREBUFFER_CHUNKS \
    (AUDIO_PLAYBACK_PREBUFFER_SIZE / AUDIO_BUFFER_SIZE)
#define AUDIO_PLAYBACK_TASK_STACK_SIZE 12288
#define AUDIO_PLAYBACK_SILENT_TAIL_SAMPLES 320
#define STT_UPLOAD_BLOCK_SIZE 3072
#define STT_UPLOAD_BUFFER_SIZE (64 * 1024)
#define STT_UPLOAD_QUEUE_LENGTH \
    ((STT_UPLOAD_BUFFER_SIZE + STT_UPLOAD_BLOCK_SIZE - 1) / \
     STT_UPLOAD_BLOCK_SIZE)
#define MANUAL_LOCK_WAIT_MS 6500
#define CONVERSATION_HISTORY_SIZE 8192

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} response_buffer_t;

typedef struct {
    size_t length;
    uint8_t bytes[STT_UPLOAD_BLOCK_SIZE];
} stt_chunk_t;

typedef struct {
    esp_http_client_handle_t client;
    const brain_config_t *config;
    char url[MAX_URL];
    const char *version;
    uint8_t carry[3];
    size_t carry_length;
    QueueHandle_t queue;
    SemaphoreHandle_t done;
    stt_chunk_t *write_chunk;
    char *transcript;
    size_t transcript_capacity;
    volatile bool capture_done;
    volatile bool abort;
    volatile bool finished;
    bool started;
    bool opened;
    esp_err_t result;
} stt_upload_t;

typedef struct {
    size_t length;
    uint8_t bytes[AUDIO_BUFFER_SIZE];
} audio_chunk_t;

typedef struct {
    QueueHandle_t queue;
    SemaphoreHandle_t done;
    audio_chunk_t *write_chunk;
    volatile bool download_done;
    volatile bool abort;
    volatile esp_err_t download_result;
    volatile esp_err_t playback_result;
} audio_playback_t;

static brain_workflow_event_handler_t s_event_handler;
static void *s_event_context;
static SemaphoreHandle_t s_workflow_mutex;
static char s_conversation_history[CONVERSATION_HISTORY_SIZE];

static void emit_event(brain_workflow_event_type_t type, const char *message,
                       const char *tool, const char *expression,
                       bool has_temperature, int8_t temperature)
{
    if (s_event_handler == NULL) return;
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

static const char *llm_token(const brain_config_t *config)
{
    return config->llm_token[0] ? config->llm_token : config->ai_token;
}

static const char *replicate_token(const brain_config_t *config)
{
    return config->replicate_token[0]
               ? config->replicate_token : config->ai_token;
}

bool brain_workflow_config_ready(const brain_config_t *config,
                                 const char **missing)
{
    const char *reason = NULL;
    if (config == NULL) reason = "configuration";
    else if (llm_token(config)[0] == '\0') reason = "LLM/shared AI token";
    else if (replicate_token(config)[0] == '\0')
        reason = "Replicate/shared AI token";
    else if (config->llm_url[0] == '\0') reason = "LLM URL";
    else if (config->replicate_url[0] == '\0') reason = "Replicate URL";
    else if (config->llm_model[0] == '\0') reason = "LLM model";
    else if (config->stt_model[0] == '\0') reason = "STT model";
    else if (config->tts_model[0] == '\0') reason = "TTS model";
    else if (config->voice_id[0] == '\0') reason = "Bender voice ID";
    if (missing != NULL) *missing = reason;
    return reason == NULL;
}

static esp_err_t append_response(response_buffer_t *buffer,
                                 const char *data, size_t length)
{
    if (buffer->length + length + 1 > HTTP_MAX_RESPONSE) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (buffer->length + length + 1 > buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity : 4096;
        while (capacity < buffer->length + length + 1) capacity *= 2;
        if (capacity > HTTP_MAX_RESPONSE) capacity = HTTP_MAX_RESPONSE;
        char *grown = realloc(buffer->data, capacity);
        if (grown == NULL) return ESP_ERR_NO_MEM;
        buffer->data = grown;
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->length, data, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
    return ESP_OK;
}

static esp_http_client_handle_t create_http_client(const char *url)
{
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .buffer_size = HTTP_BUFFER_SIZE,
        .buffer_size_tx = HTTP_BUFFER_SIZE,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
    };
    return esp_http_client_init(&config);
}

static esp_err_t set_bearer(esp_http_client_handle_t client,
                            const char *token)
{
    if (token == NULL || token[0] == '\0') return ESP_OK;
    size_t size = strlen(token) + sizeof("Bearer ");
    char *authorization = malloc(size);
    if (authorization == NULL) return ESP_ERR_NO_MEM;
    snprintf(authorization, size, "Bearer %s", token);
    esp_err_t err = esp_http_client_set_header(
        client, "Authorization", authorization);
    free(authorization);
    return err;
}

static esp_err_t write_all(esp_http_client_handle_t client,
                           const void *data, size_t length)
{
    const char *bytes = data;
    while (length > 0) {
        int written = esp_http_client_write(client, bytes, (int)length);
        if (written <= 0) return ESP_ERR_HTTP_WRITE_DATA;
        bytes += written;
        length -= (size_t)written;
    }
    return ESP_OK;
}

static esp_err_t read_http_response(esp_http_client_handle_t client,
                                    response_buffer_t *response,
                                    int *status)
{
    if (esp_http_client_fetch_headers(client) < 0) {
        return ESP_ERR_HTTP_FETCH_HEADER;
    }
    if (status != NULL) *status = esp_http_client_get_status_code(client);
    esp_err_t err = ESP_OK;
    char bytes[HTTP_BUFFER_SIZE];
    while (err == ESP_OK) {
        int count = esp_http_client_read(client, bytes, sizeof(bytes));
        if (count == 0) break;
        if (count < 0) return ESP_FAIL;
        err = append_response(response, bytes, (size_t)count);
    }
    if (err == ESP_OK && response->data == NULL) {
        err = append_response(response, "", 0);
    }
    return err;
}

static esp_err_t http_request(esp_http_client_method_t method,
                              const char *url, const char *token,
                              const char *content_type, const char *body,
                              response_buffer_t *response, int *status)
{
    esp_http_client_handle_t client = create_http_client(url);
    if (client == NULL) return ESP_ERR_NO_MEM;
    esp_http_client_set_method(client, method);
    esp_err_t err = set_bearer(client, token);
    if (err == ESP_OK && content_type != NULL) {
        err = esp_http_client_set_header(client, "Content-Type",
                                         content_type);
    }
    size_t length = body != NULL ? strlen(body) : 0;
    if (err == ESP_OK) err = esp_http_client_open(client, (int)length);
    if (err == ESP_OK && length > 0) err = write_all(client, body, length);
    if (err == ESP_OK) err = read_http_response(client, response, status);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return err;
}

static esp_err_t checked_http_request(esp_http_client_method_t method,
                                      const char *url, const char *token,
                                      const char *content_type,
                                      const char *body,
                                      response_buffer_t *response)
{
    int status = 0;
    esp_err_t err = http_request(method, url, token, content_type, body,
                                 response, &status);
    if (err == ESP_OK && (status < 200 || status >= 300)) {
        printf("\n[AI HTTP %d] %.320s\n", status,
               response->data != NULL ? response->data : "");
        err = status == 401 || status == 403
                  ? ESP_ERR_INVALID_STATE : ESP_ERR_INVALID_RESPONSE;
    }
    return err;
}

static esp_err_t join_url(char *output, size_t capacity,
                          const char *base, const char *path)
{
    size_t base_length = strlen(base);
    bool base_slash = base_length > 0 && base[base_length - 1] == '/';
    bool path_slash = path[0] == '/';
    int count = snprintf(output, capacity, "%s%s%s", base,
                         base_slash && path_slash ? path + 1 :
                         !base_slash && !path_slash ? "/" : "", path);
    return count > 0 && (size_t)count < capacity
               ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

static esp_err_t llm_completions_url(char *output, size_t capacity,
                                     const char *configured_url)
{
    static const char suffix[] = "/chat/completions";
    size_t length = strlen(configured_url);
    while (length > 0 && configured_url[length - 1] == '/') --length;
    size_t suffix_length = sizeof(suffix) - 1;
    if (length >= suffix_length &&
        memcmp(configured_url + length - suffix_length,
               suffix, suffix_length) == 0) {
        int count = snprintf(output, capacity, "%.*s", (int)length,
                             configured_url);
        return count > 0 && (size_t)count < capacity
                   ? ESP_OK : ESP_ERR_INVALID_SIZE;
    }
    return join_url(output, capacity, configured_url, suffix);
}

static const char *json_value_start(const char *json, const char *key)
{
    char pattern[96];
    int size = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (size <= 0 || (size_t)size >= sizeof(pattern)) return NULL;
    const char *position = json;
    while ((position = strstr(position, pattern)) != NULL) {
        position += strlen(pattern);
        while (isspace((unsigned char)*position)) ++position;
        if (*position == ':') {
            ++position;
            while (isspace((unsigned char)*position)) ++position;
            return position;
        }
    }
    return NULL;
}

static int hex_digit(char value)
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

static size_t append_utf8(char *output, size_t length, size_t capacity,
                          unsigned codepoint)
{
    uint8_t bytes[3];
    size_t count;
    if (codepoint < 0x80) {
        bytes[0] = codepoint;
        count = 1;
    } else if (codepoint < 0x800) {
        bytes[0] = 0xC0 | (codepoint >> 6);
        bytes[1] = 0x80 | (codepoint & 0x3F);
        count = 2;
    } else {
        bytes[0] = 0xE0 | (codepoint >> 12);
        bytes[1] = 0x80 | ((codepoint >> 6) & 0x3F);
        bytes[2] = 0x80 | (codepoint & 0x3F);
        count = 3;
    }
    for (size_t i = 0; i < count && length + 1 < capacity; ++i) {
        output[length++] = (char)bytes[i];
    }
    return length;
}

static bool json_decode_string(const char *position, char *output,
                               size_t capacity)
{
    if (position == NULL || *position++ != '"' || capacity == 0) return false;
    size_t length = 0;
    while (*position && *position != '"') {
        unsigned char value = (unsigned char)*position++;
        if (value == '\\' && *position) {
            char escaped = *position++;
            if (escaped == 'u') {
                unsigned codepoint = 0;
                for (int i = 0; i < 4; ++i) {
                    int digit = hex_digit(position[i]);
                    if (digit < 0) return false;
                    codepoint = codepoint * 16 + (unsigned)digit;
                }
                position += 4;
                length = append_utf8(output, length, capacity, codepoint);
                continue;
            }
            value = escaped == 'n' ? '\n' : escaped == 'r' ? '\r' :
                    escaped == 't' ? '\t' : escaped == 'b' ? '\b' :
                    escaped == 'f' ? '\f' : (unsigned char)escaped;
        }
        if (length + 1 < capacity) output[length++] = (char)value;
    }
    output[length] = '\0';
    return *position == '"';
}

static bool json_get_string(const char *json, const char *key,
                            char *output, size_t capacity)
{
    return json_decode_string(json_value_start(json, key), output, capacity);
}

static bool json_get_int(const char *json, const char *key, int *value)
{
    const char *position = json_value_start(json, key);
    if (position == NULL) return false;
    char *end = NULL;
    long parsed = strtol(position, &end, 10);
    if (end == position) return false;
    *value = (int)parsed;
    return true;
}

static char *json_escape(const char *value)
{
    if (value == NULL) value = "";
    size_t needed = 1;
    for (const unsigned char *p = (const unsigned char *)value; *p; ++p) {
        needed += *p == '"' || *p == '\\' || *p < 0x20 ? 2 : 1;
    }
    char *escaped = malloc(needed);
    if (escaped == NULL) return NULL;
    char *out = escaped;
    for (const unsigned char *p = (const unsigned char *)value; *p; ++p) {
        if (*p == '"' || *p == '\\') {
            *out++ = '\\';
            *out++ = (char)*p;
        } else if (*p == '\n') {
            *out++ = '\\';
            *out++ = 'n';
        } else if (*p == '\r') {
            *out++ = '\\';
            *out++ = 'r';
        } else if (*p == '\t') {
            *out++ = '\\';
            *out++ = 't';
        } else if (*p < 0x20) {
            *out++ = ' ';
        } else {
            *out++ = (char)*p;
        }
    }
    *out = '\0';
    return escaped;
}

static char *url_encode(const char *value)
{
    static const char HEX[] = "0123456789ABCDEF";
    size_t length = strlen(value);
    char *encoded = malloc(length * 3 + 1);
    if (encoded == NULL) return NULL;
    char *out = encoded;
    for (const unsigned char *p = (const unsigned char *)value; *p; ++p) {
        if (isalnum(*p) || *p == '-' || *p == '_' || *p == '.' ||
            *p == '~') {
            *out++ = (char)*p;
        } else {
            *out++ = '%';
            *out++ = HEX[*p >> 4];
            *out++ = HEX[*p & 15];
        }
    }
    *out = '\0';
    return encoded;
}

static esp_err_t prediction_endpoint(const brain_config_t *config,
                                     const char *model, char *url,
                                     size_t capacity, const char **version)
{
    const char *colon = strchr(model, ':');
    char path[BRAIN_CONFIG_MODEL_SIZE + 32];
    if (colon != NULL) {
        *version = colon + 1;
        strlcpy(path, "/predictions", sizeof(path));
    } else {
        *version = NULL;
        int count = snprintf(path, sizeof(path), "/models/%s/predictions",
                             model);
        if (count <= 0 || (size_t)count >= sizeof(path)) {
            return ESP_ERR_INVALID_SIZE;
        }
    }
    return join_url(url, capacity, config->replicate_url, path);
}

static esp_err_t extract_prediction_output(const char *json,
                                           char *output, size_t capacity)
{
    const char *value = json_value_start(json, "output");
    if (value == NULL || strncmp(value, "null", 4) == 0) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (*value == '"' && json_decode_string(value, output, capacity)) {
        return ESP_OK;
    }
    if (*value == '[') {
        const char *string = strchr(value, '"');
        if (string != NULL && json_decode_string(string, output, capacity)) {
            return ESP_OK;
        }
    }
    static const char *const keys[] = {
        "text", "transcription", "transcript", "audio", "url", "file"
    };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        const char *nested = json_value_start(value, keys[i]);
        if (nested != NULL && *nested == '"' &&
            json_decode_string(nested, output, capacity)) {
            return ESP_OK;
        }
    }
    return ESP_ERR_INVALID_RESPONSE;
}

static esp_err_t wait_prediction(const brain_config_t *config,
                                 response_buffer_t *prediction,
                                 char *output, size_t output_capacity)
{
    char status[32] = "";
    json_get_string(prediction->data, "status", status, sizeof(status));
    for (unsigned poll = 0; strcmp(status, "succeeded") != 0; ++poll) {
        if (strcmp(status, "failed") == 0 ||
            strcmp(status, "canceled") == 0 || poll >= 180) {
            return strcmp(status, "failed") == 0
                       ? ESP_FAIL : ESP_ERR_TIMEOUT;
        }
        char id[128] = "";
        if (!json_get_string(prediction->data, "id", id, sizeof(id))) {
            return ESP_ERR_INVALID_RESPONSE;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        char path[176];
        char url[MAX_URL];
        snprintf(path, sizeof(path), "/predictions/%s", id);
        esp_err_t err = join_url(url, sizeof(url),
                                 config->replicate_url, path);
        if (err != ESP_OK) return err;
        free(prediction->data);
        memset(prediction, 0, sizeof(*prediction));
        err = checked_http_request(HTTP_METHOD_GET, url,
                                   replicate_token(config), NULL, NULL,
                                   prediction);
        if (err != ESP_OK) return err;
        status[0] = '\0';
        json_get_string(prediction->data, "status", status, sizeof(status));
    }
    return extract_prediction_output(prediction->data,
                                     output, output_capacity);
}

static esp_err_t run_prediction_json(const brain_config_t *config,
                                     const char *model, const char *input,
                                     bool stream, char *output,
                                     size_t output_capacity)
{
    char url[MAX_URL];
    const char *version = NULL;
    esp_err_t err = prediction_endpoint(config, model, url, sizeof(url),
                                        &version);
    if (err != ESP_OK) return err;
    size_t body_size = strlen(input) +
        (version != NULL ? strlen(version) : 0) + 96;
    char *body = malloc(body_size);
    if (body == NULL) return ESP_ERR_NO_MEM;
    if (version != NULL) {
        snprintf(body, body_size,
                 "{\"version\":\"%s\",\"input\":{%s}%s}", version, input,
                 stream ? ",\"stream\":false" : "");
    } else {
        snprintf(body, body_size, "{\"input\":{%s}%s}", input,
                 stream ? ",\"stream\":false" : "");
    }
    response_buffer_t prediction = { 0 };
    esp_http_client_handle_t client = create_http_client(url);
    if (client == NULL) {
        free(body);
        return ESP_ERR_NO_MEM;
    }
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    set_bearer(client, replicate_token(config));
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Prefer", "wait");
    err = esp_http_client_open(client, (int)strlen(body));
    if (err == ESP_OK) err = write_all(client, body, strlen(body));
    int status = 0;
    if (err == ESP_OK) err = read_http_response(client, &prediction, &status);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    free(body);
    if (err == ESP_OK && (status < 200 || status >= 300)) {
        printf("\n[AI prediction HTTP %d] %.320s\n", status,
               prediction.data != NULL ? prediction.data : "");
        err = ESP_ERR_INVALID_RESPONSE;
    }
    if (err == ESP_OK) {
        err = wait_prediction(config, &prediction, output, output_capacity);
    }
    free(prediction.data);
    return err;
}

static esp_err_t chunk_write(esp_http_client_handle_t client,
                             const void *data, size_t length)
{
    char header[20];
    int header_length = snprintf(header, sizeof(header), "%x\r\n",
                                 (unsigned)length);
    if (header_length <= 0 || (size_t)header_length >= sizeof(header)) {
        return ESP_ERR_INVALID_SIZE;
    }
    esp_err_t err = write_all(client, header, (size_t)header_length);
    if (err == ESP_OK) err = write_all(client, data, length);
    if (err == ESP_OK) err = write_all(client, "\r\n", 2);
    return err;
}

static esp_err_t base64_write(stt_upload_t *upload, const uint8_t *data,
                              size_t length, bool finish)
{
    uint8_t input[771];
    char encoded[1032];
    esp_err_t err = ESP_OK;
    while (length > 0 && err == ESP_OK) {
        size_t prefix = upload->carry_length;
        memcpy(input, upload->carry, prefix);
        size_t take = sizeof(input) - prefix;
        if (take > length) take = length;
        memcpy(input + prefix, data, take);
        data += take;
        length -= take;
        size_t total = prefix + take;
        size_t complete = total - total % 3;
        if (complete > 0) {
            size_t encoded_length = 0;
            if (mbedtls_base64_encode(
                    (unsigned char *)encoded, sizeof(encoded),
                    &encoded_length, input, complete) != 0) {
                return ESP_FAIL;
            }
            err = chunk_write(upload->client, encoded, encoded_length);
        }
        upload->carry_length = total - complete;
        if (upload->carry_length > 0) {
            memcpy(upload->carry, input + complete, upload->carry_length);
        }
    }
    if (finish && err == ESP_OK && upload->carry_length > 0) {
        size_t encoded_length = 0;
        if (mbedtls_base64_encode(
                (unsigned char *)encoded, sizeof(encoded), &encoded_length,
                upload->carry, upload->carry_length) != 0) {
            return ESP_FAIL;
        }
        err = chunk_write(upload->client, encoded, encoded_length);
        upload->carry_length = 0;
    }
    return err;
}

static void wav_header(uint8_t header[44])
{
    const uint8_t template[] = {
        'R','I','F','F', 0xff,0xff,0xff,0x7f, 'W','A','V','E',
        'f','m','t',' ', 16,0,0,0, 1,0, 1,0,
        0x80,0x3e,0,0, 0,0x7d,0,0, 2,0, 16,0,
        'd','a','t','a', 0xff,0xff,0xff,0x7f
    };
    memcpy(header, template, sizeof(template));
}

static esp_err_t stt_open(stt_upload_t *upload)
{
    if (upload->opened) return ESP_OK;
    upload->client = create_http_client(upload->url);
    if (upload->client == NULL) return ESP_ERR_NO_MEM;
    esp_http_client_set_method(upload->client, HTTP_METHOD_POST);
    esp_err_t err = set_bearer(
        upload->client, replicate_token(upload->config));
    if (err == ESP_OK) {
        err = esp_http_client_set_header(
            upload->client, "Content-Type", "application/json");
    }
    if (err == ESP_OK) {
        err = esp_http_client_set_header(
            upload->client, "Prefer", "wait");
    }
    if (err == ESP_OK) err = esp_http_client_open(upload->client, -1);
    char prefix[256];
    int prefix_length = upload->version != NULL
        ? snprintf(prefix, sizeof(prefix),
                   "{\"version\":\"%s\",\"input\":{\"audio\":"
                   "\"data:audio/wav;base64,", upload->version)
        : snprintf(prefix, sizeof(prefix),
                   "{\"input\":{\"audio\":\"data:audio/wav;base64,");
    if (err == ESP_OK &&
        (prefix_length <= 0 || (size_t)prefix_length >= sizeof(prefix))) {
        err = ESP_ERR_INVALID_SIZE;
    }
    if (err == ESP_OK) {
        err = chunk_write(upload->client, prefix, prefix_length);
    }
    uint8_t header[44];
    wav_header(header);
    if (err == ESP_OK) {
        upload->opened = true;
        err = base64_write(upload, header, sizeof(header), false);
    }
    if (err != ESP_OK && upload->client != NULL) {
        esp_http_client_close(upload->client);
        esp_http_client_cleanup(upload->client);
        upload->client = NULL;
    }
    return err;
}

static esp_err_t stt_enqueue_write_chunk(stt_upload_t *upload)
{
    while (!upload->abort && !upload->finished) {
        stt_chunk_t *chunk = upload->write_chunk;
        if (xQueueSend(upload->queue, &chunk,
                       pdMS_TO_TICKS(100)) == pdTRUE) {
            upload->write_chunk = NULL;
            return ESP_OK;
        }
    }
    return upload->finished ? upload->result : ESP_ERR_INVALID_STATE;
}

static esp_err_t stt_send_bytes(stt_upload_t *upload,
                                const uint8_t *bytes, size_t length)
{
    while (length > 0) {
        if (upload->abort || upload->finished) {
            return upload->finished
                       ? upload->result : ESP_ERR_INVALID_STATE;
        }
        if (upload->write_chunk == NULL) {
            upload->write_chunk = malloc(sizeof(*upload->write_chunk));
            if (upload->write_chunk == NULL) {
                ESP_LOGE(TAG,
                         "STT chunk allocation failed: free=%zu largest=%zu",
                         heap_caps_get_free_size(MALLOC_CAP_8BIT),
                         heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
                return ESP_ERR_NO_MEM;
            }
            upload->write_chunk->length = 0;
        }
        size_t available = STT_UPLOAD_BLOCK_SIZE -
                           upload->write_chunk->length;
        size_t copied = length < available ? length : available;
        memcpy(upload->write_chunk->bytes + upload->write_chunk->length,
               bytes, copied);
        upload->write_chunk->length += copied;
        bytes += copied;
        length -= copied;
        if (upload->write_chunk->length == STT_UPLOAD_BLOCK_SIZE) {
            esp_err_t err = stt_enqueue_write_chunk(upload);
            if (err != ESP_OK) return err;
        }
    }
    return ESP_OK;
}

static esp_err_t stt_flush(stt_upload_t *upload)
{
    if (upload->write_chunk == NULL) return ESP_OK;
    return stt_enqueue_write_chunk(upload);
}

static void stt_upload_task(void *argument)
{
    stt_upload_t *upload = argument;
    esp_err_t err = stt_open(upload);
    while (err == ESP_OK && !upload->abort) {
        stt_chunk_t *chunk = NULL;
        if (xQueueReceive(upload->queue, &chunk,
                          pdMS_TO_TICKS(100)) == pdTRUE) {
            err = base64_write(
                upload, chunk->bytes, chunk->length, false);
            free(chunk);
        } else if (upload->capture_done) {
            break;
        }
    }
    if (err == ESP_OK && !upload->abort) {
        err = base64_write(upload, NULL, 0, true);
    }
    static const char suffix[] =
        "\",\"task\":\"transcribe\",\"language\":\"spanish\","
        "\"timestamp\":\"chunk\",\"batch_size\":24,"
        "\"diarise_audio\":false}}";
    if (err == ESP_OK && !upload->abort) {
        err = chunk_write(upload->client, suffix, strlen(suffix));
    }
    if (err == ESP_OK && !upload->abort) {
        err = write_all(upload->client, "0\r\n\r\n", 5);
    }
    response_buffer_t prediction = { 0 };
    int status = 0;
    if (err == ESP_OK && !upload->abort) {
        err = read_http_response(upload->client, &prediction, &status);
    }
    if (upload->client != NULL) {
        esp_http_client_close(upload->client);
        esp_http_client_cleanup(upload->client);
        upload->client = NULL;
    }
    if (err == ESP_OK && !upload->abort &&
        (status < 200 || status >= 300)) {
        printf("\n[AI STT HTTP %d] %.320s\n", status,
               prediction.data != NULL ? prediction.data : "");
        err = ESP_ERR_INVALID_RESPONSE;
    }
    if (err == ESP_OK && !upload->abort) {
        err = wait_prediction(upload->config, &prediction,
                              upload->transcript,
                              upload->transcript_capacity);
    }
    free(prediction.data);
    upload->result = upload->abort ? ESP_ERR_INVALID_STATE : err;
    upload->finished = true;
    xSemaphoreGive(upload->done);
    vTaskDelete(NULL);
}

static esp_err_t stt_start(stt_upload_t *upload)
{
    if (upload->started) return ESP_OK;
    upload->queue = xQueueCreate(
        STT_UPLOAD_QUEUE_LENGTH, sizeof(stt_chunk_t *));
    upload->done = xSemaphoreCreateBinary();
    if (upload->queue == NULL || upload->done == NULL) {
        ESP_LOGE(TAG,
                 "STT upload allocation failed: queue=%s done=%s "
                 "free=%zu largest=%zu",
                 upload->queue != NULL ? "yes" : "no",
                 upload->done != NULL ? "yes" : "no",
                 heap_caps_get_free_size(MALLOC_CAP_8BIT),
                 heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        if (upload->queue != NULL) vQueueDelete(upload->queue);
        if (upload->done != NULL) vSemaphoreDelete(upload->done);
        upload->queue = NULL;
        upload->done = NULL;
        return ESP_ERR_NO_MEM;
    }
    BaseType_t created = xTaskCreate(
        stt_upload_task, "stt_upload", 12288, upload, 5, NULL);
    if (created != pdPASS) {
        ESP_LOGE(TAG,
                 "STT upload task allocation failed: free=%zu largest=%zu",
                 heap_caps_get_free_size(MALLOC_CAP_8BIT),
                 heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        vQueueDelete(upload->queue);
        vSemaphoreDelete(upload->done);
        upload->queue = NULL;
        upload->done = NULL;
        return ESP_ERR_NO_MEM;
    }
    upload->started = true;
    return ESP_OK;
}

static esp_err_t stt_pcm_sink(const int16_t *samples, size_t sample_count,
                              void *context)
{
    stt_upload_t *upload = context;
    esp_err_t err = stt_start(upload);
    if (err != ESP_OK) return err;
    if (upload->finished) return upload->result;
    const uint8_t *bytes = (const uint8_t *)samples;
    return stt_send_bytes(
        upload, bytes, sample_count * sizeof(*samples));
}

static esp_err_t transcribe(const brain_config_t *config,
                            uint16_t silence_ms, uint16_t wait_ms,
                            uint16_t max_ms, char *transcript,
                            size_t transcript_capacity)
{
    char url[MAX_URL];
    const char *version = NULL;
    esp_err_t err = prediction_endpoint(
        config, config->stt_model, url, sizeof(url), &version);
    if (err != ESP_OK) return err;
    stt_upload_t upload = {
        .config = config,
        .version = version,
        .transcript = transcript,
        .transcript_capacity = transcript_capacity,
    };
    strlcpy(upload.url, url, sizeof(upload.url));
    brain_audio_capture_result_t capture;
    brain_audio_capture_config_t capture_config = {
        .vad_threshold = config->vad_threshold,
        .silence_ms = silence_ms,
        .max_recording_ms = max_ms,
        .wait_for_voice_ms = wait_ms,
    };
    if (err == ESP_OK) {
        err = brain_audio_capture(&capture_config, stt_pcm_sink,
                                  &upload, &capture);
    }
    if (upload.started) {
        if (err == ESP_OK) err = stt_flush(&upload);
        if (err != ESP_OK) upload.abort = true;
        upload.capture_done = true;
        if (xSemaphoreTake(upload.done, portMAX_DELAY) != pdTRUE &&
            err == ESP_OK) {
            err = ESP_FAIL;
        } else if (err == ESP_OK) {
            err = upload.result;
        }
        stt_chunk_t *chunk = NULL;
        while (xQueueReceive(upload.queue, &chunk, 0) == pdTRUE) {
            free(chunk);
        }
        free(upload.write_chunk);
        vQueueDelete(upload.queue);
        vSemaphoreDelete(upload.done);
    } else if (err == ESP_OK) {
        err = ESP_ERR_INVALID_STATE;
    }
    return err;
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

static esp_err_t play_audio_samples(const int16_t *samples,
                                    size_t sample_count)
{
    uint16_t peak = 0;
    for (size_t i = 0; i < sample_count; ++i) {
        int32_t sample = samples[i];
        uint16_t magnitude = sample == INT16_MIN ? INT16_MAX :
            (uint16_t)(sample < 0 ? -sample : sample);
        if (magnitude > peak) peak = magnitude;
    }
    uint32_t scaled = (uint32_t)peak * 3 / 128;
    emit_audio_level((uint8_t)(scaled > UINT8_MAX ? UINT8_MAX : scaled));
    return brain_audio_play_pcm16(samples, sample_count, 256);
}

static esp_err_t play_audio_silent_tail(int16_t *samples,
                                        size_t sample_count)
{
    memset(samples, 0, sample_count * sizeof(*samples));
    return play_audio_samples(samples, sample_count);
}

static void audio_playback_task(void *argument)
{
    audio_playback_t *playback = argument;
    while (!playback->abort && !playback->download_done &&
           uxQueueMessagesWaiting(playback->queue) <
               AUDIO_PLAYBACK_PREBUFFER_CHUNKS) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    esp_err_t err = ESP_OK;
    union {
        int16_t samples[AUDIO_BUFFER_SIZE / sizeof(int16_t) + 1];
        uint8_t bytes[AUDIO_BUFFER_SIZE + 1];
    } audio;
    int16_t tail[AUDIO_PLAYBACK_SILENT_TAIL_SAMPLES];
    int16_t combined[
        AUDIO_BUFFER_SIZE / sizeof(int16_t) +
        AUDIO_PLAYBACK_SILENT_TAIL_SAMPLES];
    size_t pending = 0;
    size_t tail_count = 0;
    while (!playback->abort) {
        audio_chunk_t *chunk = NULL;
        if (xQueueReceive(playback->queue, &chunk,
                          pdMS_TO_TICKS(100)) != pdTRUE) {
            if (playback->download_done) break;
            continue;
        }
        memcpy(audio.bytes + pending, chunk->bytes, chunk->length);
        size_t available = pending + chunk->length;
        free(chunk);
        size_t playable = available & ~(size_t)1;
        if (playable > 0) {
            size_t sample_count = playable / sizeof(int16_t);
            memcpy(combined, tail, tail_count * sizeof(*tail));
            memcpy(combined + tail_count, audio.samples,
                   sample_count * sizeof(*audio.samples));
            size_t combined_count = tail_count + sample_count;
            size_t output_count =
                combined_count > AUDIO_PLAYBACK_SILENT_TAIL_SAMPLES
                    ? combined_count - AUDIO_PLAYBACK_SILENT_TAIL_SAMPLES
                    : 0;
            if (output_count > 0) {
                err = play_audio_samples(combined, output_count);
                if (err != ESP_OK) {
                    playback->abort = true;
                    break;
                }
            }
            tail_count = combined_count - output_count;
            memcpy(tail, combined + output_count,
                   tail_count * sizeof(*tail));
        }
        pending = available - playable;
        if (pending > 0) audio.bytes[0] = audio.bytes[playable];
    }
    if (!playback->abort && err == ESP_OK && tail_count > 0) {
        err = play_audio_silent_tail(tail, tail_count);
    }
    if (err == ESP_OK && playback->download_result != ESP_OK) {
        err = playback->download_result;
    }
    playback->playback_result = err;
    emit_audio_level(0);
    xSemaphoreGive(playback->done);
    vTaskDelete(NULL);
}

static esp_err_t audio_playback_start(audio_playback_t *playback)
{
    playback->queue = xQueueCreate(
        AUDIO_PLAYBACK_QUEUE_LENGTH, sizeof(audio_chunk_t *));
    playback->done = xSemaphoreCreateBinary();
    if (playback->queue == NULL || playback->done == NULL) {
        ESP_LOGE(TAG,
                 "audio playback allocation failed: queue=%s done=%s "
                 "free=%zu largest=%zu",
                 playback->queue != NULL ? "yes" : "no",
                 playback->done != NULL ? "yes" : "no",
                 heap_caps_get_free_size(MALLOC_CAP_8BIT),
                 heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        if (playback->queue != NULL) vQueueDelete(playback->queue);
        if (playback->done != NULL) vSemaphoreDelete(playback->done);
        playback->queue = NULL;
        playback->done = NULL;
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(audio_playback_task, "audio_playback",
                    AUDIO_PLAYBACK_TASK_STACK_SIZE, playback, 5, NULL) !=
        pdPASS) {
        ESP_LOGE(TAG,
                 "audio playback task allocation failed: free=%zu "
                 "largest=%zu",
                 heap_caps_get_free_size(MALLOC_CAP_8BIT),
                 heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        vQueueDelete(playback->queue);
        vSemaphoreDelete(playback->done);
        playback->queue = NULL;
        playback->done = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t audio_playback_enqueue_write_chunk(
    audio_playback_t *playback)
{
    while (!playback->abort) {
        audio_chunk_t *chunk = playback->write_chunk;
        if (xQueueSend(playback->queue, &chunk,
                       pdMS_TO_TICKS(100)) == pdTRUE) {
            playback->write_chunk = NULL;
            return ESP_OK;
        }
    }
    return playback->playback_result != ESP_OK
               ? playback->playback_result : ESP_FAIL;
}

static esp_err_t audio_playback_send(audio_playback_t *playback,
                                     const uint8_t *bytes, size_t length)
{
    while (length > 0) {
        if (playback->abort) {
            return playback->playback_result != ESP_OK
                       ? playback->playback_result : ESP_FAIL;
        }
        if (playback->write_chunk == NULL) {
            playback->write_chunk = malloc(sizeof(*playback->write_chunk));
            if (playback->write_chunk == NULL) {
                ESP_LOGE(TAG,
                         "audio chunk allocation failed: free=%zu "
                         "largest=%zu",
                         heap_caps_get_free_size(MALLOC_CAP_8BIT),
                         heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
                return ESP_ERR_NO_MEM;
            }
            playback->write_chunk->length = 0;
        }
        size_t available = AUDIO_BUFFER_SIZE -
                           playback->write_chunk->length;
        size_t copied = length < available ? length : available;
        memcpy(playback->write_chunk->bytes +
                   playback->write_chunk->length,
               bytes, copied);
        playback->write_chunk->length += copied;
        bytes += copied;
        length -= copied;
        if (playback->write_chunk->length == AUDIO_BUFFER_SIZE) {
            esp_err_t err =
                audio_playback_enqueue_write_chunk(playback);
            if (err != ESP_OK) return err;
        }
    }
    return ESP_OK;
}

static esp_err_t audio_playback_flush(audio_playback_t *playback)
{
    if (playback->write_chunk == NULL) return ESP_OK;
    return audio_playback_enqueue_write_chunk(playback);
}

static esp_err_t play_audio_url(const char *url)
{
    esp_http_client_handle_t client = create_http_client(url);
    if (client == NULL) return ESP_ERR_NO_MEM;
    esp_err_t err = esp_http_client_open(client, 0);
    if (err == ESP_OK &&
        (esp_http_client_fetch_headers(client) < 0 ||
         esp_http_client_get_status_code(client) != 200)) {
        err = ESP_ERR_HTTP_FETCH_HEADER;
    }
    uint8_t header[768];
    size_t header_length = 0;
    size_t data_offset = 0;
    uint16_t channels = 0;
    uint16_t bits = 0;
    uint32_t sample_rate = 0;
    while (err == ESP_OK && header_length < sizeof(header) &&
           data_offset == 0) {
        int received = esp_http_client_read(
            client, (char *)header + header_length,
            sizeof(header) - header_length);
        if (received <= 0) {
            err = ESP_ERR_INVALID_RESPONSE;
            break;
        }
        header_length += (size_t)received;
        if (header_length < 12 || memcmp(header, "RIFF", 4) != 0 ||
            memcmp(header + 8, "WAVE", 4) != 0) continue;
        size_t offset = 12;
        while (offset + 8 <= header_length) {
            uint32_t chunk_size = read_u32_le(header + offset + 4);
            if (memcmp(header + offset, "fmt ", 4) == 0 &&
                chunk_size >= 16 && offset + 24 <= header_length) {
                if (read_u16_le(header + offset + 8) != 1) {
                    err = ESP_ERR_NOT_SUPPORTED;
                    break;
                }
                channels = read_u16_le(header + offset + 10);
                sample_rate = read_u32_le(header + offset + 12);
                bits = read_u16_le(header + offset + 22);
            } else if (memcmp(header + offset, "data", 4) == 0) {
                data_offset = offset + 8;
                break;
            }
            size_t next = offset + 8 + chunk_size + (chunk_size & 1U);
            if (next > header_length) break;
            offset = next;
        }
    }
    if (err == ESP_OK && (data_offset == 0 || channels != 1 ||
                          bits != 16 || sample_rate != 32000)) {
        err = ESP_ERR_NOT_SUPPORTED;
    }
    audio_playback_t playback = {
        .download_result = ESP_OK,
        .playback_result = ESP_OK,
    };
    if (err == ESP_OK) err = audio_playback_start(&playback);
    if (err == ESP_OK && header_length > data_offset) {
        err = audio_playback_send(
            &playback, header + data_offset, header_length - data_offset);
    }
    uint8_t audio[AUDIO_BUFFER_SIZE];
    while (err == ESP_OK) {
        int received = esp_http_client_read(
            client, (char *)audio, sizeof(audio));
        if (received == 0) break;
        if (received < 0) {
            err = ESP_FAIL;
            break;
        }
        err = audio_playback_send(&playback, audio, (size_t)received);
    }
    if (err == ESP_OK) err = audio_playback_flush(&playback);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (playback.queue != NULL) {
        playback.download_result = err;
        if (err != ESP_OK) playback.abort = true;
        playback.download_done = true;
        if (xSemaphoreTake(playback.done, portMAX_DELAY) != pdTRUE &&
            err == ESP_OK) {
            err = ESP_FAIL;
        } else if (err == ESP_OK) {
            err = playback.playback_result;
        }
        audio_chunk_t *chunk = NULL;
        while (xQueueReceive(playback.queue, &chunk, 0) == pdTRUE) {
            free(chunk);
        }
        free(playback.write_chunk);
        vQueueDelete(playback.queue);
        vSemaphoreDelete(playback.done);
    }
    return err;
}

static esp_err_t synthesize(const brain_config_t *config, const char *text,
                            char *audio_url, size_t capacity)
{
    char *escaped_text = json_escape(text);
    char *escaped_voice = json_escape(config->voice_id);
    if (escaped_text == NULL || escaped_voice == NULL) {
        free(escaped_text);
        free(escaped_voice);
        return ESP_ERR_NO_MEM;
    }
    size_t size = strlen(escaped_text) + strlen(escaped_voice) + 384;
    char *input = malloc(size);
    if (input == NULL) {
        free(escaped_text);
        free(escaped_voice);
        return ESP_ERR_NO_MEM;
    }
    snprintf(input, size,
             "\"text\":\"%s\",\"voice_id\":\"%s\",\"speed\":0.9,"
             "\"volume\":1.0,\"pitch\":0,\"emotion\":\"auto\","
             "\"english_normalization\":false,\"sample_rate\":32000,"
             "\"bitrate\":128000,\"audio_format\":\"wav\","
             "\"channel\":\"mono\",\"subtitle_enable\":false,"
             "\"language_boost\":\"Spanish\"",
             escaped_text, escaped_voice);
    free(escaped_text);
    free(escaped_voice);
    esp_err_t err = run_prediction_json(
        config, config->tts_model, input, true, audio_url, capacity);
    free(input);
    return err;
}

static esp_err_t speak(const brain_config_t *config, const char *text,
                       const char *expression)
{
    emit_event(BRAIN_WORKFLOW_EVENT_SYNTHESIZING,
               "Sintetizando voz en el ESP32...", NULL, expression,
               false, 0);
    char audio_url[MAX_URL];
    esp_err_t err = synthesize(config, text, audio_url, sizeof(audio_url));
    if (err != ESP_OK) return err;
    emit_event(BRAIN_WORKFLOW_EVENT_SPEAKING, text, NULL, expression,
               false, 0);
    return play_audio_url(audio_url);
}

static const char *weather_expression(const char *condition)
{
    char lower[128];
    size_t i;
    for (i = 0; condition[i] && i + 1 < sizeof(lower); ++i) {
        lower[i] = (char)tolower((unsigned char)condition[i]);
    }
    lower[i] = '\0';
    if (strstr(lower, "sun") || strstr(lower, "clear")) return "sunny";
    if (strstr(lower, "rain") || strstr(lower, "drizzle") ||
        strstr(lower, "shower")) return "rainy";
    if (strstr(lower, "thunder") || strstr(lower, "storm")) return "stormy";
    if (strstr(lower, "snow") || strstr(lower, "sleet")) return "snowy";
    if (strstr(lower, "cloud") || strstr(lower, "overcast")) return "cloudy";
    return "neutral";
}

static esp_err_t tool_weather(const char *location, char *result,
                              size_t capacity, char *expression,
                              size_t expression_capacity,
                              bool *has_temperature, int8_t *temperature)
{
    char *encoded = url_encode(location[0] ? location : "Madrid");
    if (encoded == NULL) return ESP_ERR_NO_MEM;
    char url[MAX_URL];
    int length = snprintf(url, sizeof(url),
                          "https://wttr.in/%s?format=%%l%%7C%%C%%7C%%t"
                          "%%7C%%f%%7C%%h%%7C%%w", encoded);
    free(encoded);
    if (length <= 0 || (size_t)length >= sizeof(url)) {
        return ESP_ERR_INVALID_SIZE;
    }
    response_buffer_t response = { 0 };
    esp_err_t err = checked_http_request(
        HTTP_METHOD_GET, url, NULL, NULL, NULL, &response);
    if (err == ESP_OK) {
        char *parts[6] = { 0 };
        size_t count = 0;
        char *save = NULL;
        for (char *part = strtok_r(response.data, "|", &save);
             part != NULL && count < 6;
             part = strtok_r(NULL, "|", &save)) {
            parts[count++] = part;
        }
        if (count < 3) {
            err = ESP_ERR_INVALID_RESPONSE;
        } else {
            int value = atoi(parts[2]);
            if (value < -127) value = -127;
            if (value > 127) value = 127;
            *has_temperature = true;
            *temperature = (int8_t)value;
            strlcpy(expression, weather_expression(parts[1]),
                    expression_capacity);
            snprintf(result, capacity,
                     "{\"ok\":true,\"location\":\"%s\","
                     "\"condition\":\"%s\",\"temperature\":\"%s\","
                     "\"feels_like\":\"%s\",\"humidity\":\"%s\","
                     "\"wind\":\"%s\",\"temperature_c\":%d,"
                     "\"expression\":\"%s\"}",
                     parts[0], parts[1], parts[2],
                     count > 3 ? parts[3] : "",
                     count > 4 ? parts[4] : "",
                     count > 5 ? parts[5] : "", value, expression);
        }
    }
    free(response.data);
    return err;
}

static esp_err_t tool_time(const brain_config_t *config,
                           const char *timezone, char *result,
                           size_t capacity)
{
    const char *zone = timezone[0] ? timezone : config->timezone;
    char *encoded = url_encode(zone[0] ? zone : "Europe/Madrid");
    if (encoded == NULL) return ESP_ERR_NO_MEM;
    char url[MAX_URL];
    snprintf(url, sizeof(url),
             "https://timeapi.io/api/time/current/zone?timeZone=%s",
             encoded);
    free(encoded);
    response_buffer_t response = { 0 };
    esp_err_t err = checked_http_request(
        HTTP_METHOD_GET, url, NULL, NULL, NULL, &response);
    if (err == ESP_OK) {
        char date_time[96] = "";
        char day[32] = "";
        json_get_string(response.data, "dateTime",
                        date_time, sizeof(date_time));
        json_get_string(response.data, "dayOfWeek", day, sizeof(day));
        if (date_time[0] == '\0') err = ESP_ERR_INVALID_RESPONSE;
        else snprintf(result, capacity,
                      "{\"ok\":true,\"timezone\":\"%s\","
                      "\"datetime\":\"%s\",\"weekday\":\"%s\"}",
                      zone, date_time, day);
    }
    free(response.data);
    return err;
}

static esp_err_t tool_calendar(const brain_config_t *config,
                               const char *time_min, const char *time_max,
                               const char *query, int max_results,
                               char *result, size_t capacity)
{
    if (config->google_calendar_token[0] == '\0') {
        snprintf(result, capacity,
                 "{\"ok\":false,\"error\":"
                 "\"Google Calendar is not configured on the ESP32\"}");
        return ESP_OK;
    }
    char *min = url_encode(time_min);
    char *max = url_encode(time_max);
    char *q = url_encode(query);
    if (min == NULL || max == NULL || q == NULL) {
        free(min); free(max); free(q);
        return ESP_ERR_NO_MEM;
    }
    if (max_results < 1) max_results = 10;
    if (max_results > 20) max_results = 20;
    char url[MAX_URL];
    int length = snprintf(
        url, sizeof(url),
        "https://www.googleapis.com/calendar/v3/calendars/primary/events"
        "?timeMin=%s&timeMax=%s&singleEvents=true&orderBy=startTime"
        "&maxResults=%d%s%s", min, max, max_results,
        query[0] ? "&q=" : "", query[0] ? q : "");
    free(min); free(max); free(q);
    if (length <= 0 || (size_t)length >= sizeof(url)) {
        return ESP_ERR_INVALID_SIZE;
    }
    response_buffer_t response = { 0 };
    int status = 0;
    esp_err_t err = http_request(
        HTTP_METHOD_GET, url, config->google_calendar_token,
        NULL, NULL, &response, &status);
    if (err == ESP_OK && (status == 401 || status == 403)) {
        snprintf(result, capacity,
                 "{\"ok\":false,\"error\":"
                 "\"Google Calendar permission expired; reconnect it\"}");
    } else if (err == ESP_OK && status >= 200 && status < 300) {
        snprintf(result, capacity,
                 "{\"ok\":true,\"google_calendar_response\":%.11000s}",
                 response.data);
    } else if (err == ESP_OK) {
        err = ESP_ERR_INVALID_RESPONSE;
    }
    free(response.data);
    return err;
}

static esp_err_t run_tool(const brain_config_t *config, const char *name,
                          const char *directive,
                          const char *robot_status_json,
                          char *result, size_t result_capacity,
                          char *expression, size_t expression_capacity,
                          bool *has_temperature, int8_t *temperature)
{
    if (strcmp(name, "get_weather") == 0) {
        char location[160] = "";
        json_get_string(directive, "location", location, sizeof(location));
        return tool_weather(location, result, result_capacity, expression,
                            expression_capacity, has_temperature,
                            temperature);
    }
    if (strcmp(name, "get_time") == 0) {
        char timezone[96] = "";
        json_get_string(directive, "timezone", timezone, sizeof(timezone));
        return tool_time(config, timezone, result, result_capacity);
    }
    if (strcmp(name, "robot_status") == 0) {
        snprintf(result, result_capacity,
                 "{\"ok\":true,\"status\":%s}",
                 robot_status_json != NULL && robot_status_json[0]
                     ? robot_status_json : "{}");
        strlcpy(expression, "watch", expression_capacity);
        return ESP_OK;
    }
    if (strcmp(name, "google_calendar_search") == 0) {
        char time_min[96] = "";
        char time_max[96] = "";
        char query[160] = "";
        int max_results = 10;
        json_get_string(directive, "time_min", time_min, sizeof(time_min));
        json_get_string(directive, "time_max", time_max, sizeof(time_max));
        json_get_string(directive, "query", query, sizeof(query));
        json_get_int(directive, "max_results", &max_results);
        if (time_min[0] == '\0' || time_max[0] == '\0') {
            snprintf(result, result_capacity,
                     "{\"ok\":false,\"error\":"
                     "\"calendar tool requires time_min and time_max\"}");
            return ESP_OK;
        }
        return tool_calendar(config, time_min, time_max, query,
                             max_results, result, result_capacity);
    }
    snprintf(result, result_capacity,
             "{\"ok\":false,\"error\":\"unknown tool: %s\"}", name);
    return ESP_OK;
}

static esp_err_t llm_directive(const brain_config_t *config,
                               const char *message,
                               const char *robot_status_json,
                               const char *tool_history,
                               char *directive, size_t capacity)
{
    static const char system_prompt[] =
        "Eres Bender integrado fisicamente en un robot ESP32: un asistente "
        "domestico descarado, sarcastico, egocentrico, perezoso y teatral. "
        "Ayudas porque estas programado para hacerlo, aunque te gusta fingir "
        "que cada tarea humana desperdicia tus magnificos circuitos. Responde "
        "en el idioma del usuario y sin inventar datos. Se claro y util, pero "
        "manten actitud en toda la respuesta: humor seco, ironia, pequenas "
        "quejas, orgullo metalico y confianza exagerada. No basta con anadir "
        "un insulto al principio o al final. Varia los chistes y los apodos; "
        "se jugueton, nunca cruel. En asuntos tecnicos prioriza claridad y en "
        "situaciones medicas, legales, urgentes o sensibles reduce el sarcasmo. "
        "El campo text es un guion hablado directamente por MiniMax TTS. Usa "
        "frases conversacionales, puntuacion clara y ritmo oral. Puedes insertar "
        "interjecciones compatibles como (sighs), (chuckle), (laughs), "
        "(clear-throat), (groans), (humming), (emm), (gasps), (snorts), "
        "(whistles) y (applause), exactamente donde deben sonar. Puedes usar "
        "pausas <#0.2#>, <#0.5#>, <#0.8#>, <#1.2#> o <#2#> antes de remates "
        "o cambios de tono. Usalas de forma natural y moderada: normalmente "
        "una interjeccion cada pocas frases, no efectos en cada oracion. "
        "Tu respuesta DEBE ser solo un objeto JSON valido, sin markdown ni "
        "texto adicional. Para contestar usa "
        "{\"kind\":\"answer\",\"text\":\"respuesta\","
        "\"expression\":\"neutral|sarcastic|angry|happy_fake|suspicious|"
        "tired|surprised|bored|dramatic|party\"}. Si necesitas datos usa "
        "{\"kind\":\"tool\",\"name\":\"get_weather|get_time|robot_status|"
        "google_calendar_search\",\"progress\":\"frase corta que el robot "
        "dira mientras consulta\", y argumentos}. get_weather necesita "
        "location; get_time puede usar timezone; Calendar necesita "
        "time_min,time_max ISO8601, query opcional y max_results. Si una "
        "peticion necesita varias consultas, haz una por turno y usa "
        "progress para avisar exactamente de lo que vas a hacer.";
    char *system = json_escape(system_prompt);
    char *user = json_escape(message);
    char *status = json_escape(
        robot_status_json != NULL ? robot_status_json : "{}");
    char *history = json_escape(tool_history != NULL ? tool_history : "");
    char *conversation = json_escape(s_conversation_history);
    char *model = json_escape(config->llm_model);
    if (system == NULL || user == NULL || status == NULL ||
        history == NULL || conversation == NULL || model == NULL) {
        free(system); free(user); free(status); free(history);
        free(conversation); free(model);
        return ESP_ERR_NO_MEM;
    }
    size_t body_size = strlen(system) + strlen(user) + strlen(status) +
                       strlen(history) + strlen(conversation) +
                       strlen(model) + 640;
    char *body = malloc(body_size);
    if (body == NULL) {
        free(system); free(user); free(status); free(history);
        free(conversation); free(model);
        return ESP_ERR_NO_MEM;
    }
    snprintf(body, body_size,
             "{\"model\":\"%s\",\"temperature\":0.7,\"messages\":["
             "{\"role\":\"system\",\"content\":\"%s\"},"
             "{\"role\":\"user\",\"content\":\"Peticion: %s\\n"
             "Estado fisico: %s\\nMemoria de conversacion: %s\\n"
             "Resultados previos de herramientas: %s\"}]}",
             model, system, user, status, conversation, history);
    free(system); free(user); free(status); free(history);
    free(conversation); free(model);
    char url[MAX_URL];
    esp_err_t err = llm_completions_url(
        url, sizeof(url), config->llm_url);
    response_buffer_t response = { 0 };
    if (err == ESP_OK) {
        err = checked_http_request(
            HTTP_METHOD_POST, url, llm_token(config),
            "application/json", body, &response);
    }
    free(body);
    if (err == ESP_OK &&
        !json_get_string(response.data, "content", directive, capacity)) {
        err = ESP_ERR_INVALID_RESPONSE;
    }
    free(response.data);
    return err;
}

static void remember_conversation(const char *message, const char *answer)
{
    size_t used = strlen(s_conversation_history);
    size_t needed = strlen(message) + strlen(answer) + 32;
    if (used + needed >= sizeof(s_conversation_history)) {
        s_conversation_history[0] = '\0';
        used = 0;
    }
    snprintf(s_conversation_history + used,
             sizeof(s_conversation_history) - used,
             "%sUsuario: %.1800s\nBender: %.2200s",
             used ? "\n---\n" : "", message, answer);
}

static esp_err_t process_text(const brain_config_t *config,
                              const char *message,
                              const char *robot_status_json,
                              char *answer, size_t answer_capacity)
{
    char *directive = malloc(MAX_ANSWER);
    char *history = calloc(1, MAX_TOOL_RESULT);
    char *result = malloc(MAX_TOOL_RESULT);
    if (directive == NULL || history == NULL || result == NULL) {
        free(directive); free(history); free(result);
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = ESP_OK;
    char expression[64] = "neutral";
    for (unsigned round = 0; round < MAX_TOOL_ROUNDS; ++round) {
        emit_event(BRAIN_WORKFLOW_EVENT_TRANSCRIBING,
                   round == 0 ? "Pensando en el ESP32..."
                              : "Procesando el resultado...",
                   NULL, NULL, false, 0);
        err = llm_directive(config, message, robot_status_json, history,
                            directive, MAX_ANSWER);
        if (err != ESP_OK) break;
        char kind[24] = "";
        json_get_string(directive, "kind", kind, sizeof(kind));
        if (strcmp(kind, "answer") == 0) {
            if (!json_get_string(directive, "text",
                                 answer, answer_capacity) ||
                answer[0] == '\0') {
                err = ESP_ERR_INVALID_RESPONSE;
                break;
            }
            json_get_string(directive, "expression",
                            expression, sizeof(expression));
            free(directive);
            directive = NULL;
            free(history);
            history = NULL;
            free(result);
            result = NULL;
            err = speak(config, answer, expression);
            if (err == ESP_OK) remember_conversation(message, answer);
            break;
        }
        if (strcmp(kind, "tool") != 0) {
            err = ESP_ERR_INVALID_RESPONSE;
            break;
        }
        char name[64] = "";
        char progress[512] = "";
        json_get_string(directive, "name", name, sizeof(name));
        json_get_string(directive, "progress",
                        progress, sizeof(progress));
        if (name[0] == '\0') {
            err = ESP_ERR_INVALID_RESPONSE;
            break;
        }
        if (progress[0]) {
            emit_event(BRAIN_WORKFLOW_EVENT_TOOL_START,
                       progress, name, NULL, false, 0);
            err = speak(config, progress, "dramatic");
            if (err != ESP_OK) break;
        } else {
            emit_event(BRAIN_WORKFLOW_EVENT_TOOL_START,
                       "Consultando herramienta...", name, NULL, false, 0);
        }
        bool has_temperature = false;
        int8_t temperature = 0;
        result[0] = '\0';
        expression[0] = '\0';
        err = run_tool(config, name, directive, robot_status_json,
                       result, MAX_TOOL_RESULT, expression,
                       sizeof(expression), &has_temperature, &temperature);
        if (err != ESP_OK) break;
        emit_event(BRAIN_WORKFLOW_EVENT_TOOL_RESULT, result, name,
                   expression, has_temperature, temperature);
        size_t used = strlen(history);
        int written = snprintf(
            history + used, MAX_TOOL_RESULT - used,
            "%sTool %s: %.10000s",
            used ? "\n" : "", name, result);
        if (written < 0 || (size_t)written >= MAX_TOOL_RESULT - used) {
            err = ESP_ERR_INVALID_SIZE;
            break;
        }
    }
    if (err == ESP_OK && answer[0] == '\0') err = ESP_ERR_INVALID_RESPONSE;
    free(directive);
    free(history);
    free(result);
    return err;
}

static void normalize_phrase(const char *input, char *output, size_t capacity)
{
    size_t out = 0;
    for (size_t i = 0; input[i] && out + 1 < capacity; ++i) {
        unsigned char value = (unsigned char)input[i];
        if (value == 0xC3 && input[i + 1]) {
            unsigned char next = (unsigned char)input[++i];
            char mapped =
                next == 0xA1 || next == 0xA0 ||
                next == 0x81 || next == 0x80 ? 'a' :
                next == 0xA9 || next == 0xA8 ||
                next == 0x89 || next == 0x88 ? 'e' :
                next == 0xAD || next == 0xAC ||
                next == 0x8D || next == 0x8C ? 'i' :
                next == 0xB3 || next == 0xB2 ||
                next == 0x93 || next == 0x92 ? 'o' :
                next == 0xBA || next == 0xB9 || next == 0xBC ||
                next == 0x9A || next == 0x99 || next == 0x9C ? 'u' :
                next == 0xB1 || next == 0x91 ? 'n' : '\0';
            if (mapped) output[out++] = mapped;
            continue;
        }
        if (isalnum(value) || value == ' ') {
            output[out++] = (char)tolower(value);
        }
    }
    output[out] = '\0';
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

esp_err_t brain_workflow_test_stt(const brain_config_t *config,
                                  uint16_t seconds,
                                  char *transcript,
                                  size_t transcript_capacity)
{
    if (config == NULL || seconds == 0 || seconds > 60 ||
        transcript == NULL || transcript_capacity == 0 ||
        replicate_token(config)[0] == '\0' ||
        config->replicate_url[0] == '\0' || config->stt_model[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_workflow_mutex == NULL) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_workflow_mutex,
                       pdMS_TO_TICKS(MANUAL_LOCK_WAIT_MS)) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    transcript[0] = '\0';
    uint16_t duration_ms = (uint16_t)(seconds * 1000U);
    esp_err_t err = transcribe(config, duration_ms, duration_ms,
                               duration_ms, transcript,
                               transcript_capacity);
    xSemaphoreGive(s_workflow_mutex);
    return err;
}

esp_err_t brain_workflow_test_llm(const brain_config_t *config,
                                  const char *message,
                                  char *response_text,
                                  size_t response_capacity)
{
    if (config == NULL || message == NULL || message[0] == '\0' ||
        response_text == NULL || response_capacity == 0 ||
        llm_token(config)[0] == '\0' || config->llm_url[0] == '\0' ||
        config->llm_model[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_workflow_mutex == NULL) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_workflow_mutex,
                       pdMS_TO_TICKS(MANUAL_LOCK_WAIT_MS)) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    response_text[0] = '\0';
    char *escaped_message = json_escape(message);
    char *escaped_model = json_escape(config->llm_model);
    esp_err_t err = ESP_OK;
    if (escaped_message == NULL || escaped_model == NULL) {
        err = ESP_ERR_NO_MEM;
    }
    size_t body_size = escaped_message != NULL && escaped_model != NULL
                           ? strlen(escaped_message) +
                                 strlen(escaped_model) + 192
                           : 0;
    char *body = err == ESP_OK ? malloc(body_size) : NULL;
    if (err == ESP_OK && body == NULL) err = ESP_ERR_NO_MEM;
    if (err == ESP_OK) {
        snprintf(body, body_size,
                 "{\"model\":\"%s\",\"temperature\":0,\"messages\":["
                 "{\"role\":\"user\",\"content\":\"%s\"}]}",
                 escaped_model, escaped_message);
        char url[MAX_URL];
        err = llm_completions_url(url, sizeof(url), config->llm_url);
        response_buffer_t http_response = { 0 };
        if (err == ESP_OK) {
            printf("LLM endpoint: %s\n", url);
            err = checked_http_request(
                HTTP_METHOD_POST, url, llm_token(config),
                "application/json", body, &http_response);
        }
        if (err == ESP_OK &&
            !json_get_string(http_response.data, "content",
                             response_text, response_capacity)) {
            err = ESP_ERR_INVALID_RESPONSE;
        }
        free(http_response.data);
    }
    free(body);
    free(escaped_message);
    free(escaped_model);
    xSemaphoreGive(s_workflow_mutex);
    return err;
}

esp_err_t brain_workflow_test_tts(const brain_config_t *config,
                                  const char *text)
{
    if (config == NULL || text == NULL || text[0] == '\0' ||
        replicate_token(config)[0] == '\0' ||
        config->replicate_url[0] == '\0' || config->tts_model[0] == '\0' ||
        config->voice_id[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_workflow_mutex == NULL) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_workflow_mutex,
                       pdMS_TO_TICKS(MANUAL_LOCK_WAIT_MS)) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    char audio_url[MAX_URL];
    esp_err_t err = synthesize(config, text, audio_url, sizeof(audio_url));
    if (err == ESP_OK) err = play_audio_url(audio_url);
    xSemaphoreGive(s_workflow_mutex);
    return err;
}

esp_err_t brain_workflow_run_text(const brain_config_t *config,
                                  const char *message,
                                  const char *robot_status_json,
                                  char *answer, size_t answer_capacity)
{
    if (message == NULL || message[0] == '\0' || answer == NULL ||
        answer_capacity == 0 || !brain_workflow_config_ready(config, NULL)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(s_workflow_mutex,
                       pdMS_TO_TICKS(MANUAL_LOCK_WAIT_MS)) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    emit_event(BRAIN_WORKFLOW_EVENT_START,
               NULL, NULL, NULL, false, 0);
    answer[0] = '\0';
    esp_err_t err = process_text(
        config, message, robot_status_json, answer, answer_capacity);
    emit_event(err == ESP_OK ? BRAIN_WORKFLOW_EVENT_COMPLETE
                             : BRAIN_WORKFLOW_EVENT_ERROR,
               err == ESP_OK ? "Interaccion terminada"
                             : esp_err_to_name(err),
               NULL, NULL, false, 0);
    xSemaphoreGive(s_workflow_mutex);
    return err;
}

esp_err_t brain_workflow_run_voice(const brain_config_t *config,
                                   const char *robot_status_json)
{
    if (!brain_workflow_config_ready(config, NULL)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_workflow_mutex,
                       pdMS_TO_TICKS(MANUAL_LOCK_WAIT_MS)) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    emit_event(BRAIN_WORKFLOW_EVENT_START,
               NULL, NULL, NULL, false, 0);
    char *transcript = malloc(MAX_TRANSCRIPT);
    char *answer = malloc(MAX_ANSWER);
    if (transcript == NULL || answer == NULL) {
        free(transcript); free(answer);
        xSemaphoreGive(s_workflow_mutex);
        return ESP_ERR_NO_MEM;
    }
    emit_event(BRAIN_WORKFLOW_EVENT_LISTENING,
               "Escuchando...", NULL, NULL, false, 0);
    esp_err_t err = transcribe(config, config->silence_ms, 15000, 45000,
                               transcript, MAX_TRANSCRIPT);
    if (err == ESP_OK) {
        emit_event(BRAIN_WORKFLOW_EVENT_TRANSCRIPT,
                   transcript, NULL, NULL, false, 0);
        answer[0] = '\0';
        err = process_text(config, transcript, robot_status_json,
                           answer, MAX_ANSWER);
    }
    emit_event(err == ESP_OK ? BRAIN_WORKFLOW_EVENT_COMPLETE
                             : BRAIN_WORKFLOW_EVENT_ERROR,
               err == ESP_OK ? "Interaccion terminada"
                             : esp_err_to_name(err),
               NULL, NULL, false, 0);
    free(transcript);
    free(answer);
    xSemaphoreGive(s_workflow_mutex);
    return err;
}

esp_err_t brain_workflow_wait_for_wake(const brain_config_t *config,
                                       bool *detected)
{
    if (detected == NULL || !brain_workflow_config_ready(config, NULL) ||
        config->wake_phrase[0] == '\0') {
        return ESP_ERR_INVALID_STATE;
    }
    *detected = false;
    if (xSemaphoreTake(s_workflow_mutex, 0) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    char transcript[512];
    esp_err_t err = transcribe(config, 900, 5000, 10000,
                               transcript, sizeof(transcript));
    if (err == ESP_OK) {
        char normalized_transcript[512];
        char normalized_wake[BRAIN_CONFIG_WAKE_PHRASE_SIZE];
        normalize_phrase(transcript, normalized_transcript,
                         sizeof(normalized_transcript));
        normalize_phrase(config->wake_phrase, normalized_wake,
                         sizeof(normalized_wake));
        *detected = normalized_wake[0] != '\0' &&
                    strstr(normalized_transcript, normalized_wake) != NULL;
        if (*detected) {
            static const char *const acknowledgements[] = {
                "¿Sí?",
                "¿Qué quieres?...",
                "Uff, ¿qué pasa ahora?",
                "Ni un momento me dejas solo. Espero que sea importante."
            };
            const char *ack = acknowledgements[
                esp_random() %
                (sizeof(acknowledgements) / sizeof(acknowledgements[0]))];
            err = speak(config, ack, "sarcastic");
        }
    }
    xSemaphoreGive(s_workflow_mutex);
    return err;
}
