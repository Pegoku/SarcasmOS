#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "display_protocol.h"
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

#define PROTO_VERSION 0x01
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
    uint32_t last_seen_ms;
    i2c_master_dev_handle_t i2c_handle;
} display_device_t;

static const char *TAG = "sarcasmos-brain";
static display_device_t g_displays[] = {
    { .name = "left_eye", .address = ADDR_LEFT_EYE },
    { .name = "right_eye", .address = ADDR_RIGHT_EYE },
};
static assistant_state_t g_state = STATE_BOOTING;
static uint8_t g_sequence = 1;
static uint8_t g_brightness = 160;
static bool g_wifi_connected;
static bool g_wifi_should_connect;
static bool g_ethernet_connected;
static bool g_mouth_initialized;
static EventGroupHandle_t g_wifi_events;
static i2c_master_bus_handle_t g_i2c_bus;

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

static esp_err_t i2c_send(uint8_t addr, uint8_t command, const uint8_t *payload, uint8_t len)
{
    i2c_master_dev_handle_t handle = NULL;
    for (size_t i = 0; i < sizeof(g_displays) / sizeof(g_displays[0]); ++i) {
        if (g_displays[i].address == addr) {
            handle = g_displays[i].i2c_handle;
            break;
        }
    }
    if (!handle) return ESP_ERR_NOT_FOUND;

    uint8_t frame[4 + 64 + 1];
    if (len > 64) return ESP_ERR_INVALID_SIZE;
    frame[0] = PROTO_VERSION;
    frame[1] = command;
    frame[2] = g_sequence++;
    frame[3] = len;
    if (len && payload) memcpy(&frame[4], payload, len);
    frame[4 + len] = crc8(frame, 4 + len);
    return i2c_master_transmit(handle, frame, 5 + len, I2C_TIMEOUT_MS);
}

static esp_err_t display_command_all(uint8_t command, const uint8_t *payload, uint8_t len)
{
    esp_err_t result = ESP_OK;
    for (size_t i = 0; i < sizeof(g_displays) / sizeof(g_displays[0]); ++i) {
        esp_err_t err = i2c_send(g_displays[i].address, command, payload, len);
        g_displays[i].present = (err == ESP_OK);
        if (err == ESP_OK) {
            g_displays[i].last_seen_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
        } else {
            result = err;
        }
    }
#if CONFIG_SARCASMOS_MOUTH_ESPNOW
    if (g_mouth_initialized) {
        esp_err_t err = mouth_espnow_send(command, payload, len, true);
        if (err != ESP_OK) {
            result = err;
        }
    }
#endif
    return result;
}

static void set_animation_all(uint8_t animation)
{
    uint8_t payload[2] = { animation, 0 };
    display_command_all(CMD_SET_ANIMATION, payload, sizeof(payload));
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
    uint8_t sync_payload[4] = { now & 0xff, (now >> 8) & 0xff, (now >> 16) & 0xff, (now >> 24) & 0xff };
    display_command_all(CMD_SYNC, sync_payload, sizeof(sync_payload));
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

static void configure_audio(void)
{
    i2s_chan_handle_t tx_chan;
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    if (i2s_new_channel(&chan_cfg, &tx_chan, NULL) != ESP_OK) {
        ESP_LOGW(TAG, "I2S TX channel unavailable");
        return;
    }
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = PIN_I2S_BCLK,
            .ws = PIN_I2S_LRCLK,
            .dout = PIN_I2S_DOUT,
            .din = PIN_I2S_DIN,
            .invert_flags = { 0 },
        },
    };
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2s_channel_init_std_mode(tx_chan, &std_cfg));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2s_channel_enable(tx_chan));
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
    g_wifi_should_connect = strlen(CONFIG_SARCASMOS_WIFI_SSID) > 0;
    if (g_wifi_should_connect) {
        wifi_config_t wifi_config = { 0 };
        strlcpy((char *)wifi_config.sta.ssid, CONFIG_SARCASMOS_WIFI_SSID,
                sizeof(wifi_config.sta.ssid));
        strlcpy((char *)wifi_config.sta.password, CONFIG_SARCASMOS_WIFI_PASSWORD,
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
        for (size_t i = 0; i < sizeof(g_displays) / sizeof(g_displays[0]); ++i) {
            esp_err_t err = i2c_send(g_displays[i].address, CMD_PING, NULL, 0);
            g_displays[i].present = (err == ESP_OK);
            if (err == ESP_OK) g_displays[i].last_seen_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
        }
#if CONFIG_SARCASMOS_MOUTH_ESPNOW
        if (g_mouth_initialized) {
            mouth_espnow_send(CMD_PING, NULL, 0, true);
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

static void animation_task(void *arg)
{
    assistant_state_t last_state = STATE_BOOTING;
    while (true) {
        if (last_state != g_state) {
            last_state = g_state;
            set_animation_all(animation_for_state(g_state));
        }
        gpio_set_level(PIN_STATUS_LED, !gpio_get_level(PIN_STATUS_LED));
        vTaskDelay(pdMS_TO_TICKS(g_state == STATE_ERROR ? 150 : 500));
    }
}

static esp_err_t status_handler(httpd_req_t *req)
{
    char json[1024];
    int pos = snprintf(json, sizeof(json),
        "{\"state\":\"%s\",\"uptime_ms\":%llu,\"wifi_connected\":%s,\"ethernet_connected\":%s,\"brightness\":%u,\"displays\":[",
        state_name(g_state), esp_timer_get_time() / 1000ULL, g_wifi_connected ? "true" : "false",
        g_ethernet_connected ? "true" : "false", g_brightness);
    for (size_t i = 0; i < sizeof(g_displays) / sizeof(g_displays[0]); ++i) {
        pos += snprintf(json + pos, sizeof(json) - pos,
            "%s{\"name\":\"%s\",\"address\":%u,\"present\":%s,\"last_seen_ms\":%" PRIu32 "}",
            i ? "," : "", g_displays[i].name, g_displays[i].address,
            g_displays[i].present ? "true" : "false", g_displays[i].last_seen_ms);
    }
    mouth_espnow_status_t mouth;
    mouth_espnow_get_status(&mouth);
    snprintf(json + pos, sizeof(json) - pos,
             "%s{\"name\":\"mouth\",\"transport\":\"esp-now\","
             "\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\","
             "\"present\":%s,\"firmware\":\"%u.%u\",\"channel\":%u,"
             "\"animation\":%u,\"brightness\":%u,\"intensity\":%u,"
             "\"last_seen_ms\":%" PRIu32 ",\"retries\":%" PRIu32 ","
             "\"timeouts\":%" PRIu32 "}]}",
             sizeof(g_displays) > 0 ? "," : "",
             mouth.mac[0], mouth.mac[1], mouth.mac[2],
             mouth.mac[3], mouth.mac[4], mouth.mac[5],
             mouth.present ? "true" : "false",
             mouth.firmware_major, mouth.firmware_minor, mouth.channel,
             mouth.current_animation, mouth.brightness,
             mouth.speaking_intensity, mouth.last_ack_ms,
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
            display_command_all(CMD_SET_BRIGHTNESS, &g_brightness, 1);
        }
    } else if (strstr(body, "stop")) {
        display_command_all(CMD_STOP, NULL, 0);
        g_state = STATE_SLEEP;
    } else {
        g_state = parse_state(body);
        set_animation_all(animation_for_state(g_state));
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
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

void app_main(void)
{
    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    configure_gpio();
    configure_i2c();
    configure_audio();
    esp_err_t wifi_err = start_wifi();
    if (wifi_err != ESP_OK && wifi_err != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGE(TAG, "Wi-Fi radio start failed: %s", esp_err_to_name(wifi_err));
    }
    if (wifi_err == ESP_OK) {
        start_mouth_espnow();
    }
    start_ethernet();
    start_http_server();

    vTaskDelay(pdMS_TO_TICKS(250));
    display_command_all(CMD_SET_BRIGHTNESS, &g_brightness, 1);
    g_state = STATE_IDLE;
    set_animation_all(ANIM_IDLE);

    xTaskCreate(display_health_task, "display_health", 4096, NULL, 5, NULL);
    xTaskCreate(animation_task, "animation", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "SarcasmOS brain ready");
}
