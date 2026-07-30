#include <stdbool.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "brain_audio.h"
#include "brain_config.h"
#include "display_protocol.h"
#include "eye_protocol.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_eth.h"
#include "esp_eth_mac_spi.h"
#include "esp_eth_netif_glue.h"
#include "esp_eth_phy.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mouth_espnow.h"
#include "nvs_flash.h"
#include "driver/spi_master.h"

#define PIN_5V_EN GPIO_NUM_1
#define PIN_5VHP_EN GPIO_NUM_2
#define PIN_CHARGER_CE GPIO_NUM_6
#define PIN_BQ_INT GPIO_NUM_7
#define PIN_I2C_SDA GPIO_NUM_8
#define PIN_I2C_SCL GPIO_NUM_9
#define PIN_ETH_CS GPIO_NUM_10
#define PIN_ETH_MOSI GPIO_NUM_11
#define PIN_ETH_SCLK GPIO_NUM_12
#define PIN_ETH_MISO GPIO_NUM_13
#define PIN_ETH_INT GPIO_NUM_14
#define PIN_ETH_RST GPIO_NUM_15
#define PIN_TMC_STEP GPIO_NUM_16
#define PIN_TMC_DIR GPIO_NUM_17
#define PIN_TMC_EN GPIO_NUM_18
#define PIN_TMC_UART GPIO_NUM_21
#define PIN_TMC_DIAG GPIO_NUM_38
#define PIN_I2S_BCLK GPIO_NUM_39
#define PIN_I2S_LRCLK GPIO_NUM_40
#define PIN_I2S_DOUT GPIO_NUM_41
#define PIN_FUEL_ALERT GPIO_NUM_42
#define PIN_I2S_DIN GPIO_NUM_47
#define PIN_STATUS_LED GPIO_NUM_48

#define I2C_TIMEOUT_MS 80
#define WIFI_CONNECTED_BIT BIT0

#define ADDR_LEFT_EYE 0x30
#define ADDR_RIGHT_EYE 0x31

#define PROTO_VERSION EYE_PROTOCOL_VERSION_TRANSITIONS
#define CMD_PING DISPLAY_CMD_PING
#define CMD_GET_INFO DISPLAY_CMD_GET_INFO
#define CMD_SET_BRIGHTNESS DISPLAY_CMD_SET_BRIGHTNESS
#define CMD_SET_ANIMATION DISPLAY_CMD_SET_ANIMATION
#define CMD_SET_EXPRESSION DISPLAY_CMD_SET_EXPRESSION
#define CMD_SYNC DISPLAY_CMD_SYNC
#define CMD_STOP DISPLAY_CMD_STOP
#define CMD_SET_PARAM DISPLAY_CMD_SET_PARAM
#define CMD_DEBUG_FRAME DISPLAY_CMD_DEBUG_FRAME
#define CMD_RESET DISPLAY_CMD_RESET

#define ANIM_IDLE DISPLAY_ANIM_IDLE
#define ANIM_LISTENING DISPLAY_ANIM_LISTENING
#define ANIM_THINKING DISPLAY_ANIM_THINKING
#define ANIM_THINKING_AUDIO DISPLAY_ANIM_THINKING_AUDIO
#define ANIM_THINKING_LONG DISPLAY_ANIM_THINKING_LONG
#define ANIM_SPEAKING DISPLAY_ANIM_SPEAKING
#define ANIM_HAPPY DISPLAY_ANIM_HAPPY
#define ANIM_ANGRY DISPLAY_ANIM_ANGRY
#define ANIM_ERROR DISPLAY_ANIM_ERROR
#define ANIM_SLEEP DISPLAY_ANIM_SLEEP

#define EYE_COUNT 2
#define EYE_LEFT_INDEX 0
#define EYE_RIGHT_INDEX 1
#define EYE_LEFT_MASK (1U << EYE_LEFT_INDEX)
#define EYE_RIGHT_MASK (1U << EYE_RIGHT_INDEX)
#define EYE_BOTH_MASK (EYE_LEFT_MASK | EYE_RIGHT_MASK)

typedef enum {
    STATE_BOOTING,
    STATE_IDLE,
    STATE_LISTENING,
    STATE_THINKING,
    STATE_SPEAKING,
    STATE_ERROR,
    STATE_SLEEP,
} assistant_state_t;

typedef struct {
    const char *name;
    uint8_t address;
    bool present;
    uint8_t last_sequence;
    uint8_t last_result;
    uint8_t role;
    uint8_t fw_major;
    uint8_t fw_minor;
    uint8_t active_animation;
    uint8_t pending_animation;
    uint8_t active_transition_token;
    uint8_t pending_transition_token;
    uint8_t playback_flags;
    uint8_t current_frame;
    uint8_t last_error;
    uint32_t last_seen_ms;
    uint32_t status_read_ms;
    i2c_master_dev_handle_t i2c_handle;
} display_device_t;

typedef struct {
    uint8_t animation;
    bool emergency;
} face_request_t;

typedef struct {
    uint8_t desired_animation;
    uint8_t committed_animation;
    uint8_t token;
    uint8_t waiting_mask;
    uint32_t started_ms;
    bool degraded;
} face_transition_status_t;

static const char *TAG = "sarcasmos-brain";
static display_device_t g_displays[] = {
    {
        .name = "left_eye",
        .address = ADDR_LEFT_EYE,
        .role = EYE_LEFT_INDEX,
        .pending_animation = EYE_PROTOCOL_NO_PENDING_ANIMATION,
    },
    {
        .name = "right_eye",
        .address = ADDR_RIGHT_EYE,
        .role = EYE_RIGHT_INDEX,
        .pending_animation = EYE_PROTOCOL_NO_PENDING_ANIMATION,
    },
};
static assistant_state_t g_state = STATE_BOOTING;
static uint8_t g_sequence = 1;
static uint8_t g_brightness = 160;
static bool g_wifi_connected;
static bool g_wifi_should_connect;
static bool g_ethernet_connected;
static bool g_mouth_initialized;
static bool g_usb_driver_ready;
static bool g_discard_line_feed;
static EventGroupHandle_t g_wifi_events;
static i2c_master_bus_handle_t g_i2c_bus;
static QueueHandle_t g_face_queue;
static SemaphoreHandle_t g_face_mutex;
static SemaphoreHandle_t g_display_mutex;
static brain_config_t g_config;
static face_transition_status_t g_face_transition = {
    .desired_animation = ANIM_IDLE,
    .committed_animation = ANIM_IDLE,
};

static uint8_t crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static const char *state_name(assistant_state_t state)
{
    switch (state) {
    case STATE_BOOTING: return "booting";
    case STATE_IDLE: return "idle";
    case STATE_LISTENING: return "listening";
    case STATE_THINKING: return "thinking";
    case STATE_SPEAKING: return "speaking";
    case STATE_ERROR: return "error";
    case STATE_SLEEP: return "sleep";
    default: return "unknown";
    }
}

static uint8_t animation_for_state(assistant_state_t state)
{
    switch (state) {
    case STATE_BOOTING: return ANIM_THINKING;
    case STATE_IDLE: return ANIM_IDLE;
    case STATE_LISTENING: return ANIM_LISTENING;
    case STATE_THINKING: return ANIM_THINKING;
    case STATE_SPEAKING: return ANIM_SPEAKING;
    case STATE_ERROR: return ANIM_ERROR;
    case STATE_SLEEP: return ANIM_SLEEP;
    default: return ANIM_ERROR;
    }
}

static assistant_state_t parse_state(const char *body)
{
    if (strstr(body, "listening")) return STATE_LISTENING;
    if (strstr(body, "thinking_audio")) return STATE_THINKING;
    if (strstr(body, "thinking-long") || strstr(body, "thinking_long")) return STATE_THINKING;
    if (strstr(body, "thinking")) return STATE_THINKING;
    if (strstr(body, "speaking")) return STATE_SPEAKING;
    if (strstr(body, "happy")) return STATE_IDLE;
    if (strstr(body, "angry")) return STATE_ERROR;
    if (strstr(body, "error")) return STATE_ERROR;
    if (strstr(body, "sleep")) return STATE_SLEEP;
    return STATE_IDLE;
}

static uint32_t uptime_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static esp_err_t eye_transmit_locked(display_device_t *eye, uint8_t command,
                                     const uint8_t *payload, uint8_t len)
{
    if (eye == NULL || eye->i2c_handle == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    uint8_t frame[4 + 64 + 1];
    if (len > 64) {
        return ESP_ERR_INVALID_SIZE;
    }
    frame[0] = PROTO_VERSION;
    frame[1] = command;
    frame[2] = g_sequence++;
    if (g_sequence == 0) {
        g_sequence = 1;
    }
    frame[3] = len;
    if (len > 0 && payload != NULL) {
        memcpy(&frame[4], payload, len);
    }
    frame[4 + len] = crc8(frame, 4 + len);
    esp_err_t err =
        i2c_master_transmit(eye->i2c_handle, frame, 5 + len, I2C_TIMEOUT_MS);
    eye->present = err == ESP_OK;
    if (err == ESP_OK) {
        eye->last_sequence = frame[2];
        eye->last_seen_ms = uptime_ms();
    }
    return err;
}

static esp_err_t eye_command(display_device_t *eye, uint8_t command,
                             const uint8_t *payload, uint8_t len)
{
    if (xSemaphoreTake(g_display_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    esp_err_t err = eye_transmit_locked(eye, command, payload, len);
    xSemaphoreGive(g_display_mutex);
    return err;
}

static esp_err_t eye_command_both(uint8_t command, const uint8_t *payload,
                                  uint8_t len)
{
    esp_err_t result = ESP_OK;
    for (size_t i = 0; i < EYE_COUNT; ++i) {
        esp_err_t err = eye_command(&g_displays[i], command, payload, len);
        if (err != ESP_OK) {
            result = err;
        }
    }
    return result;
}

static uint8_t eye_commit_ready(uint8_t ready_mask, uint8_t token)
{
    uint8_t committed_mask = 0;
    uint8_t payload[EYE_SYNC_PAYLOAD_SIZE];
    eye_protocol_encode_sync(payload, token, EYE_SYNC_START_DELAY_MS);
    if (xSemaphoreTake(g_display_mutex, portMAX_DELAY) != pdTRUE) {
        return 0;
    }
    for (size_t i = 0; i < EYE_COUNT; ++i) {
        const uint8_t mask = 1U << i;
        if ((ready_mask & mask) == 0) {
            continue;
        }
        esp_err_t err = eye_transmit_locked(
            &g_displays[i], CMD_SYNC, payload, sizeof(payload));
        if (err == ESP_OK) {
            committed_mask |= mask;
            ESP_LOGI(TAG, "%s commit sent token=%u delay=%u ms at %" PRIu32,
                     g_displays[i].name, token, EYE_SYNC_START_DELAY_MS,
                     uptime_ms());
        } else {
            ESP_LOGE(TAG, "%s commit failed: %s",
                     g_displays[i].name, esp_err_to_name(err));
        }
    }
    xSemaphoreGive(g_display_mutex);
    return committed_mask;
}

static esp_err_t mouth_command(uint8_t command, const uint8_t *payload,
                               uint8_t len)
{
#if CONFIG_SARCASMOS_MOUTH_ESPNOW
    if (g_mouth_initialized) {
        return mouth_espnow_send(command, payload, len, true);
    }
    return ESP_ERR_INVALID_STATE;
#else
    (void)command;
    (void)payload;
    (void)len;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

static esp_err_t display_command_non_animation_all(
    uint8_t command, const uint8_t *payload, uint8_t len)
{
    esp_err_t result = eye_command_both(command, payload, len);
    esp_err_t mouth_err = mouth_command(command, payload, len);
    if (mouth_err != ESP_OK && mouth_err != ESP_ERR_NOT_SUPPORTED &&
        mouth_err != ESP_ERR_INVALID_STATE) {
        result = mouth_err;
    }
    return result;
}

static esp_err_t eye_read_status(display_device_t *eye,
                                 eye_protocol_status_t *status)
{
    if (eye == NULL || status == NULL ||
        xSemaphoreTake(g_display_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t response[EYE_PROTOCOL_STATUS_SIZE] = { 0 };
    esp_err_t err = i2c_master_receive(
        eye->i2c_handle, response, sizeof(response), I2C_TIMEOUT_MS);
    if (err == ESP_OK &&
        !eye_protocol_decode_status(response, sizeof(response), eye->role,
                                    status)) {
        err = ESP_ERR_INVALID_RESPONSE;
    }
    if (err == ESP_OK &&
        (status->active_animation >= DISPLAY_ANIM_COUNT ||
         (status->pending_animation != EYE_PROTOCOL_NO_PENDING_ANIMATION &&
          status->pending_animation >= DISPLAY_ANIM_COUNT))) {
        err = ESP_ERR_INVALID_RESPONSE;
    }
    if (err == ESP_OK) {
        eye->present = true;
        eye->fw_major = status->firmware_major;
        eye->fw_minor = status->firmware_minor;
        eye->active_animation = status->active_animation;
        eye->pending_animation = status->pending_animation;
        eye->active_transition_token = status->active_transition_token;
        eye->pending_transition_token = status->pending_transition_token;
        eye->playback_flags = status->playback_flags;
        eye->current_frame = status->current_frame;
        eye->last_result = status->last_error;
        eye->last_error = status->last_error;
        eye->status_read_ms = uptime_ms();
        eye->last_seen_ms = eye->status_read_ms;
    }
    xSemaphoreGive(g_display_mutex);
    return err;
}

static bool emergency_animation(uint8_t animation)
{
    return animation == ANIM_ERROR || animation == ANIM_SLEEP ||
           animation == DISPLAY_ANIM_BATTERY_LOW;
}

static esp_err_t request_face_state(uint8_t animation, bool emergency)
{
    if (animation >= DISPLAY_ANIM_COUNT || g_face_queue == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    face_request_t request = {
        .animation = animation,
        .emergency = emergency || emergency_animation(animation),
    };
    if (xSemaphoreTake(g_face_mutex, portMAX_DELAY) == pdTRUE) {
        g_face_transition.desired_animation = animation;
        xSemaphoreGive(g_face_mutex);
    }
    return xQueueOverwrite(g_face_queue, &request) == pdPASS
               ? ESP_OK : ESP_FAIL;
}

static void configure_gpio(void)
{
    gpio_config_t out = {
        .pin_bit_mask = (1ULL << PIN_5V_EN) | (1ULL << PIN_5VHP_EN) | (1ULL << PIN_CHARGER_CE) |
                        (1ULL << PIN_STATUS_LED) | (1ULL << PIN_ETH_RST) | (1ULL << PIN_TMC_EN) |
                        (1ULL << PIN_TMC_STEP) | (1ULL << PIN_TMC_DIR),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&out));
    gpio_set_level(PIN_5V_EN, 1);
    gpio_set_level(PIN_5VHP_EN, 1);
    gpio_set_level(PIN_CHARGER_CE, 0);
    gpio_set_level(PIN_ETH_RST, 1);
    gpio_set_level(PIN_TMC_EN, 1);
    gpio_set_level(PIN_STATUS_LED, 0);

    gpio_config_t in = {
        .pin_bit_mask = (1ULL << PIN_BQ_INT) | (1ULL << PIN_FUEL_ALERT) | (1ULL << PIN_ETH_INT) | (1ULL << PIN_TMC_DIAG),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&in));
}

static void configure_i2c(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &g_i2c_bus));

    for (size_t i = 0; i < sizeof(g_displays) / sizeof(g_displays[0]); ++i) {
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = g_displays[i].address,
            .scl_speed_hz = CONFIG_SARCASMOS_I2C_FREQ_HZ,
        };
        ESP_ERROR_CHECK(i2c_master_bus_add_device(g_i2c_bus, &dev_cfg, &g_displays[i].i2c_handle));
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (g_wifi_should_connect) {
            esp_wifi_connect();
        }
    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        g_wifi_connected = false;
        xEventGroupClearBits(g_wifi_events, WIFI_CONNECTED_BIT);
        if (g_wifi_should_connect) {
            esp_wifi_connect();
        }
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        g_wifi_connected = true;
        xEventGroupSetBits(g_wifi_events, WIFI_CONNECTED_BIT);
    }
}

static void ensure_netif_event_loop(void)
{
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
}

static esp_err_t start_wifi(void)
{
#if CONFIG_SARCASMOS_ENABLE_WIFI || CONFIG_SARCASMOS_MOUTH_ESPNOW
    ensure_netif_event_loop();
    esp_netif_create_default_wifi_sta();
    g_wifi_events = xEventGroupCreate();
    if (g_wifi_events == NULL) {
        return ESP_ERR_NO_MEM;
    }
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "Wi-Fi init failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(
                            WIFI_EVENT, ESP_EVENT_ANY_ID,
                            wifi_event_handler, NULL),
                        TAG, "Wi-Fi event registration failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(
                            IP_EVENT, IP_EVENT_STA_GOT_IP,
                            wifi_event_handler, NULL),
                        TAG, "IP event registration failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM),
                        TAG, "Wi-Fi storage setup failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA),
                        TAG, "Wi-Fi mode setup failed");

#if CONFIG_SARCASMOS_ENABLE_WIFI
    g_wifi_should_connect = strlen(g_config.wifi_ssid) > 0;
    if (g_wifi_should_connect) {
        wifi_config_t wifi_config = { 0 };
        strlcpy((char *)wifi_config.sta.ssid, g_config.wifi_ssid,
                sizeof(wifi_config.sta.ssid));
        strlcpy((char *)wifi_config.sta.password, g_config.wifi_password,
                sizeof(wifi_config.sta.password));
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config),
                            TAG, "Wi-Fi config failed");
    }
#endif

    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Wi-Fi start failed");
    if (g_wifi_should_connect) {
        EventBits_t bits = xEventGroupWaitBits(
            g_wifi_events, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE,
            pdMS_TO_TICKS(15000));
        if ((bits & WIFI_CONNECTED_BIT) == 0) {
            ESP_LOGW(TAG, "Wi-Fi connection timed out; ESP-NOW may use the wrong channel");
        }
    } else {
#if CONFIG_SARCASMOS_MOUTH_ESPNOW
        ESP_RETURN_ON_ERROR(
            esp_wifi_set_channel(CONFIG_SARCASMOS_ESPNOW_CHANNEL,
                                 WIFI_SECOND_CHAN_NONE),
            TAG, "ESP-NOW channel setup failed");
#endif
    }
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

static void start_mouth_espnow(void)
{
#if CONFIG_SARCASMOS_MOUTH_ESPNOW
    uint8_t mac[6];
    if (!mouth_espnow_parse_mac(CONFIG_SARCASMOS_MOUTH_MAC, mac)) {
        ESP_LOGE(TAG, "invalid or empty ESP-NOW mouth MAC; mouth disabled");
        return;
    }
    mouth_espnow_config_t config = {
        .peer_channel = 0,
        .ack_timeout_ms = CONFIG_SARCASMOS_ESPNOW_ACK_TIMEOUT_MS,
        .retries = CONFIG_SARCASMOS_ESPNOW_RETRIES,
    };
    memcpy(config.mac, mac, sizeof(config.mac));
    esp_err_t err = mouth_espnow_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW mouth init failed: %s", esp_err_to_name(err));
        return;
    }
    g_mouth_initialized = true;
    ESP_LOGI(TAG, "ESP-NOW mouth peer %s ready", CONFIG_SARCASMOS_MOUTH_MAC);
#endif
}

#if CONFIG_SARCASMOS_ENABLE_ETHERNET
static void ethernet_event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    if (base == ETH_EVENT && event_id == ETHERNET_EVENT_CONNECTED) {
        g_ethernet_connected = true;
    } else if (base == ETH_EVENT && event_id == ETHERNET_EVENT_DISCONNECTED) {
        g_ethernet_connected = false;
    }
}
#endif

static void start_ethernet(void)
{
#if CONFIG_SARCASMOS_ENABLE_ETHERNET
    ensure_netif_event_loop();

    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_ETH_MOSI,
        .miso_io_num = PIN_ETH_MISO,
        .sclk_io_num = PIN_ETH_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = CONFIG_SARCASMOS_W5500_SPI_FREQ_HZ,
        .spics_io_num = PIN_ETH_CS,
        .queue_size = 20,
    };
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(SPI2_HOST, &devcfg);
    w5500_config.int_gpio_num = PIN_ETH_INT;
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.reset_gpio_num = PIN_ETH_RST;
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, ethernet_event_handler, NULL));
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
#endif
}

static void display_health_task(void *arg)
{
    while (true) {
        for (size_t i = 0; i < EYE_COUNT; ++i) {
            eye_protocol_status_t status;
            esp_err_t err = eye_read_status(&g_displays[i], &status);
            if (err != ESP_OK && err != ESP_ERR_INVALID_RESPONSE) {
                ESP_LOGW(TAG, "%s health check failed: %s",
                         g_displays[i].name, esp_err_to_name(err));
            }
        }
#if CONFIG_SARCASMOS_MOUTH_ESPNOW
        if (g_mouth_initialized) {
            mouth_command(CMD_PING, NULL, 0);
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

static uint8_t next_transition_token(uint8_t current)
{
    ++current;
    return current == 0 ? 1 : current;
}

static void set_transition_runtime(uint8_t desired, uint8_t token,
                                   uint8_t waiting_mask, bool degraded)
{
    if (xSemaphoreTake(g_face_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    g_face_transition.desired_animation = desired;
    g_face_transition.token = token;
    g_face_transition.waiting_mask = waiting_mask;
    g_face_transition.started_ms = uptime_ms();
    g_face_transition.degraded = degraded;
    xSemaphoreGive(g_face_mutex);
}

static void update_transition_waiting(uint8_t waiting_mask, bool degraded)
{
    if (xSemaphoreTake(g_face_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    g_face_transition.waiting_mask = waiting_mask;
    g_face_transition.degraded |= degraded;
    xSemaphoreGive(g_face_mutex);
}

static void commit_transition(uint8_t animation, uint8_t token, bool degraded,
                              bool mouth_acknowledged)
{
    if (xSemaphoreTake(g_face_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    if (g_face_transition.token == token) {
        if (mouth_acknowledged) {
            g_face_transition.committed_animation = animation;
        }
        g_face_transition.waiting_mask = 0;
        g_face_transition.degraded |= degraded || !mouth_acknowledged;
    }
    xSemaphoreGive(g_face_mutex);
}

static void log_eye_timeout(size_t index, uint8_t animation, uint8_t token)
{
    display_device_t snapshot;
    if (xSemaphoreTake(g_display_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    snapshot = g_displays[index];
    g_displays[index].present = false;
    xSemaphoreGive(g_display_mutex);
    ESP_LOGE(
        TAG,
        "%s transition timeout target=0x%02X token=%u active=0x%02X "
        "active_token=%u pending=0x%02X pending_token=%u flags=0x%02X "
        "frame=%u error=%u status_ms=%" PRIu32,
        snapshot.name, animation, token, snapshot.active_animation,
        snapshot.active_transition_token, snapshot.pending_animation,
        snapshot.pending_transition_token, snapshot.playback_flags,
        snapshot.current_frame, snapshot.last_error, snapshot.status_read_ms);
}

static void face_transition_task(void *arg)
{
    face_request_t request;
    uint8_t token = 0;
    while (true) {
        if (xQueueReceive(g_face_queue, &request, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        unsigned eye_retry_count = 0;

restart_transition:
        token = next_transition_token(token);
        uint8_t duration_ticks =
            request.emergency
                ? 1
                : (uint8_t)((CONFIG_SARCASMOS_FACE_TRANSITION_MS +
                             DISPLAY_TRANSITION_TICK_MS - 1) /
                            DISPLAY_TRANSITION_TICK_MS);
        uint8_t payload[3] = {
            request.animation, token, duration_ticks
        };
        set_transition_runtime(
            request.animation, token, EYE_BOTH_MASK, false);

        ESP_LOGI(TAG, "face request target=0x%02X token=%u emergency=%s at %" PRIu32,
                 request.animation, token, request.emergency ? "true" : "false",
                 uptime_ms());

        uint8_t eye_sent_mask = 0;
        for (size_t i = 0; i < EYE_COUNT; ++i) {
            esp_err_t err = eye_command(
                &g_displays[i], CMD_SET_ANIMATION, payload, sizeof(payload));
            if (err == ESP_OK) {
                eye_sent_mask |= (1U << i);
                ESP_LOGI(TAG, "%s request sent token=%u at %" PRIu32,
                         g_displays[i].name, token, uptime_ms());
            } else {
                ESP_LOGE(TAG, "%s request failed: %s",
                         g_displays[i].name, esp_err_to_name(err));
            }
        }

        bool degraded = eye_sent_mask != EYE_BOTH_MASK;
        uint8_t ready_mask = 0;
        uint32_t started = uptime_ms();
        uint32_t timeout_ms = eye_sent_mask == 0
                                  ? CONFIG_SARCASMOS_NO_EYE_TIMEOUT_MS
                                  : CONFIG_SARCASMOS_EYE_BARRIER_TIMEOUT_MS;
        bool replaced = false;
        while (ready_mask != eye_sent_mask &&
               uptime_ms() - started < timeout_ms) {
            face_request_t newer;
            if (xQueueReceive(g_face_queue, &newer, 0) == pdTRUE) {
                request = newer;
                replaced = true;
                ESP_LOGI(TAG, "face request replaced before barrier");
                break;
            }

            for (size_t i = 0; i < EYE_COUNT; ++i) {
                uint8_t mask = 1U << i;
                if ((ready_mask & mask) != 0 || (eye_sent_mask & mask) == 0) {
                    continue;
                }
                eye_protocol_status_t status;
                esp_err_t err = eye_read_status(&g_displays[i], &status);
                if (err == ESP_OK &&
                    eye_protocol_transition_ready(
                        &status, request.animation, token)) {
                    ready_mask |= mask;
                    ESP_LOGI(TAG, "%s READY target=0x%02X token=%u frame=%u at %" PRIu32,
                             g_displays[i].name, request.animation, token,
                             status.current_frame,
                             uptime_ms());
                }
            }
            update_transition_waiting(eye_sent_mask & ~ready_mask, degraded);
            if (ready_mask != eye_sent_mask) {
                vTaskDelay(pdMS_TO_TICKS(
                    CONFIG_SARCASMOS_EYE_POLL_INTERVAL_MS));
            }
        }
        if (replaced) {
            eye_retry_count = 0;
            goto restart_transition;
        }

        face_request_t newer;
        if (xQueueReceive(g_face_queue, &newer, 0) == pdTRUE) {
            request = newer;
            eye_retry_count = 0;
            ESP_LOGI(TAG, "face request replaced at barrier");
            goto restart_transition;
        }

        uint8_t incomplete_mask = EYE_BOTH_MASK & ~ready_mask;
        if (incomplete_mask != 0) {
            if (eye_retry_count == 0) {
                ++eye_retry_count;
                if (eye_sent_mask != EYE_BOTH_MASK) {
                    vTaskDelay(pdMS_TO_TICKS(
                        CONFIG_SARCASMOS_NO_EYE_TIMEOUT_MS));
                }
                ESP_LOGW(
                    TAG,
                    "eye READY timeout target=0x%02X token=%u mask=0x%02X; "
                    "retrying both eyes with a fresh token before mouth",
                    request.animation, token, incomplete_mask);
                goto restart_transition;
            }
            degraded = true;
        }

        uint8_t committed_mask = eye_commit_ready(ready_mask, token);
        if (committed_mask != ready_mask) {
            degraded = true;
        }
        update_transition_waiting(committed_mask, degraded);

        uint8_t activated_mask = 0;
        started = uptime_ms();
        while (activated_mask != committed_mask &&
               uptime_ms() - started <
                   CONFIG_SARCASMOS_EYE_BARRIER_TIMEOUT_MS) {
            for (size_t i = 0; i < EYE_COUNT; ++i) {
                const uint8_t mask = 1U << i;
                if ((committed_mask & mask) == 0 ||
                    (activated_mask & mask) != 0) {
                    continue;
                }
                eye_protocol_status_t status;
                esp_err_t err = eye_read_status(&g_displays[i], &status);
                if (err == ESP_OK &&
                    eye_protocol_transition_complete(
                        &status, request.animation, token)) {
                    activated_mask |= mask;
                    ESP_LOGI(
                        TAG,
                        "%s activated target=0x%02X token=%u frame=%u at %" PRIu32,
                        g_displays[i].name, request.animation, token,
                        status.current_frame, uptime_ms());
                }
            }
            update_transition_waiting(
                committed_mask & ~activated_mask, degraded);
            if (activated_mask != committed_mask) {
                vTaskDelay(pdMS_TO_TICKS(
                    CONFIG_SARCASMOS_EYE_POLL_INTERVAL_MS));
            }
        }
        const uint8_t activation_timeout_mask =
            EYE_BOTH_MASK & ~activated_mask;
        if (activation_timeout_mask != 0) {
            if (eye_retry_count == 0) {
                ++eye_retry_count;
                ESP_LOGW(
                    TAG,
                    "eye activation timeout target=0x%02X token=%u "
                    "mask=0x%02X; retrying both eyes with a fresh token "
                    "before mouth",
                    request.animation, token, activation_timeout_mask);
                goto restart_transition;
            }
            degraded = true;
            for (size_t i = 0; i < EYE_COUNT; ++i) {
                if ((activation_timeout_mask & (1U << i)) != 0) {
                    log_eye_timeout(i, request.animation, token);
                }
            }
        }

        ESP_LOGI(TAG,
                 "eye barrier complete target=0x%02X token=%u ready=0x%02X "
                 "committed=0x%02X activated=0x%02X degraded=%s",
                 request.animation, token, ready_mask, committed_mask,
                 activated_mask, degraded ? "true" : "false");
        esp_err_t mouth_err =
            mouth_command(CMD_SET_ANIMATION, payload, sizeof(payload));
        ESP_LOGI(TAG, "mouth command target=0x%02X token=%u at %" PRIu32 ": %s",
                 request.animation, token, uptime_ms(),
                 esp_err_to_name(mouth_err));
        commit_transition(
            request.animation, token, degraded, mouth_err == ESP_OK);
    }
}

static void state_led_task(void *arg)
{
    while (true) {
        gpio_set_level(PIN_STATUS_LED, !gpio_get_level(PIN_STATUS_LED));
        vTaskDelay(pdMS_TO_TICKS(g_state == STATE_ERROR ? 150 : 500));
    }
}

static esp_err_t status_handler(httpd_req_t *req)
{
    display_device_t eyes[EYE_COUNT];
    face_transition_status_t face;
    if (xSemaphoreTake(g_display_mutex, portMAX_DELAY) == pdTRUE) {
        memcpy(eyes, g_displays, sizeof(eyes));
        xSemaphoreGive(g_display_mutex);
    } else {
        memset(eyes, 0, sizeof(eyes));
    }
    if (xSemaphoreTake(g_face_mutex, portMAX_DELAY) == pdTRUE) {
        face = g_face_transition;
        xSemaphoreGive(g_face_mutex);
    } else {
        memset(&face, 0, sizeof(face));
    }

    const char *waiting =
        face.waiting_mask == EYE_BOTH_MASK ? "\"left_eye\",\"right_eye\"" :
        face.waiting_mask == EYE_LEFT_MASK ? "\"left_eye\"" :
        face.waiting_mask == EYE_RIGHT_MASK ? "\"right_eye\"" : "";

    char json[1536];
    int pos = snprintf(json, sizeof(json),
        "{\"state\":\"%s\",\"uptime_ms\":%llu,\"wifi_connected\":%s,"
        "\"ethernet_connected\":%s,\"brightness\":%u,"
        "\"face_transition\":{\"desired_animation\":%u,"
        "\"committed_animation\":%u,\"token\":%u,\"waiting_for\":[%s],"
        "\"started_ms\":%" PRIu32 ",\"degraded\":%s},\"displays\":[",
        state_name(g_state), esp_timer_get_time() / 1000ULL, g_wifi_connected ? "true" : "false",
        g_ethernet_connected ? "true" : "false", g_brightness,
        face.desired_animation, face.committed_animation, face.token, waiting,
        face.started_ms, face.degraded ? "true" : "false");
    for (size_t i = 0; i < EYE_COUNT; ++i) {
        pos += snprintf(json + pos, sizeof(json) - pos,
            "%s{\"name\":\"%s\",\"address\":%u,\"present\":%s,"
            "\"firmware\":\"%u.%u\",\"active_animation\":%u,"
            "\"pending_animation\":%u,\"active_token\":%u,"
            "\"pending_token\":%u,\"playback_flags\":%u,"
            "\"current_frame\":%u,\"last_error\":%u,"
            "\"last_seen_ms\":%" PRIu32 ",\"status_read_ms\":%" PRIu32 "}",
            i ? "," : "", eyes[i].name, eyes[i].address,
            eyes[i].present ? "true" : "false",
            eyes[i].fw_major, eyes[i].fw_minor,
            eyes[i].active_animation, eyes[i].pending_animation,
            eyes[i].active_transition_token,
            eyes[i].pending_transition_token, eyes[i].playback_flags,
            eyes[i].current_frame, eyes[i].last_error,
            eyes[i].last_seen_ms, eyes[i].status_read_ms);
    }
    mouth_espnow_status_t mouth;
    mouth_espnow_get_status(&mouth);
    snprintf(json + pos, sizeof(json) - pos,
             "%s{\"name\":\"mouth\",\"transport\":\"esp-now\","
             "\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\","
             "\"present\":%s,\"firmware\":\"%u.%u\",\"channel\":%u,"
             "\"animation\":%u,\"brightness\":%u,\"intensity\":%u,"
             "\"transition_token\":%u,\"transition_active\":%s,"
             "\"transition_progress\":%u,"
             "\"last_seen_ms\":%" PRIu32 ",\"retries\":%" PRIu32 ","
             "\"timeouts\":%" PRIu32 "}]}",
             sizeof(g_displays) > 0 ? "," : "",
             mouth.mac[0], mouth.mac[1], mouth.mac[2],
             mouth.mac[3], mouth.mac[4], mouth.mac[5],
             mouth.present ? "true" : "false",
             mouth.firmware_major, mouth.firmware_minor, mouth.channel,
             mouth.current_animation, mouth.brightness,
             mouth.speaking_intensity, mouth.transition_token,
             mouth.transition_active ? "true" : "false",
             mouth.transition_progress, mouth.last_ack_ms,
             mouth.retry_count, mouth.timeout_count);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, json);
}

static esp_err_t command_handler(httpd_req_t *req)
{
    char body[256] = { 0 };
    int read = httpd_req_recv(req, body, sizeof(body) - 1);
    if (read < 0) return ESP_FAIL;
    body[read] = 0;

    if (strstr(body, "brightness")) {
        char *p = strstr(body, "brightness");
        while (p && *p && (*p < '0' || *p > '9')) ++p;
        if (p && *p) {
            int value = atoi(p);
            if (value < 0) value = 0;
            if (value > 255) value = 255;
            g_brightness = (uint8_t)value;
            display_command_non_animation_all(
                CMD_SET_BRIGHTNESS, &g_brightness, 1);
        }
    } else if (strstr(body, "stop")) {
        g_state = STATE_SLEEP;
        request_face_state(ANIM_SLEEP, true);
    } else {
        g_state = parse_state(body);
        request_face_state(
            animation_for_state(g_state), g_state == STATE_ERROR);
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true,\"queued\":true}");
}

static void start_http_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = CONFIG_SARCASMOS_HTTP_PORT;
    httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(httpd_start(&server, &config));
    httpd_uri_t status_uri = { .uri = "/api/status", .method = HTTP_GET, .handler = status_handler };
    httpd_uri_t command_uri = { .uri = "/api/command", .method = HTTP_POST, .handler = command_handler };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &status_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &command_uri));
}

typedef enum {
    CLI_PAGE_HOME,
    CLI_PAGE_TEST,
    CLI_PAGE_INTERACT,
    CLI_PAGE_CONFIG,
} cli_page_t;

typedef struct {
    uint8_t id;
    const char *name;
} animation_option_t;

static const animation_option_t CLI_ANIMATIONS[] = {
    { DISPLAY_ANIM_IDLE, "idle" },
    { DISPLAY_ANIM_LISTENING, "listening" },
    { DISPLAY_ANIM_THINKING, "thinking" },
    { DISPLAY_ANIM_THINKING_AUDIO, "thinking_audio" },
    { DISPLAY_ANIM_THINKING_LONG, "thinking_long" },
    { DISPLAY_ANIM_SPEAKING, "speaking" },
    { DISPLAY_ANIM_HAPPY, "happy" },
    { DISPLAY_ANIM_ANGRY, "angry" },
    { DISPLAY_ANIM_ERROR, "error" },
    { DISPLAY_ANIM_SLEEP, "sleep" },
    { DISPLAY_ANIM_TOOL, "tool" },
    { DISPLAY_ANIM_LEFT, "left" },
    { DISPLAY_ANIM_RIGHT, "right" },
    { DISPLAY_ANIM_UP, "up" },
    { DISPLAY_ANIM_DOWN, "down" },
    { DISPLAY_ANIM_CENTER, "center" },
    { DISPLAY_ANIM_NEUTRAL, "neutral" },
    { DISPLAY_ANIM_SARCASTIC, "sarcastic" },
    { DISPLAY_ANIM_SUSPICIOUS, "suspicious" },
    { DISPLAY_ANIM_TIRED, "tired" },
    { DISPLAY_ANIM_SURPRISED, "surprised" },
    { DISPLAY_ANIM_BORED, "bored" },
    { DISPLAY_ANIM_DRAMATIC, "dramatic" },
    { DISPLAY_ANIM_WATCH, "watch" },
    { DISPLAY_ANIM_PARTY, "party" },
    { DISPLAY_ANIM_BATTERY_LOW, "battery_low" },
    { DISPLAY_ANIM_SUNNY, "sunny" },
    { DISPLAY_ANIM_RAINY, "rainy" },
    { DISPLAY_ANIM_CLOUDY, "cloudy" },
    { DISPLAY_ANIM_STORMY, "stormy" },
    { DISPLAY_ANIM_SNOWY, "snowy" },
};

static bool cli_read_serial_byte(uint8_t *input, TickType_t timeout)
{
    if (g_usb_driver_ready) {
        return usb_serial_jtag_read_bytes(input, 1, timeout) == 1;
    }
    int character = fgetc(stdin);
    if (character == EOF) {
        clearerr(stdin);
        if (timeout > 0) {
            vTaskDelay(timeout);
        }
        return false;
    }
    *input = (uint8_t)character;
    return true;
}

static bool cli_read_line(const char *prompt, char *buffer, size_t capacity)
{
    size_t length = 0;
    if (capacity == 0) {
        return false;
    }
    buffer[0] = '\0';
    printf("%s", prompt);
    fflush(stdout);
    while (true) {
        uint8_t input;
        if (!cli_read_serial_byte(&input, pdMS_TO_TICKS(20))) {
            continue;
        }
        if (g_discard_line_feed) {
            g_discard_line_feed = false;
            if (input == '\n') {
                continue;
            }
        }
        if (input == '\r' || input == '\n') {
            g_discard_line_feed = input == '\r';
            buffer[length] = '\0';
            printf("\n");
            fflush(stdout);
            return true;
        }
        if (input == '\b' || input == 0x7f) {
            if (length > 0) {
                buffer[--length] = '\0';
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        }
        if (input >= 0x20 && length + 1 < capacity) {
            buffer[length++] = (char)input;
            buffer[length] = '\0';
            putchar(input);
            fflush(stdout);
        }
    }
}

static char *cli_trim(char *value)
{
    while (isspace((unsigned char)*value)) {
        ++value;
    }
    char *end = value + strlen(value);
    while (end > value && isspace((unsigned char)end[-1])) {
        *--end = '\0';
    }
    return value;
}

static void cli_print_home(void)
{
    printf("\n================ SarcasmOS Brain ================\n");
    printf("  1  Testing       board and peripheral diagnostics\n");
    printf("  2  Interact      displays, power and AI assistant\n");
    printf("  3  Configuration persistent Wi-Fi and AI settings\n");
    printf("  s  Quick status  h/? show this page\n");
    printf("==================================================\n");
}

static void cli_print_test(void)
{
    printf("\n---------------- Testing ----------------\n");
    printf("  a  run all checks       i  scan I2C bus\n");
    printf("  e  read both eyes       m  ping mouth\n");
    printf("  n  network status       g  GPIO/power status\n");
    printf("  p  speaker tone         r  microphone level\n");
    printf("  0  home                 h/? show this page\n");
    printf("-----------------------------------------\n");
}

static void cli_print_interact(void)
{
    printf("\n--------------- Interact ----------------\n");
    printf("  face <hex>       send an animation to eyes + mouth\n");
    printf("  states           list animation codes\n");
    printf("  brightness <0-255>\n");
    printf("  buck 5v on|off   buck hp on|off\n");
    printf("  listen           start one assistant voice interaction\n");
    printf("  0  home          h/? show this page\n");
    printf("-----------------------------------------\n");
}

static void cli_print_config(void)
{
    printf("\n------------- Configuration -------------\n");
    printf("  show\n");
    printf("  set ssid <value>          set password <value>\n");
    printf("  set url <http[s]://...>   set token <value>\n");
    printf("  set wake <phrase>         set wake-enabled on|off\n");
    printf("  set silence-ms <500-15000>\n");
    printf("  set vad <1-65535>         reset\n");
    printf("  apply-wifi                apply Wi-Fi without rebooting\n");
    printf("  0  home                   h/? show this page\n");
    printf("-----------------------------------------\n");
}

static void cli_print_status(void)
{
    mouth_espnow_status_t mouth = { 0 };
    if (g_mouth_initialized) {
        mouth_espnow_get_status(&mouth);
    }
    printf("State: %-10s  Wi-Fi: %-12s  Ethernet: %s\n",
           state_name(g_state),
           g_wifi_connected ? "connected" :
               (g_wifi_should_connect ? "connecting" : "not configured"),
           g_ethernet_connected ? "connected" : "offline");
    printf("Power: +5V=%s  5VHP=%s  charger=%s\n",
           gpio_get_level(PIN_5V_EN) ? "ON" : "OFF",
           gpio_get_level(PIN_5VHP_EN) ? "ON" : "OFF",
           gpio_get_level(PIN_CHARGER_CE) == 0 ? "enabled" : "disabled");
    printf("Eyes: left=%s right=%s  Mouth: %s\n",
           g_displays[EYE_LEFT_INDEX].present ? "online" : "unknown/offline",
           g_displays[EYE_RIGHT_INDEX].present ? "online" : "unknown/offline",
           mouth.present ? "online" :
               (g_mouth_initialized ? "awaiting response" : "disabled"));
}

static void cli_scan_i2c(void)
{
    printf("Scanning I2C addresses");
    fflush(stdout);
    unsigned found = 0;
    if (xSemaphoreTake(g_display_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        printf("\n[FAIL] I2C bus is busy.\n");
        return;
    }
    for (uint8_t address = 1; address < 0x7f; ++address) {
        if (i2c_master_probe(g_i2c_bus, address, 30) == ESP_OK) {
            printf("\n  0x%02X%s", address,
                   address == ADDR_LEFT_EYE ? " left eye" :
                   address == ADDR_RIGHT_EYE ? " right eye" : "");
            ++found;
        }
    }
    xSemaphoreGive(g_display_mutex);
    printf("\n%s: %u device%s found.\n", found ? "[PASS]" : "[WARN]",
           found, found == 1 ? "" : "s");
}

static void cli_test_eyes(void)
{
    for (size_t i = 0; i < EYE_COUNT; ++i) {
        eye_protocol_status_t status;
        esp_err_t err = eye_read_status(&g_displays[i], &status);
        if (err == ESP_OK) {
            printf("[PASS] %-9s firmware %u.%u animation=0x%02X frame=%u\n",
                   g_displays[i].name, status.firmware_major,
                   status.firmware_minor, status.active_animation,
                   status.current_frame);
        } else {
            printf("[FAIL] %-9s %s\n", g_displays[i].name,
                   esp_err_to_name(err));
        }
    }
}

static void cli_test_mouth(void)
{
    if (!g_mouth_initialized) {
        printf("[FAIL] Mouth ESP-NOW is not initialized.\n");
        return;
    }
    esp_err_t err = mouth_command(CMD_GET_INFO, NULL, 0);
    mouth_espnow_status_t status = { 0 };
    mouth_espnow_get_status(&status);
    printf("[%s] Mouth ping: %s, present=%s, firmware=%u.%u, channel=%u\n",
           err == ESP_OK ? "PASS" : "FAIL", esp_err_to_name(err),
           status.present ? "yes" : "no", status.firmware_major,
           status.firmware_minor, status.channel);
}

static void cli_network_status(void)
{
    cli_print_status();
    if (g_wifi_connected) {
        wifi_ap_record_t access_point = { 0 };
        if (esp_wifi_sta_get_ap_info(&access_point) == ESP_OK) {
            printf("Wi-Fi SSID: %s  RSSI: %d dBm  channel: %u\n",
                   access_point.ssid, access_point.rssi,
                   access_point.primary);
        }
    }
}

static void cli_run_tests(void)
{
    printf("\nRunning regular-firmware diagnostics...\n");
    printf("[PASS] MCU uptime=%" PRIu32 " ms, free heap=%" PRIu32 " bytes\n",
           uptime_ms(), esp_get_free_heap_size());
    cli_print_status();
    cli_scan_i2c();
    cli_test_eyes();
    cli_test_mouth();
    cli_network_status();
    printf("Diagnostics complete. Power rails still require a multimeter "
           "for voltage verification.\n");
}

static void cli_list_animations(void)
{
    for (size_t i = 0;
         i < sizeof(CLI_ANIMATIONS) / sizeof(CLI_ANIMATIONS[0]); ++i) {
        printf("  0x%02X  %s\n", CLI_ANIMATIONS[i].id,
               CLI_ANIMATIONS[i].name);
    }
}

static esp_err_t cli_apply_wifi(void)
{
#if CONFIG_SARCASMOS_ENABLE_WIFI
    g_wifi_should_connect = g_config.wifi_ssid[0] != '\0';
    esp_wifi_disconnect();
    wifi_config_t wifi_config = { 0 };
    strlcpy((char *)wifi_config.sta.ssid, g_config.wifi_ssid,
            sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, g_config.wifi_password,
            sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode =
        g_config.wifi_password[0] == '\0' ? WIFI_AUTH_OPEN :
                                           WIFI_AUTH_WPA2_PSK;
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config),
                        TAG, "Wi-Fi config apply failed");
    if (g_wifi_should_connect) {
        return esp_wifi_connect();
    }
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

static void cli_show_config(void)
{
    printf("Wi-Fi SSID:       %s\n",
           g_config.wifi_ssid[0] ? g_config.wifi_ssid : "<not set>");
    printf("Wi-Fi password:   %s\n",
           g_config.wifi_password[0] ? "<set>" : "<not set>");
    printf("Workflow URL:     %s\n",
           g_config.workflow_url[0] ? g_config.workflow_url : "<not set>");
    printf("Workflow token:   %s\n",
           g_config.workflow_token[0] ? "<set>" : "<not set>");
    printf("Wake phrase:      %s\n",
           g_config.wake_phrase[0] ? g_config.wake_phrase : "<not set>");
    printf("Wake enabled:     %s\n", g_config.wake_enabled ? "yes" : "no");
    printf("Silence timeout:  %u ms\n", g_config.silence_ms);
    printf("VAD threshold:    %u\n", g_config.vad_threshold);
    printf("Values stored from this page override blank or default build values.\n");
}

static void cli_set_config(char *arguments)
{
    char *key = arguments;
    while (*arguments && !isspace((unsigned char)*arguments)) {
        ++arguments;
    }
    if (*arguments) {
        *arguments++ = '\0';
    }
    char *value = cli_trim(arguments);
    esp_err_t err = ESP_ERR_INVALID_ARG;
    if (strcmp(key, "ssid") == 0) {
        err = brain_config_set_string(
            &g_config, BRAIN_CONFIG_WIFI_SSID, value);
    } else if (strcmp(key, "password") == 0) {
        err = brain_config_set_string(
            &g_config, BRAIN_CONFIG_WIFI_PASSWORD, value);
    } else if (strcmp(key, "url") == 0) {
        err = brain_config_set_string(
            &g_config, BRAIN_CONFIG_WORKFLOW_URL, value);
    } else if (strcmp(key, "token") == 0) {
        err = brain_config_set_string(
            &g_config, BRAIN_CONFIG_WORKFLOW_TOKEN, value);
    } else if (strcmp(key, "wake") == 0) {
        err = brain_config_set_string(
            &g_config, BRAIN_CONFIG_WAKE_PHRASE, value);
    } else if (strcmp(key, "wake-enabled") == 0) {
        if (strcmp(value, "on") == 0 || strcmp(value, "true") == 0) {
            err = brain_config_set_wake_enabled(&g_config, true);
        } else if (strcmp(value, "off") == 0 ||
                   strcmp(value, "false") == 0) {
            err = brain_config_set_wake_enabled(&g_config, false);
        }
    } else if (strcmp(key, "silence-ms") == 0) {
        char *end = NULL;
        unsigned long parsed = strtoul(value, &end, 10);
        if (end != value && *end == '\0' && parsed <= UINT16_MAX) {
            err = brain_config_set_silence_ms(
                &g_config, (uint16_t)parsed);
        }
    } else if (strcmp(key, "vad") == 0) {
        char *end = NULL;
        unsigned long parsed = strtoul(value, &end, 10);
        if (end != value && *end == '\0' && parsed <= UINT16_MAX) {
            err = brain_config_set_vad_threshold(
                &g_config, (uint16_t)parsed);
        }
    } else {
        printf("Unknown configuration key '%s'.\n", key);
        return;
    }
    printf("%s: %s\n", err == ESP_OK ? "Saved" : "Not saved",
           esp_err_to_name(err));
}

static void cli_handle_test(char *command)
{
    if (strcmp(command, "a") == 0) cli_run_tests();
    else if (strcmp(command, "i") == 0) cli_scan_i2c();
    else if (strcmp(command, "e") == 0) cli_test_eyes();
    else if (strcmp(command, "m") == 0) cli_test_mouth();
    else if (strcmp(command, "n") == 0) cli_network_status();
    else if (strcmp(command, "g") == 0) cli_print_status();
    else if (strcmp(command, "p") == 0) {
        printf("Playing 440 Hz for one second...\n");
        esp_err_t err = brain_audio_play_tone(440, 1000, 12000);
        printf("[%s] Speaker stream: %s (confirm it was audible)\n",
               err == ESP_OK ? "PASS" : "FAIL", esp_err_to_name(err));
    } else if (strcmp(command, "r") == 0) {
        printf("Measuring microphone for one second; speak or clap now...\n");
        brain_audio_capture_result_t result;
        esp_err_t err = brain_audio_measure(1000, &result);
        printf("[%s] Microphone: %s, samples=%" PRIu32
               ", peak=%u, RMS=%u\n",
               err == ESP_OK && result.peak > 0 ? "PASS" : "FAIL",
               esp_err_to_name(err), result.sample_count,
               result.peak, result.rms);
    }
    else printf("Unknown test command. Enter h for this page.\n");
}

static void cli_handle_interact(char *command)
{
    if (strcmp(command, "states") == 0) {
        cli_list_animations();
    } else if (strncmp(command, "face ", 5) == 0) {
        char *value = cli_trim(command + 5);
        char *end = NULL;
        unsigned long animation = strtoul(value, &end, 0);
        esp_err_t err =
            end != value && *end == '\0' &&
                    animation < DISPLAY_ANIM_COUNT
                ? request_face_state((uint8_t)animation, false)
                : ESP_ERR_INVALID_ARG;
        printf("Face state: %s\n", esp_err_to_name(err));
    } else if (strncmp(command, "brightness ", 11) == 0) {
        char *value = cli_trim(command + 11);
        char *end = NULL;
        unsigned long brightness = strtoul(value, &end, 10);
        if (end != value && *end == '\0' && brightness <= UINT8_MAX) {
            g_brightness = (uint8_t)brightness;
            esp_err_t err = display_command_non_animation_all(
                CMD_SET_BRIGHTNESS, &g_brightness, 1);
            printf("Brightness: %s\n", esp_err_to_name(err));
        } else {
            printf("Brightness must be from 0 through 255.\n");
        }
    } else if (strcmp(command, "buck 5v on") == 0 ||
               strcmp(command, "buck 5v off") == 0) {
        gpio_set_level(PIN_5V_EN, strcmp(command, "buck 5v on") == 0);
        printf("+5V buck %s.\n", gpio_get_level(PIN_5V_EN) ? "ON" : "OFF");
    } else if (strcmp(command, "buck hp on") == 0 ||
               strcmp(command, "buck hp off") == 0) {
        gpio_set_level(PIN_5VHP_EN, strcmp(command, "buck hp on") == 0);
        printf("5VHP buck %s.\n",
               gpio_get_level(PIN_5VHP_EN) ? "ON" : "OFF");
    } else if (strcmp(command, "listen") == 0) {
        printf("AI voice transport is not ready yet; configure its URL/token "
               "now, then use this command after the workflow module is enabled.\n");
    } else {
        printf("Unknown interaction command. Enter h for this page.\n");
    }
}

static void cli_handle_config(char *command)
{
    if (strcmp(command, "show") == 0) {
        cli_show_config();
    } else if (strncmp(command, "set ", 4) == 0) {
        cli_set_config(cli_trim(command + 4));
    } else if (strcmp(command, "apply-wifi") == 0) {
        esp_err_t err = cli_apply_wifi();
        printf("Wi-Fi apply: %s\n", esp_err_to_name(err));
    } else if (strcmp(command, "reset") == 0) {
        esp_err_t err = brain_config_reset();
        if (err == ESP_OK) {
            err = brain_config_load(&g_config);
        }
        printf("Configuration reset: %s\n", esp_err_to_name(err));
    } else {
        printf("Unknown configuration command. Enter h for this page.\n");
    }
}

static void cli_task(void *arg)
{
    (void)arg;
    cli_page_t page = CLI_PAGE_HOME;
    char input[256];
    printf("\nUSB console ready. Enter h for the three-page menu.\n");
    cli_print_home();
    while (true) {
        const char *prompt =
            page == CLI_PAGE_TEST ? "brain/test> " :
            page == CLI_PAGE_INTERACT ? "brain/interact> " :
            page == CLI_PAGE_CONFIG ? "brain/config> " : "brain> ";
        if (!cli_read_line(prompt, input, sizeof(input))) {
            continue;
        }
        char *command = cli_trim(input);
        if (strcmp(command, "0") == 0 || strcmp(command, "home") == 0) {
            page = CLI_PAGE_HOME;
            cli_print_home();
        } else if (strcmp(command, "h") == 0 ||
                   strcmp(command, "?") == 0 || command[0] == '\0') {
            if (page == CLI_PAGE_TEST) cli_print_test();
            else if (page == CLI_PAGE_INTERACT) cli_print_interact();
            else if (page == CLI_PAGE_CONFIG) cli_print_config();
            else cli_print_home();
        } else if (page == CLI_PAGE_HOME) {
            if (strcmp(command, "1") == 0) {
                page = CLI_PAGE_TEST;
                cli_print_test();
            } else if (strcmp(command, "2") == 0) {
                page = CLI_PAGE_INTERACT;
                cli_print_interact();
            } else if (strcmp(command, "3") == 0) {
                page = CLI_PAGE_CONFIG;
                cli_print_config();
            } else if (strcmp(command, "s") == 0) {
                cli_print_status();
            } else {
                printf("Unknown command. Enter 1, 2, 3, s or h.\n");
            }
        } else if (page == CLI_PAGE_TEST) {
            cli_handle_test(command);
        } else if (page == CLI_PAGE_INTERACT) {
            cli_handle_interact(command);
        } else {
            cli_handle_config(command);
        }
    }
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(300));
    usb_serial_jtag_driver_config_t usb_config =
        USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    esp_err_t usb_err = usb_serial_jtag_driver_install(&usb_config);
    if (usb_err == ESP_OK) {
        usb_serial_jtag_vfs_use_driver();
        g_usb_driver_ready = true;
    }
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    ESP_ERROR_CHECK(brain_config_load(&g_config));
    g_face_queue = xQueueCreate(1, sizeof(face_request_t));
    g_face_mutex = xSemaphoreCreateMutex();
    g_display_mutex = xSemaphoreCreateMutex();
    ESP_ERROR_CHECK(g_face_queue != NULL && g_face_mutex != NULL &&
                            g_display_mutex != NULL
                        ? ESP_OK : ESP_ERR_NO_MEM);

    configure_gpio();
    configure_i2c();
    esp_err_t audio_err = brain_audio_init();
    if (audio_err != ESP_OK) {
        ESP_LOGE(TAG, "I2S audio init failed: %s",
                 esp_err_to_name(audio_err));
    }
    esp_err_t wifi_err = start_wifi();
    if (wifi_err != ESP_OK && wifi_err != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGE(TAG, "Wi-Fi radio start failed: %s", esp_err_to_name(wifi_err));
    }
    if (wifi_err == ESP_OK) {
        start_mouth_espnow();
    }
    start_ethernet();

    vTaskDelay(pdMS_TO_TICKS(250));
    display_command_non_animation_all(
        CMD_SET_BRIGHTNESS, &g_brightness, 1);

    ESP_ERROR_CHECK(xTaskCreate(
        face_transition_task, "face_transition", 6144, NULL, 6, NULL) == pdPASS
                        ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreate(
        display_health_task, "display_health", 4096, NULL, 5, NULL) == pdPASS
                        ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreate(
        state_led_task, "state_led", 2048, NULL, 4, NULL) == pdPASS
                        ? ESP_OK : ESP_ERR_NO_MEM);

    g_state = STATE_IDLE;
    ESP_ERROR_CHECK(request_face_state(ANIM_IDLE, false));
    start_http_server();

    ESP_LOGI(TAG, "SarcasmOS brain ready");
    if (usb_err != ESP_OK) {
        ESP_LOGW(TAG, "USB byte driver unavailable: %s",
                 esp_err_to_name(usb_err));
    }
    ESP_ERROR_CHECK(xTaskCreate(
        cli_task, "brain_cli", 6144, NULL, 3, NULL) == pdPASS
                        ? ESP_OK : ESP_ERR_NO_MEM);
}
