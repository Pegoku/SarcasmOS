#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "brain_config.h"
#include "esp_err.h"

typedef enum {
    BRAIN_WORKFLOW_EVENT_START,
    BRAIN_WORKFLOW_EVENT_LISTENING,
    BRAIN_WORKFLOW_EVENT_TRANSCRIBING,
    BRAIN_WORKFLOW_EVENT_TRANSCRIPT,
    BRAIN_WORKFLOW_EVENT_TOOL_START,
    BRAIN_WORKFLOW_EVENT_TOOL_RESULT,
    BRAIN_WORKFLOW_EVENT_SYNTHESIZING,
    BRAIN_WORKFLOW_EVENT_SPEAKING,
    BRAIN_WORKFLOW_EVENT_AUDIO_LEVEL,
    BRAIN_WORKFLOW_EVENT_COMPLETE,
    BRAIN_WORKFLOW_EVENT_ERROR,
} brain_workflow_event_type_t;

typedef struct {
    brain_workflow_event_type_t type;
    const char *message;
    const char *tool;
    const char *expression;
    bool has_temperature;
    int8_t temperature_c;
    uint8_t audio_level;
} brain_workflow_event_t;

typedef void (*brain_workflow_event_handler_t)(
    const brain_workflow_event_t *event, void *context);

esp_err_t brain_workflow_init(brain_workflow_event_handler_t handler,
                              void *context);
bool brain_workflow_config_ready(const brain_config_t *config,
                                 const char **missing);
esp_err_t brain_workflow_run_voice(const brain_config_t *config,
                                   const char *robot_status_json);
esp_err_t brain_workflow_run_text(const brain_config_t *config,
                                  const char *message,
                                  const char *robot_status_json,
                                  char *answer, size_t answer_capacity);
esp_err_t brain_workflow_wait_for_wake(const brain_config_t *config,
                                       bool *detected);
esp_err_t brain_workflow_test_stt(const brain_config_t *config,
                                  uint16_t seconds,
                                  char *transcript,
                                  size_t transcript_capacity);
esp_err_t brain_workflow_test_llm(const brain_config_t *config,
                                  const char *message,
                                  char *response,
                                  size_t response_capacity);
esp_err_t brain_workflow_test_tts(const brain_config_t *config,
                                  const char *text);
