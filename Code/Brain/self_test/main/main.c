#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "driver/spi_master.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "display_protocol.h"
#include "eye_protocol.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_flash.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "mouth_espnow.h"
#include "nvs_flash.h"

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
#define PIN_TMC_DIAG GPIO_NUM_38
#define PIN_I2S_BCLK GPIO_NUM_39
#define PIN_I2S_LRCLK GPIO_NUM_40
#define PIN_I2S_DOUT GPIO_NUM_41
#define PIN_FUEL_ALERT GPIO_NUM_42
#define PIN_I2S_DIN GPIO_NUM_47
#define PIN_STATUS_LED GPIO_NUM_48

#define I2C_ADDR_MAX17049 0x36
#define I2C_ADDR_BQ25792 0x6B
#define I2C_ADDR_LEFT_EYE 0x30
#define I2C_ADDR_RIGHT_EYE 0x31

#define MAX17049_REG_VCELL 0x02
#define MAX17049_REG_SOC 0x04
#define MAX17049_REG_VERSION 0x08

#define W5500_VERSIONR 0x0039
#define W5500_EXPECTED_VERSION 0x04

#define EYE_PROTOCOL_VERSION EYE_PROTOCOL_VERSION_TRANSITIONS
#define FACE_TRANSITION_DURATION_TICKS 5
#define FACE_EYE_POLL_MS 20
#define FACE_EYE_TIMEOUT_MS 2500

#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_BLOCK_FRAMES 64
#define AUDIO_AUTO_BLOCKS 50
#define AUDIO_MANUAL_BLOCKS 250
#define AUDIO_IO_TIMEOUT_MS 100
#define WIFI_SCAN_RECORDS 20
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAILED_BIT BIT1

typedef enum {
    RESULT_PASS,
    RESULT_WARN,
    RESULT_FAIL,
    RESULT_SKIP,
} result_t;

typedef struct {
    unsigned pass;
    unsigned warn;
    unsigned fail;
    unsigned skip;
} summary_t;

typedef struct {
    bool left_eye;
    bool right_eye;
    bool mouth;
    esp_err_t left_eye_error;
    esp_err_t right_eye_error;
    esp_err_t mouth_error;
} face_discovery_t;

typedef struct {
    bool left_eye;
    bool right_eye;
    bool mouth;
} face_targets_t;

typedef struct {
    uint8_t id;
    const char *name;
    const char *description;
} face_state_t;

typedef enum {
    TUI_KEY_NONE,
    TUI_KEY_UP,
    TUI_KEY_DOWN,
    TUI_KEY_ENTER,
    TUI_KEY_SPACE,
    TUI_KEY_ESCAPE,
    TUI_KEY_QUIT,
    TUI_KEY_RESCAN,
} tui_key_t;

static const face_state_t FACE_STATES[] = {
    { DISPLAY_ANIM_IDLE, "idle", "awake, ready, resting face" },
    { DISPLAY_ANIM_LISTENING, "listening", "accepting voice input" },
    { DISPLAY_ANIM_THINKING, "thinking", "processing a request" },
    { DISPLAY_ANIM_THINKING_AUDIO, "thinking_audio", "processing recorded audio" },
    { DISPLAY_ANIM_THINKING_LONG, "thinking_long", "extended processing" },
    { DISPLAY_ANIM_SPEAKING, "speaking", "producing speech" },
    { DISPLAY_ANIM_HAPPY, "happy_fake", "deliberately cheerful expression" },
    { DISPLAY_ANIM_ANGRY, "angry", "angry or determined expression" },
    { DISPLAY_ANIM_ERROR, "error", "operation or hardware fault" },
    { DISPLAY_ANIM_SLEEP, "asleep", "face sleeping or display off" },
    { DISPLAY_ANIM_TOOL, "tool", "external tool in progress" },
    { DISPLAY_ANIM_LEFT, "left", "look left" },
    { DISPLAY_ANIM_RIGHT, "right", "look right" },
    { DISPLAY_ANIM_UP, "up", "look up" },
    { DISPLAY_ANIM_DOWN, "down", "look down" },
    { DISPLAY_ANIM_CENTER, "center", "return gaze to center" },
    { DISPLAY_ANIM_NEUTRAL, "neutral", "neutral persistent expression" },
    { DISPLAY_ANIM_SARCASTIC, "sarcastic", "sarcastic or smug expression" },
    { DISPLAY_ANIM_SUSPICIOUS, "suspicious", "skeptical expression" },
    { DISPLAY_ANIM_TIRED, "tired", "low-energy expression" },
    { DISPLAY_ANIM_SURPRISED, "surprised", "surprised expression" },
    { DISPLAY_ANIM_BORED, "bored", "disinterested expression" },
    { DISPLAY_ANIM_DRAMATIC, "dramatic", "theatrical expression" },
    { DISPLAY_ANIM_WATCH, "watch", "monitoring a live condition" },
    { DISPLAY_ANIM_PARTY, "party", "celebration mode" },
    { DISPLAY_ANIM_BATTERY_LOW, "battery_low", "low battery warning" },
    { DISPLAY_ANIM_SUNNY, "sunny", "sunny weather state" },
    { DISPLAY_ANIM_RAINY, "rainy", "rainy weather state" },
    { DISPLAY_ANIM_CLOUDY, "cloudy", "cloudy weather state" },
    { DISPLAY_ANIM_STORMY, "stormy", "storm warning state" },
    { DISPLAY_ANIM_SNOWY, "snowy", "snowy weather state" },
};

static summary_t g_summary;
static i2c_master_bus_handle_t g_i2c_bus;
static esp_netif_t *g_wifi_netif;
static EventGroupHandle_t g_wifi_events;
static bool g_wifi_initialized;
static bool g_wifi_connected;
static bool g_usb_driver_ready;
static bool g_mouth_initialized;
static bool g_discard_line_feed;
static uint8_t g_wifi_disconnect_reason;
static uint8_t g_display_sequence = 1;
static uint8_t g_face_transition_token = 1;
static esp_err_t g_service_status = ESP_OK;

static void print_result(result_t result, const char *test, const char *detail)
{
    const char *label;

    switch (result) {
    case RESULT_PASS:
        label = "PASS";
        ++g_summary.pass;
        break;
    case RESULT_WARN:
        label = "WARN";
        ++g_summary.warn;
        break;
    case RESULT_FAIL:
        label = "FAIL";
        ++g_summary.fail;
        break;
    default:
        label = "SKIP";
        ++g_summary.skip;
        break;
    }

    printf("[%-4s] %-22s %s\n", label, test, detail);
    fflush(stdout);
}

static const char *known_i2c_device(uint8_t address)
{
    switch (address) {
    case I2C_ADDR_LEFT_EYE:
        return "left eye (expected)";
    case I2C_ADDR_RIGHT_EYE:
        return "right eye (expected)";
    case I2C_ADDR_MAX17049:
        return "MAX17049 fuel gauge";
    case I2C_ADDR_BQ25792:
        return "BQ25792 charger";
    default:
        return "unidentified";
    }
}

static void test_mcu(void)
{
    esp_chip_info_t chip;
    uint32_t flash_size = 0;
    uint8_t mac[6] = { 0 };
    char detail[160];

    esp_chip_info(&chip);
    esp_err_t flash_err = esp_flash_get_size(NULL, &flash_size);
    esp_err_t mac_err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (flash_err != ESP_OK || mac_err != ESP_OK) {
        snprintf(detail, sizeof(detail), "chip rev %u, unable to read flash or MAC", chip.revision);
        print_result(RESULT_FAIL, "ESP32-S3", detail);
        return;
    }

    snprintf(detail, sizeof(detail),
             "%u cores, rev %u, %" PRIu32 " MiB flash, MAC %02X:%02X:%02X:%02X:%02X:%02X",
             chip.cores, chip.revision, flash_size / (1024 * 1024),
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    print_result(RESULT_PASS, "ESP32-S3", detail);
    print_result(RESULT_PASS, "USB serial", "this report is using native USB Serial/JTAG");
}

static esp_err_t configure_board_gpio(void)
{
    const uint64_t outputs =
        (1ULL << PIN_5V_EN) | (1ULL << PIN_5VHP_EN) |
        (1ULL << PIN_CHARGER_CE) | (1ULL << PIN_ETH_RST) |
        (1ULL << PIN_TMC_STEP) | (1ULL << PIN_TMC_DIR) |
        (1ULL << PIN_TMC_EN) | (1ULL << PIN_STATUS_LED);
    gpio_config_t output_config = {
        .pin_bit_mask = outputs,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&output_config);
    if (err != ESP_OK) {
        return err;
    }

    const uint64_t inputs =
        (1ULL << PIN_BQ_INT) | (1ULL << PIN_ETH_INT) |
        (1ULL << PIN_TMC_DIAG) | (1ULL << PIN_FUEL_ALERT);
    gpio_config_t input_config = {
        .pin_bit_mask = inputs,
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&input_config);
    if (err != ESP_OK) {
        return err;
    }

    gpio_set_level(PIN_5V_EN, 1);
    gpio_set_level(PIN_5VHP_EN, 1);
    gpio_set_level(PIN_CHARGER_CE, 0);
    gpio_set_level(PIN_ETH_RST, 1);
    gpio_set_level(PIN_TMC_STEP, 0);
    gpio_set_level(PIN_TMC_DIR, 0);
    gpio_set_level(PIN_TMC_EN, 1);
    gpio_set_level(PIN_STATUS_LED, 0);
    return ESP_OK;
}

static void test_gpio_and_power(void)
{
    esp_err_t err = configure_board_gpio();
    if (err != ESP_OK) {
        char detail[96];
        snprintf(detail, sizeof(detail), "GPIO configuration failed: %s", esp_err_to_name(err));
        print_result(RESULT_FAIL, "board GPIO", detail);
        return;
    }

    gpio_set_level(PIN_STATUS_LED, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
    gpio_set_level(PIN_STATUS_LED, 0);
    print_result(RESULT_PASS, "status LED", "blink command completed on GPIO48");

    if (gpio_get_level(PIN_5V_EN) == 1) {
        print_result(RESULT_WARN, "+5V buck control",
                     "GPIO1 enable is high; output voltage has no feedback (measure +5V)");
    } else {
        print_result(RESULT_FAIL, "+5V buck control", "GPIO1 did not latch high");
    }

    if (gpio_get_level(PIN_5VHP_EN) == 1) {
        print_result(RESULT_WARN, "5VHP buck control",
                     "GPIO2 enable is high; output voltage has no feedback (measure 5VHP)");
    } else {
        print_result(RESULT_FAIL, "5VHP buck control", "GPIO2 did not latch high");
    }

    char detail[128];
    snprintf(detail, sizeof(detail), "BQINT=%d, fuel ALRT=%d, W5500 INT=%d, TMC DIAG=%d",
             gpio_get_level(PIN_BQ_INT), gpio_get_level(PIN_FUEL_ALERT),
             gpio_get_level(PIN_ETH_INT), gpio_get_level(PIN_TMC_DIAG));
    print_result(RESULT_PASS, "interrupt GPIOs", detail);
    print_result(RESULT_PASS, "TMC safe state", "driver disabled; STEP and DIR held low");
}

static esp_err_t init_i2c(void)
{
    if (g_i2c_bus != NULL) {
        return ESP_OK;
    }

    i2c_master_bus_config_t config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,
    };
    return i2c_new_master_bus(&config, &g_i2c_bus);
}

static bool i2c_present(uint8_t address)
{
    return i2c_master_probe(g_i2c_bus, address, 40) == ESP_OK;
}

static esp_err_t i2c_read_registers(uint8_t address, uint8_t reg, uint8_t *data, size_t length)
{
    i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = CONFIG_BRAIN_SELF_TEST_I2C_FREQ_HZ,
    };
    i2c_master_dev_handle_t device = NULL;
    esp_err_t err = i2c_master_bus_add_device(g_i2c_bus, &config, &device);
    if (err == ESP_OK) {
        err = i2c_master_transmit_receive(device, &reg, 1, data, length, 80);
        i2c_master_bus_rm_device(device);
    }
    return err;
}

static uint8_t crc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07)
                               : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static esp_err_t display_command(uint8_t address, uint8_t expected_role,
                                 uint8_t command, const uint8_t *payload,
                                 uint8_t payload_length)
{
    if (payload_length > 64) {
        return ESP_ERR_INVALID_SIZE;
    }

    i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = CONFIG_BRAIN_SELF_TEST_I2C_FREQ_HZ,
    };
    i2c_master_dev_handle_t device = NULL;
    esp_err_t err = i2c_master_bus_add_device(g_i2c_bus, &config, &device);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t sequence = g_display_sequence++;
    uint8_t frame[69] = {
        EYE_PROTOCOL_VERSION, command, sequence, payload_length
    };
    if (payload_length > 0 && payload != NULL) {
        memcpy(&frame[4], payload, payload_length);
    }
    frame[4 + payload_length] = crc8(frame, 4 + payload_length);
    err = i2c_master_transmit(device, frame, 5 + payload_length, 80);

    uint8_t status_data[EYE_PROTOCOL_STATUS_SIZE] = { 0 };
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(80));
        err = i2c_master_receive(
            device, status_data, sizeof(status_data), 80);
    }
    i2c_master_bus_rm_device(device);

    eye_protocol_status_t status;
    if (err == ESP_OK &&
        (!eye_protocol_decode_status(
             status_data, sizeof(status_data), expected_role, &status) ||
         status.last_sequence != sequence || status.last_error != 0)) {
        err = ESP_ERR_INVALID_RESPONSE;
    }
    return err;
}

static esp_err_t read_eye_status(uint8_t address, uint8_t role,
                                 eye_protocol_status_t *status)
{
    i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = CONFIG_BRAIN_SELF_TEST_I2C_FREQ_HZ,
    };
    i2c_master_dev_handle_t device = NULL;
    esp_err_t err = i2c_master_bus_add_device(g_i2c_bus, &config, &device);
    uint8_t sequence = g_display_sequence++;
    if (g_display_sequence == 0) {
        g_display_sequence = 1;
    }
    uint8_t ping[5] = {
        EYE_PROTOCOL_VERSION, DISPLAY_CMD_PING, sequence, 0, 0
    };
    ping[4] = crc8(ping, 4);
    uint8_t data[EYE_PROTOCOL_STATUS_SIZE] = { 0 };
    if (err == ESP_OK) {
        err = i2c_master_transmit(device, ping, sizeof(ping), 80);
    }
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(2));
        err = i2c_master_receive(device, data, sizeof(data), 80);
    }
    if (device != NULL) {
        i2c_master_bus_rm_device(device);
    }
    if (err == ESP_OK &&
        (!eye_protocol_decode_status(data, sizeof(data), role, status) ||
         status->last_sequence != sequence)) {
        err = ESP_ERR_INVALID_RESPONSE;
    }
    return err;
}

static esp_err_t wait_for_eye_transition(uint8_t address, uint8_t role,
                                         uint8_t animation, uint8_t token)
{
    uint32_t waited = 0;
    while (waited < FACE_EYE_TIMEOUT_MS) {
        eye_protocol_status_t status;
        esp_err_t err = read_eye_status(address, role, &status);
        if (err == ESP_OK &&
            eye_protocol_transition_complete(&status, animation, token)) {
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(FACE_EYE_POLL_MS));
        waited += FACE_EYE_POLL_MS;
    }
    return ESP_ERR_TIMEOUT;
}

static void test_display_controller(uint8_t address, uint8_t role, const char *name)
{
    esp_err_t err = init_i2c();
    if (err == ESP_OK && !i2c_present(address)) {
        err = ESP_ERR_NOT_FOUND;
    }
    if (err == ESP_OK) {
        err = display_command(address, role, DISPLAY_CMD_PING, NULL, 0);
    }

    uint8_t brightness = 200;
    if (err == ESP_OK) {
        err = display_command(address, role, DISPLAY_CMD_SET_BRIGHTNESS,
                              &brightness, 1);
    }

    const uint8_t animations[] = {
        DISPLAY_ANIM_HAPPY, DISPLAY_ANIM_ERROR, DISPLAY_ANIM_IDLE
    };
    if (err == ESP_OK) {
        printf("\nTesting %s at 0x%02X: green/happy, red/error, then idle...\n",
               name, address);
        for (size_t i = 0; i < sizeof(animations); ++i) {
            uint8_t token = g_face_transition_token++;
            if (g_face_transition_token == 0) {
                g_face_transition_token = 1;
            }
            uint8_t payload[3] = {
                animations[i], token, FACE_TRANSITION_DURATION_TICKS
            };
            err = display_command(address, role, DISPLAY_CMD_SET_ANIMATION,
                                  payload, sizeof(payload));
            if (err == ESP_OK) {
                err = wait_for_eye_transition(
                    address, role, animations[i], token);
            }
            if (err != ESP_OK) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(700));
        }
    }

    char detail[120];
    if (err == ESP_OK) {
        snprintf(detail, sizeof(detail),
                 "protocol/status verified at 0x%02X; confirm visual sequence", address);
        print_result(RESULT_WARN, name, detail);
    } else {
        snprintf(detail, sizeof(detail), "test failed at 0x%02X: %s",
                 address, esp_err_to_name(err));
        print_result(RESULT_FAIL, name, detail);
    }
}

static void test_all_displays(void)
{
    test_display_controller(I2C_ADDR_LEFT_EYE, 0, "left eye");
    test_display_controller(I2C_ADDR_RIGHT_EYE, 1, "right eye");
}

static void test_max17049(void)
{
    if (!i2c_present(I2C_ADDR_MAX17049)) {
        print_result(RESULT_FAIL, "MAX17049 fuel gauge", "no ACK at I2C address 0x36");
        return;
    }

    uint8_t vcell_data[2];
    uint8_t soc_data[2];
    uint8_t version_data[2];
    esp_err_t vcell_err = i2c_read_registers(
        I2C_ADDR_MAX17049, MAX17049_REG_VCELL, vcell_data, sizeof(vcell_data));
    esp_err_t soc_err = i2c_read_registers(
        I2C_ADDR_MAX17049, MAX17049_REG_SOC, soc_data, sizeof(soc_data));
    esp_err_t version_err = i2c_read_registers(
        I2C_ADDR_MAX17049, MAX17049_REG_VERSION, version_data, sizeof(version_data));

    if (vcell_err != ESP_OK || soc_err != ESP_OK || version_err != ESP_OK) {
        print_result(RESULT_FAIL, "MAX17049 fuel gauge", "ACK received but register read failed");
        return;
    }

    uint16_t vcell_raw = ((uint16_t)vcell_data[0] << 8) | vcell_data[1];
    uint16_t soc_raw = ((uint16_t)soc_data[0] << 8) | soc_data[1];
    uint16_t version = ((uint16_t)version_data[0] << 8) | version_data[1];
    float voltage = vcell_raw * 0.000078125f;
    float soc = soc_raw / 256.0f;
    char detail[144];
    snprintf(detail, sizeof(detail), "version 0x%04X, cell %.3f V, state of charge %.1f%%",
             version, (double)voltage, (double)soc);
    print_result(RESULT_PASS, "MAX17049 fuel gauge", detail);
}

static void test_i2c(void)
{
    esp_err_t err = init_i2c();
    if (err != ESP_OK) {
        char detail[96];
        snprintf(detail, sizeof(detail), "bus initialization failed: %s", esp_err_to_name(err));
        print_result(RESULT_FAIL, "I2C bus", detail);
        return;
    }

    unsigned found = 0;
    printf("\nI2C scan on GPIO8/GPIO9 at %d Hz:\n", CONFIG_BRAIN_SELF_TEST_I2C_FREQ_HZ);
    for (uint8_t address = 0x08; address <= 0x77; ++address) {
        if (i2c_present(address)) {
            printf("  - 0x%02X: %s\n", address, known_i2c_device(address));
            ++found;
        }
    }
    fflush(stdout);

    char detail[96];
    snprintf(detail, sizeof(detail), "%u device%s acknowledged", found, found == 1 ? "" : "s");
    print_result(found > 0 ? RESULT_PASS : RESULT_FAIL, "I2C bus scan", detail);

    print_result(i2c_present(I2C_ADDR_BQ25792) ? RESULT_PASS : RESULT_FAIL,
                 "BQ25792 charger",
                 i2c_present(I2C_ADDR_BQ25792) ? "ACK at expected address 0x6B"
                                              : "no ACK at expected address 0x6B");
    test_max17049();

    const uint8_t displays[] = {
        I2C_ADDR_LEFT_EYE, I2C_ADDR_RIGHT_EYE
    };
    unsigned displays_found = 0;
    for (size_t i = 0; i < sizeof(displays); ++i) {
        displays_found += i2c_present(displays[i]) ? 1 : 0;
    }
    snprintf(detail, sizeof(detail), "%u/2 expected eye controllers present", displays_found);
    print_result(displays_found == 2 ? RESULT_PASS : RESULT_WARN, "display I2C", detail);
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t event_id,
                               void *event_data)
{
    if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        g_wifi_connected = true;
        xEventGroupClearBits(g_wifi_events, WIFI_FAILED_BIT);
        xEventGroupSetBits(g_wifi_events, WIFI_CONNECTED_BIT);
    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *disconnected = event_data;
        g_wifi_connected = false;
        g_wifi_disconnect_reason = disconnected != NULL ? disconnected->reason : 0;
        xEventGroupClearBits(g_wifi_events, WIFI_CONNECTED_BIT);
        xEventGroupSetBits(g_wifi_events, WIFI_FAILED_BIT);
    }
}

static esp_err_t wifi_ensure_started(void)
{
    if (g_wifi_initialized) {
        return ESP_OK;
    }

    g_wifi_events = xEventGroupCreate();
    if (g_wifi_events == NULL) {
        return ESP_ERR_NO_MEM;
    }

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&init_config);
    if (err == ESP_OK) {
        err = esp_event_handler_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    }
    if (err == ESP_OK) {
        err = esp_event_handler_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    }
    if (err == ESP_OK) {
        err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    }
    if (err == ESP_OK) {
        err = esp_wifi_set_mode(WIFI_MODE_STA);
    }
    if (err == ESP_OK) {
        err = esp_wifi_start();
    }
    if (err == ESP_OK) {
        g_wifi_initialized = true;
    }
    return err;
}

static esp_err_t mouth_espnow_ensure_started(void)
{
#if CONFIG_BRAIN_SELF_TEST_MOUTH_ESPNOW
    if (g_mouth_initialized) {
        return ESP_OK;
    }

    uint8_t mac[6];
    if (!mouth_espnow_parse_mac(CONFIG_BRAIN_SELF_TEST_MOUTH_MAC, mac)) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = wifi_ensure_started();
    if (err != ESP_OK) {
        return err;
    }
    if (!g_wifi_connected) {
        err = esp_wifi_set_channel(CONFIG_BRAIN_SELF_TEST_ESPNOW_CHANNEL,
                                   WIFI_SECOND_CHAN_NONE);
        if (err != ESP_OK) {
            return err;
        }
    }

    mouth_espnow_config_t config = {
        .peer_channel = 0,
        .ack_timeout_ms = CONFIG_BRAIN_SELF_TEST_ESPNOW_ACK_TIMEOUT_MS,
        .retries = CONFIG_BRAIN_SELF_TEST_ESPNOW_RETRIES,
    };
    memcpy(config.mac, mac, sizeof(config.mac));
    err = mouth_espnow_init(&config);
    if (err == ESP_OK) {
        g_mouth_initialized = true;
    }
    return err;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

static esp_err_t discover_eye(uint8_t address, uint8_t role)
{
    esp_err_t err = init_i2c();
    if (err == ESP_OK && !i2c_present(address)) {
        err = ESP_ERR_NOT_FOUND;
    }
    if (err == ESP_OK) {
        err = display_command(address, role, DISPLAY_CMD_PING, NULL, 0);
    }
    return err;
}

static face_discovery_t discover_face_devices(void)
{
    face_discovery_t discovery = {
        .left_eye_error = ESP_ERR_NOT_FOUND,
        .right_eye_error = ESP_ERR_NOT_FOUND,
        .mouth_error = ESP_ERR_NOT_FOUND,
    };

    discovery.left_eye_error = discover_eye(I2C_ADDR_LEFT_EYE, 0);
    discovery.left_eye = discovery.left_eye_error == ESP_OK;
    discovery.right_eye_error = discover_eye(I2C_ADDR_RIGHT_EYE, 1);
    discovery.right_eye = discovery.right_eye_error == ESP_OK;

#if CONFIG_BRAIN_SELF_TEST_MOUTH_ESPNOW
    discovery.mouth_error = mouth_espnow_ensure_started();
    if (discovery.mouth_error == ESP_OK) {
        discovery.mouth_error =
            mouth_espnow_send(DISPLAY_CMD_PING, NULL, 0, true);
    }
    discovery.mouth = discovery.mouth_error == ESP_OK;
#else
    discovery.mouth_error = ESP_ERR_NOT_SUPPORTED;
#endif

    return discovery;
}

static void test_mouth_espnow(void)
{
#if CONFIG_BRAIN_SELF_TEST_MOUTH_ESPNOW
    esp_err_t err = mouth_espnow_ensure_started();
    if (err == ESP_ERR_INVALID_ARG) {
        print_result(
            RESULT_SKIP, "mouth ESP-NOW",
            "set the mouth MAC under SarcasmOS Brain self-test in menuconfig");
        return;
    }
    if (err != ESP_OK) {
        char detail[112];
        snprintf(detail, sizeof(detail), "radio initialization failed: %s",
                 esp_err_to_name(err));
        print_result(RESULT_FAIL, "mouth ESP-NOW", detail);
        return;
    }

    err = mouth_espnow_send(DISPLAY_CMD_PING, NULL, 0, true);
    mouth_espnow_status_t status;
    mouth_espnow_get_status(&status);
    uint8_t radio_channel = 0;
    wifi_second_chan_t secondary_channel;
    if (err == ESP_OK) {
        err = esp_wifi_get_channel(&radio_channel, &secondary_channel);
    }
    if (err == ESP_OK && status.channel != radio_channel) {
        err = ESP_ERR_INVALID_RESPONSE;
    }

    uint8_t brightness = 200;
    if (err == ESP_OK) {
        err = mouth_espnow_send(
            DISPLAY_CMD_SET_BRIGHTNESS, &brightness, 1, true);
    }

    const uint8_t animations[] = {
        DISPLAY_ANIM_HAPPY, DISPLAY_ANIM_ERROR, DISPLAY_ANIM_IDLE
    };
    if (err == ESP_OK) {
        printf("\nTesting wireless mouth: green/happy, red/error, then idle...\n");
        for (size_t i = 0; i < sizeof(animations); ++i) {
            uint8_t token = g_face_transition_token++;
            if (g_face_transition_token == 0) {
                g_face_transition_token = 1;
            }
            uint8_t payload[3] = {
                animations[i], token, FACE_TRANSITION_DURATION_TICKS
            };
            err = mouth_espnow_send(
                DISPLAY_CMD_SET_ANIMATION, payload, sizeof(payload), true);
            if (err != ESP_OK) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(700));
        }
    }

    mouth_espnow_get_status(&status);
    char detail[160];
    if (err == ESP_OK) {
        snprintf(
            detail, sizeof(detail),
            "MAC %02X:%02X:%02X:%02X:%02X:%02X, firmware %u.%u, channel %u; confirm visuals",
            status.mac[0], status.mac[1], status.mac[2],
            status.mac[3], status.mac[4], status.mac[5],
            status.firmware_major, status.firmware_minor, status.channel);
        print_result(RESULT_WARN, "mouth ESP-NOW", detail);
    } else {
        snprintf(detail, sizeof(detail),
                 "no valid acknowledged status: %s, retries %" PRIu32
                 ", timeouts %" PRIu32,
                 esp_err_to_name(err), status.retry_count,
                 status.timeout_count);
        print_result(RESULT_FAIL, "mouth ESP-NOW", detail);
    }
#else
    print_result(RESULT_SKIP, "mouth ESP-NOW", "disabled in menuconfig");
#endif
}

static esp_err_t wifi_scan(wifi_ap_record_t *records, uint16_t capacity,
                           uint16_t *total_count, uint16_t *record_count)
{
    esp_err_t err = wifi_ensure_started();
    if (err != ESP_OK) {
        return err;
    }

    wifi_scan_config_t scan_config = {
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };
    err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        return err;
    }

    uint16_t total = 0;
    err = esp_wifi_scan_get_ap_num(&total);
    if (err != ESP_OK) {
        return err;
    }

    uint16_t count = total < capacity ? total : capacity;
    if (count > 0) {
        err = esp_wifi_scan_get_ap_records(&count, records);
        if (err != ESP_OK) {
            return err;
        }
    }
    *total_count = total;
    *record_count = count;
    return ESP_OK;
}

static void print_wifi_records(const wifi_ap_record_t *records, uint16_t count,
                               bool numbered)
{
    for (uint16_t i = 0; i < count; ++i) {
        if (numbered) {
            printf("  [%2u] ch %-2u  %4d dBm  %-4s  %s\n",
                   i + 1, records[i].primary, records[i].rssi,
                   records[i].authmode == WIFI_AUTH_OPEN ? "open" : "auth",
                   records[i].ssid);
        } else {
            printf("  - ch %-2u  %4d dBm  %-4s  %s\n",
                   records[i].primary, records[i].rssi,
                   records[i].authmode == WIFI_AUTH_OPEN ? "open" : "auth",
                   records[i].ssid);
        }
    }
    fflush(stdout);
}

static void test_wifi(void)
{
#if CONFIG_BRAIN_SELF_TEST_WIFI
    wifi_ap_record_t records[8];
    uint16_t total = 0;
    uint16_t count = 0;
    esp_err_t err = wifi_scan(records, 8, &total, &count);
    if (err != ESP_OK) {
        char detail[96];
        snprintf(detail, sizeof(detail), "scan failed: %s", esp_err_to_name(err));
        print_result(RESULT_FAIL, "Wi-Fi radio", detail);
        return;
    }

    char detail[96];
    snprintf(detail, sizeof(detail), "active scan completed, %u access point%s found",
             total, total == 1 ? "" : "s");
    print_result(RESULT_PASS, "Wi-Fi radio", detail);
    if (count > 0) {
        printf("  strongest Wi-Fi networks:\n");
        print_wifi_records(records, count, false);
    }
#else
    print_result(RESULT_SKIP, "Wi-Fi radio", "disabled in menuconfig");
#endif
}

static void test_w5500(void)
{
#if CONFIG_BRAIN_SELF_TEST_W5500
    gpio_set_level(PIN_ETH_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_ETH_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(150));

    spi_bus_config_t bus_config = {
        .mosi_io_num = PIN_ETH_MOSI,
        .miso_io_num = PIN_ETH_MISO,
        .sclk_io_num = PIN_ETH_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    esp_err_t err = spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_DISABLED);
    if (err != ESP_OK) {
        char detail[96];
        snprintf(detail, sizeof(detail), "SPI bus failed: %s", esp_err_to_name(err));
        print_result(RESULT_FAIL, "W5500 Ethernet", detail);
        return;
    }

    spi_device_interface_config_t device_config = {
        .mode = 0,
        .clock_speed_hz = 1000000,
        .spics_io_num = PIN_ETH_CS,
        .queue_size = 1,
    };
    spi_device_handle_t device = NULL;
    err = spi_bus_add_device(SPI2_HOST, &device_config, &device);
    uint8_t version = 0;
    if (err == ESP_OK) {
        spi_transaction_t transaction = {
            .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
            .length = 32,
            .rxlength = 32,
        };
        transaction.tx_data[0] = (uint8_t)(W5500_VERSIONR >> 8);
        transaction.tx_data[1] = (uint8_t)W5500_VERSIONR;
        transaction.tx_data[2] = 0x00;
        transaction.tx_data[3] = 0x00;
        err = spi_device_transmit(device, &transaction);
        version = transaction.rx_data[3];
    }

    if (err != ESP_OK) {
        char detail[96];
        snprintf(detail, sizeof(detail), "SPI transaction failed: %s", esp_err_to_name(err));
        print_result(RESULT_FAIL, "W5500 Ethernet", detail);
    } else if (version == W5500_EXPECTED_VERSION) {
        print_result(RESULT_PASS, "W5500 Ethernet", "VERSIONR returned 0x04");
    } else {
        char detail[96];
        snprintf(detail, sizeof(detail), "VERSIONR returned 0x%02X (expected 0x04)", version);
        print_result(RESULT_FAIL, "W5500 Ethernet", detail);
    }

    if (device != NULL) {
        spi_bus_remove_device(device);
    }
    spi_bus_free(SPI2_HOST);
#else
    print_result(RESULT_SKIP, "W5500 Ethernet", "disabled in menuconfig");
#endif
}

static void fill_tone(int32_t *samples, size_t frames, uint32_t *phase)
{
    const uint32_t phase_step = (uint32_t)(((uint64_t)440 << 32) / AUDIO_SAMPLE_RATE);

    for (size_t frame = 0; frame < frames; ++frame) {
        int32_t value = (*phase & 0x80000000U) ? 0x01000000 : -0x01000000;
        samples[frame * 2] = value;
        samples[frame * 2 + 1] = value;
        *phase += phase_step;
    }
}

static void test_audio_paths(bool play_tone, bool measure_microphone,
                             unsigned block_count)
{
#if CONFIG_BRAIN_SELF_TEST_AUDIO
    i2s_chan_handle_t tx_channel = NULL;
    i2s_chan_handle_t rx_channel = NULL;
    i2s_chan_config_t channel_config =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    esp_err_t err = i2s_new_channel(
        &channel_config, &tx_channel, measure_microphone ? &rx_channel : NULL);
    if (err != ESP_OK) {
        char detail[96];
        snprintf(detail, sizeof(detail), "channel allocation failed: %s", esp_err_to_name(err));
        print_result(RESULT_FAIL, "I2S audio", detail);
        return;
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
    err = i2s_channel_init_std_mode(tx_channel, &standard_config);
    if (err == ESP_OK && rx_channel != NULL) {
        err = i2s_channel_init_std_mode(rx_channel, &standard_config);
    }
    if (err == ESP_OK && rx_channel != NULL) {
        err = i2s_channel_enable(rx_channel);
    }
    if (err == ESP_OK) {
        err = i2s_channel_enable(tx_channel);
    }
    if (err != ESP_OK) {
        char detail[96];
        snprintf(detail, sizeof(detail), "driver setup failed: %s", esp_err_to_name(err));
        print_result(RESULT_FAIL, "I2S audio", detail);
        goto cleanup;
    }

    int32_t output[AUDIO_BLOCK_FRAMES * 2];
    int32_t input[AUDIO_BLOCK_FRAMES * 2];
    uint32_t phase = 0;
    uint64_t energy = 0;
    uint32_t peak = 0;
    unsigned changing_samples = 0;
    int32_t previous = 0;
    size_t total_input_bytes = 0;

    for (unsigned block = 0; block < block_count; ++block) {
        if (play_tone) {
            fill_tone(output, AUDIO_BLOCK_FRAMES, &phase);
        } else {
            memset(output, 0, sizeof(output));
        }
        size_t bytes_written = 0;
        size_t bytes_read = 0;
        esp_err_t write_err = i2s_channel_write(
            tx_channel, output, sizeof(output), &bytes_written, AUDIO_IO_TIMEOUT_MS);
        esp_err_t read_err = ESP_OK;
        if (measure_microphone) {
            read_err = i2s_channel_read(
                rx_channel, input, sizeof(input), &bytes_read, AUDIO_IO_TIMEOUT_MS);
        }
        if (write_err != ESP_OK || read_err != ESP_OK) {
            err = write_err != ESP_OK ? write_err : read_err;
            char detail[128];
            snprintf(detail, sizeof(detail), "%s stream failed: %s",
                     write_err != ESP_OK ? "speaker TX" : "microphone RX",
                     esp_err_to_name(err));
            print_result(RESULT_FAIL, "I2S audio", detail);
            break;
        }

        if (measure_microphone) {
            total_input_bytes += bytes_read;
            size_t sample_count = bytes_read / sizeof(input[0]);
            for (size_t i = 0; i < sample_count; ++i) {
                int32_t sample = input[i] >> 8;
                uint32_t magnitude =
                    sample < 0 ? (uint32_t)(-sample) : (uint32_t)sample;
                if (magnitude > peak) {
                    peak = magnitude;
                }
                energy += (uint64_t)magnitude * magnitude;
                if (sample != previous) {
                    ++changing_samples;
                }
                previous = sample;
            }
        }
    }

    if (err != ESP_OK) {
        /* The direction-specific failure was printed at the point of failure. */
    } else if (measure_microphone && total_input_bytes == 0) {
        char detail[96];
        snprintf(detail, sizeof(detail), "microphone RX returned no samples");
        print_result(RESULT_FAIL, "I2S audio", detail);
    } else {
        if (measure_microphone) {
            const size_t sample_count = total_input_bytes / sizeof(int32_t);
            double rms = sample_count > 0 ? sqrt((double)energy / sample_count) : 0.0;
            char detail[128];
            snprintf(detail, sizeof(detail),
                     "captured %u samples, peak %" PRIu32 ", RMS %.0f",
                     (unsigned)sample_count, peak, rms);
            print_result(changing_samples > 16 && peak > 0 ? RESULT_PASS : RESULT_FAIL,
                         "I2S microphone", detail);
        }
        if (play_tone) {
            print_result(
                RESULT_WARN, "I2S speaker",
                "440 Hz waveform sent; confirm that the tone was audible");
        }
    }

cleanup:
    if (tx_channel != NULL) {
        i2s_channel_disable(tx_channel);
        i2s_del_channel(tx_channel);
    }
    if (rx_channel != NULL) {
        i2s_channel_disable(rx_channel);
        i2s_del_channel(rx_channel);
    }
#else
    print_result(RESULT_SKIP, "I2S audio", "disabled in menuconfig");
#endif
}

static void test_audio(void)
{
#if CONFIG_BRAIN_SELF_TEST_SPEAKER_TONE
    test_audio_paths(true, true, AUDIO_AUTO_BLOCKS);
#else
    test_audio_paths(false, true, AUDIO_AUTO_BLOCKS);
#endif
}

static void manual_test_speaker(void)
{
    printf("\nPlaying a 440 Hz speaker tone for one second...\n");
    test_audio_paths(true, false, AUDIO_MANUAL_BLOCKS);
}

static void manual_test_microphone(void)
{
    printf("\nRecording the microphone for one second. Speak or clap now...\n");
    test_audio_paths(false, true, AUDIO_MANUAL_BLOCKS);
}

static esp_err_t initialize_system_services(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        err = nvs_flash_erase();
        if (err == ESP_OK) {
            err = nvs_flash_init();
        }
    }
    if (err != ESP_OK) {
        return err;
    }

    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    g_wifi_netif = esp_netif_create_default_wifi_sta();
    return g_wifi_netif != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

static void print_summary(void)
{
    printf("============================================================\n");
    printf(" FINAL: %u PASS, %u WARN, %u FAIL, %u SKIP\n",
           g_summary.pass, g_summary.warn, g_summary.fail, g_summary.skip);
    printf(" RESULT: %s\n", g_summary.fail == 0 ? "BOARD SELF-TEST COMPLETED" : "CHECK FAILED ITEMS");
    printf(" Buck rail WARN results require a multimeter measurement.\n");
    printf(" The TMC2209 remains disabled; no motor motion is commanded.\n");
    printf("============================================================\n");
    fflush(stdout);
}

static void run_self_test(void)
{
    memset(&g_summary, 0, sizeof(g_summary));
    printf("\nRunning complete board self-test...\n\n");
    if (g_service_status != ESP_OK) {
        char detail[96];
        snprintf(detail, sizeof(detail), "NVS/network setup failed: %s",
                 esp_err_to_name(g_service_status));
        print_result(RESULT_FAIL, "system services", detail);
    }
    test_mcu();
    test_gpio_and_power();
    test_i2c();
    test_w5500();
    test_wifi();
    test_mouth_espnow();
    test_audio();
    printf("\n");
    print_summary();
}

static bool read_serial_byte(uint8_t *input, TickType_t timeout)
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

static bool read_line(const char *prompt, char *buffer, size_t buffer_size)
{
    size_t length = 0;
    if (buffer_size == 0) {
        return false;
    }
    buffer[0] = '\0';
    printf("%s", prompt);
    fflush(stdout);

    while (true) {
        uint8_t input;
        if (!read_serial_byte(&input, pdMS_TO_TICKS(20))) {
            continue;
        }
        int character = input;

        if (g_discard_line_feed) {
            g_discard_line_feed = false;
            if (character == '\n') {
                continue;
            }
        }

        if (character == '\r' || character == '\n') {
            g_discard_line_feed = character == '\r';
            buffer[length] = '\0';
            printf("\n");
            fflush(stdout);
            return true;
        }

        if (character == '\b' || character == 0x7F) {
            if (length > 0) {
                --length;
                buffer[length] = '\0';
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        }

        if (character >= 0x20 && length + 1 < buffer_size) {
            buffer[length++] = (char)character;
            buffer[length] = '\0';
            putchar(character);
            fflush(stdout);
        }
    }
}

static tui_key_t read_tui_key(void)
{
    while (true) {
        uint8_t input;
        if (!read_serial_byte(&input, pdMS_TO_TICKS(20))) {
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
            return TUI_KEY_ENTER;
        }
        if (input == ' ') {
            return TUI_KEY_SPACE;
        }
        if (input == 'q' || input == 'Q') {
            return TUI_KEY_QUIT;
        }
        if (input == 'r' || input == 'R') {
            return TUI_KEY_RESCAN;
        }
        if (input == 'w' || input == 'W') {
            return TUI_KEY_UP;
        }
        if (input == 's' || input == 'S') {
            return TUI_KEY_DOWN;
        }
        if (input != 0x1B) {
            continue;
        }

        uint8_t prefix;
        if (!read_serial_byte(&prefix, pdMS_TO_TICKS(30))) {
            return TUI_KEY_ESCAPE;
        }
        if (prefix != '[' && prefix != 'O') {
            return TUI_KEY_ESCAPE;
        }
        uint8_t arrow;
        if (!read_serial_byte(&arrow, pdMS_TO_TICKS(30))) {
            return TUI_KEY_ESCAPE;
        }
        if (arrow == 'A') {
            return TUI_KEY_UP;
        }
        if (arrow == 'B') {
            return TUI_KEY_DOWN;
        }
        return TUI_KEY_NONE;
    }
}

static void manual_set_output(gpio_num_t pin, int level, const char *name,
                              const char *measurement)
{
    esp_err_t err = gpio_set_level(pin, level);
    if (err != ESP_OK) {
        printf("\n[FAIL] %s: %s\n", name, esp_err_to_name(err));
        return;
    }

    printf("\n[OK] %s is %s (GPIO%d=%d).\n", name, level ? "ON" : "OFF", pin, level);
    if (level && measurement != NULL) {
        printf("     %s\n", measurement);
    }
}

static void manual_set_charger(bool enabled)
{
    esp_err_t err = gpio_set_level(PIN_CHARGER_CE, enabled ? 0 : 1);
    if (err != ESP_OK) {
        printf("\n[FAIL] charger control: %s\n", esp_err_to_name(err));
        return;
    }
    printf("\n[OK] charger is %s (active-low CE GPIO6=%d).\n",
           enabled ? "enabled" : "disabled", enabled ? 0 : 1);
}

static void manual_gpio_status(void)
{
    printf("\nGPIO state:\n");
    printf("  +5V_EN=%d  5VHP_EN=%d  charger=%s  status LED=%d\n",
           gpio_get_level(PIN_5V_EN), gpio_get_level(PIN_5VHP_EN),
           gpio_get_level(PIN_CHARGER_CE) == 0 ? "enabled" : "disabled",
           gpio_get_level(PIN_STATUS_LED));
    printf("  BQINT=%d  fuel ALRT=%d  W5500 INT=%d  TMC DIAG=%d\n",
           gpio_get_level(PIN_BQ_INT), gpio_get_level(PIN_FUEL_ALERT),
           gpio_get_level(PIN_ETH_INT), gpio_get_level(PIN_TMC_DIAG));
    printf("  TMC driver remains disabled (TmcEN=%d).\n", gpio_get_level(PIN_TMC_EN));
}

static void manual_wifi_scan(void)
{
#if CONFIG_BRAIN_SELF_TEST_WIFI
    wifi_ap_record_t records[WIFI_SCAN_RECORDS];
    uint16_t total = 0;
    uint16_t count = 0;
    printf("\nScanning Wi-Fi...\n");
    esp_err_t err = wifi_scan(records, WIFI_SCAN_RECORDS, &total, &count);
    if (err != ESP_OK) {
        printf("[FAIL] Wi-Fi scan: %s\n", esp_err_to_name(err));
        return;
    }

    printf("Found %u access point%s", total, total == 1 ? "" : "s");
    if (total > count) {
        printf(" (showing strongest %u)", count);
    }
    printf(":\n");
    print_wifi_records(records, count, true);
#else
    printf("\nWi-Fi support is disabled in menuconfig.\n");
#endif
}

static void manual_wifi_status(void)
{
#if CONFIG_BRAIN_SELF_TEST_WIFI
    esp_err_t err = wifi_ensure_started();
    if (err != ESP_OK) {
        printf("\n[FAIL] Wi-Fi driver: %s\n", esp_err_to_name(err));
        return;
    }

    wifi_ap_record_t ap;
    if (!g_wifi_connected || esp_wifi_sta_get_ap_info(&ap) != ESP_OK) {
        printf("\nWi-Fi is disconnected");
        if (g_wifi_disconnect_reason != 0) {
            printf(" (last reason %u)", g_wifi_disconnect_reason);
        }
        printf(".\n");
        return;
    }

    esp_netif_ip_info_t ip_info;
    err = esp_netif_get_ip_info(g_wifi_netif, &ip_info);
    printf("\nWi-Fi connected to %s, channel %u, RSSI %d dBm",
           ap.ssid, ap.primary, ap.rssi);
    if (err == ESP_OK) {
        printf(", IP " IPSTR, IP2STR(&ip_info.ip));
    }
    printf(".\n");
#else
    printf("\nWi-Fi support is disabled in menuconfig.\n");
#endif
}

static void manual_wifi_connect(void)
{
#if CONFIG_BRAIN_SELF_TEST_WIFI
    wifi_ap_record_t records[WIFI_SCAN_RECORDS];
    uint16_t total = 0;
    uint16_t count = 0;
    char input[80];

    printf("\nScanning before connection...\n");
    esp_err_t err = wifi_scan(records, WIFI_SCAN_RECORDS, &total, &count);
    if (err != ESP_OK) {
        printf("[FAIL] Wi-Fi scan: %s\n", esp_err_to_name(err));
        return;
    }
    if (count == 0) {
        printf("No access points found.\n");
        return;
    }

    print_wifi_records(records, count, true);
    if (!read_line("Network number (blank cancels): ", input, sizeof(input)) ||
        input[0] == '\0') {
        printf("Connection cancelled.\n");
        return;
    }

    char *end = NULL;
    long selected = strtol(input, &end, 10);
    if (*end != '\0' || selected < 1 || selected > count) {
        printf("Invalid network number.\n");
        return;
    }

    const wifi_ap_record_t *ap = &records[selected - 1];
    char password[65] = { 0 };
    if (ap->authmode != WIFI_AUTH_OPEN) {
        printf("Note: password input may be visible in your serial terminal.\n");
        if (!read_line("Password (blank cancels): ", password, sizeof(password)) ||
            password[0] == '\0') {
            printf("Connection cancelled.\n");
            return;
        }
    }

    if (g_wifi_connected) {
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    xEventGroupClearBits(g_wifi_events, WIFI_CONNECTED_BIT | WIFI_FAILED_BIT);
    g_wifi_disconnect_reason = 0;

    wifi_config_t config = { 0 };
    strlcpy((char *)config.sta.ssid, (const char *)ap->ssid, sizeof(config.sta.ssid));
    strlcpy((char *)config.sta.password, password, sizeof(config.sta.password));
    config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    err = esp_wifi_set_config(WIFI_IF_STA, &config);
    if (err == ESP_OK) {
        err = esp_wifi_connect();
    }
    if (err != ESP_OK) {
        printf("[FAIL] Could not start connection: %s\n", esp_err_to_name(err));
        return;
    }

    printf("Connecting to %s", ap->ssid);
    fflush(stdout);
    EventBits_t bits = xEventGroupWaitBits(
        g_wifi_events, WIFI_CONNECTED_BIT | WIFI_FAILED_BIT,
        pdFALSE, pdFALSE, pdMS_TO_TICKS(CONFIG_BRAIN_SELF_TEST_WIFI_TIMEOUT_MS));
    printf("\n");
    if (bits & WIFI_CONNECTED_BIT) {
        manual_wifi_status();
    } else if (bits & WIFI_FAILED_BIT) {
        printf("[FAIL] Connection rejected (Wi-Fi reason %u).\n",
               g_wifi_disconnect_reason);
    } else {
        printf("[FAIL] Connection timed out after %d ms.\n",
               CONFIG_BRAIN_SELF_TEST_WIFI_TIMEOUT_MS);
        esp_wifi_disconnect();
    }
#else
    printf("\nWi-Fi support is disabled in menuconfig.\n");
#endif
}

static void manual_wifi_disconnect(void)
{
#if CONFIG_BRAIN_SELF_TEST_WIFI
    if (!g_wifi_initialized || !g_wifi_connected) {
        printf("\nWi-Fi is already disconnected.\n");
        return;
    }
    esp_err_t err = esp_wifi_disconnect();
    printf("\n%s\n", err == ESP_OK ? "Wi-Fi disconnected."
                                   : esp_err_to_name(err));
#else
    printf("\nWi-Fi support is disabled in menuconfig.\n");
#endif
}

static void clear_tui_screen(void)
{
    printf("\033[2J\033[H");
}

static unsigned selected_target_count(const face_targets_t *targets)
{
    return (targets->left_eye ? 1U : 0U) +
           (targets->right_eye ? 1U : 0U) +
           (targets->mouth ? 1U : 0U);
}

static const char *discovery_label(bool present, esp_err_t error)
{
    return present ? "READY" : esp_err_to_name(error);
}

static void render_target_selector(const face_discovery_t *discovery,
                                   const face_targets_t *targets,
                                   unsigned cursor, const char *message)
{
    clear_tui_screen();
    printf("Face display connection\n");
    printf("=======================\n");
    printf("I2C eyes are verified by protocol role; the mouth requires an ESP-NOW reply.\n\n");
    printf("%c [%c] Left eye   I2C 0x30  %-20s\n",
           cursor == 0 ? '>' : ' ', targets->left_eye ? 'x' : ' ',
           discovery_label(discovery->left_eye, discovery->left_eye_error));
    printf("%c [%c] Right eye  I2C 0x31  %-20s\n",
           cursor == 1 ? '>' : ' ', targets->right_eye ? 'x' : ' ',
           discovery_label(discovery->right_eye, discovery->right_eye_error));
    printf("%c [%c] Mouth      ESP-NOW   %-20s\n",
           cursor == 2 ? '>' : ' ', targets->mouth ? 'x' : ' ',
           discovery_label(discovery->mouth, discovery->mouth_error));
    printf("\nUp/Down or W/S: move   Space: toggle   Enter: continue\n");
    printf("R: rescan              Q/Esc: cancel\n");
    if (message != NULL && message[0] != '\0') {
        printf("\n%s\n", message);
    }
    fflush(stdout);
}

static bool select_face_targets(face_discovery_t *discovery,
                                face_targets_t *targets)
{
    unsigned cursor = 0;
    char message[128] = "";
    memset(targets, 0, sizeof(*targets));

    while (true) {
        render_target_selector(discovery, targets, cursor, message);
        tui_key_t key = read_tui_key();
        message[0] = '\0';
        if (key == TUI_KEY_UP) {
            cursor = cursor == 0 ? 2 : cursor - 1;
        } else if (key == TUI_KEY_DOWN) {
            cursor = (cursor + 1) % 3;
        } else if (key == TUI_KEY_SPACE) {
            bool available = cursor == 0 ? discovery->left_eye
                             : cursor == 1 ? discovery->right_eye
                                           : discovery->mouth;
            if (!available) {
                snprintf(message, sizeof(message),
                         "That target did not answer discovery; press R to scan again.");
            } else if (cursor == 0) {
                targets->left_eye = !targets->left_eye;
            } else if (cursor == 1) {
                targets->right_eye = !targets->right_eye;
            } else {
                targets->mouth = !targets->mouth;
            }
        } else if (key == TUI_KEY_RESCAN) {
            clear_tui_screen();
            printf("Scanning I2C eyes and pinging the ESP-NOW mouth...\n");
            fflush(stdout);
            *discovery = discover_face_devices();
            targets->left_eye &= discovery->left_eye;
            targets->right_eye &= discovery->right_eye;
            targets->mouth &= discovery->mouth;
            snprintf(message, sizeof(message), "Discovery refreshed.");
        } else if (key == TUI_KEY_ENTER) {
            if (selected_target_count(targets) > 0) {
                return true;
            }
            snprintf(message, sizeof(message),
                     "Select at least one READY target with Space.");
        } else if (key == TUI_KEY_QUIT || key == TUI_KEY_ESCAPE) {
            return false;
        }
    }
}

static void append_send_result(char *status, size_t capacity, size_t *used,
                               const char *target, esp_err_t result)
{
    if (*used >= capacity) {
        return;
    }
    int written = snprintf(
        status + *used, capacity - *used, "%s%s=%s",
        *used > 0 ? "  " : "", target,
        result == ESP_OK ? "OK" : esp_err_to_name(result));
    if (written > 0) {
        size_t available = capacity - *used;
        *used += (size_t)written < available ? (size_t)written : available;
    }
}

static void send_face_state(const face_targets_t *targets,
                            const face_state_t *state,
                            char *status, size_t status_capacity)
{
    size_t used = 0;
    status[0] = '\0';
    uint8_t token = g_face_transition_token++;
    if (g_face_transition_token == 0) {
        g_face_transition_token = 1;
    }
    uint8_t payload[3] = {
        state->id, token, FACE_TRANSITION_DURATION_TICKS
    };
    uint8_t requested_mask = 0;
    uint8_t sent_mask = 0;
    uint8_t ready_mask = 0;

    if (targets->left_eye) {
        requested_mask |= 1U << 0;
        esp_err_t err = display_command(
            I2C_ADDR_LEFT_EYE, 0, DISPLAY_CMD_SET_ANIMATION,
            payload, sizeof(payload));
        if (err == ESP_OK) {
            sent_mask |= 1U << 0;
        } else {
            append_send_result(status, status_capacity, &used, "left", err);
        }
    }
    if (targets->right_eye) {
        requested_mask |= 1U << 1;
        esp_err_t err = display_command(
            I2C_ADDR_RIGHT_EYE, 1, DISPLAY_CMD_SET_ANIMATION,
            payload, sizeof(payload));
        if (err == ESP_OK) {
            sent_mask |= 1U << 1;
        } else {
            append_send_result(status, status_capacity, &used, "right", err);
        }
    }

    uint32_t waited = 0;
    while (ready_mask != sent_mask && waited < FACE_EYE_TIMEOUT_MS) {
        if ((sent_mask & (1U << 0)) != 0 &&
            (ready_mask & (1U << 0)) == 0) {
            eye_protocol_status_t eye;
            if (read_eye_status(I2C_ADDR_LEFT_EYE, 0, &eye) == ESP_OK &&
                eye_protocol_transition_complete(&eye, state->id, token)) {
                ready_mask |= 1U << 0;
            }
        }
        if ((sent_mask & (1U << 1)) != 0 &&
            (ready_mask & (1U << 1)) == 0) {
            eye_protocol_status_t eye;
            if (read_eye_status(I2C_ADDR_RIGHT_EYE, 1, &eye) == ESP_OK &&
                eye_protocol_transition_complete(&eye, state->id, token)) {
                ready_mask |= 1U << 1;
            }
        }
        if (ready_mask != sent_mask) {
            vTaskDelay(pdMS_TO_TICKS(FACE_EYE_POLL_MS));
            waited += FACE_EYE_POLL_MS;
        }
    }
    if ((requested_mask & (1U << 0)) != 0 &&
        (sent_mask & (1U << 0)) != 0) {
        append_send_result(
            status, status_capacity, &used, "left",
            (ready_mask & (1U << 0)) != 0 ? ESP_OK : ESP_ERR_TIMEOUT);
    }
    if ((requested_mask & (1U << 1)) != 0 &&
        (sent_mask & (1U << 1)) != 0) {
        append_send_result(
            status, status_capacity, &used, "right",
            (ready_mask & (1U << 1)) != 0 ? ESP_OK : ESP_ERR_TIMEOUT);
    }
    if (targets->mouth) {
        esp_err_t err = mouth_espnow_send(
            DISPLAY_CMD_SET_ANIMATION, payload, sizeof(payload), true);
        append_send_result(status, status_capacity, &used, "mouth", err);
    }
}

static void print_selected_targets(const face_targets_t *targets)
{
    bool separator = false;
    if (targets->left_eye) {
        printf("left eye");
        separator = true;
    }
    if (targets->right_eye) {
        printf("%sright eye", separator ? ", " : "");
        separator = true;
    }
    if (targets->mouth) {
        printf("%smouth", separator ? ", " : "");
    }
}

static void render_state_selector(const face_targets_t *targets,
                                  size_t cursor, const char *status)
{
    const size_t state_count = sizeof(FACE_STATES) / sizeof(FACE_STATES[0]);
    const size_t page_size = 10;
    size_t page_start = (cursor / page_size) * page_size;
    size_t page_end = page_start + page_size;
    if (page_end > state_count) {
        page_end = state_count;
    }

    clear_tui_screen();
    printf("Face state sender\n");
    printf("=================\n");
    printf("Targets: ");
    print_selected_targets(targets);
    printf("\nStates %u-%u of %u\n\n", (unsigned)(page_start + 1),
           (unsigned)page_end, (unsigned)state_count);
    for (size_t i = page_start; i < page_end; ++i) {
        printf("%c 0x%02X  %-16s %s\n", i == cursor ? '>' : ' ',
               FACE_STATES[i].id, FACE_STATES[i].name,
               FACE_STATES[i].description);
    }
    printf("\nUp/Down or W/S: move   Enter/Space: send state\n");
    printf("Q/Esc: return to the main test menu\n");
    if (status != NULL && status[0] != '\0') {
        printf("\n%s\n", status);
    }
    fflush(stdout);
}

static void run_face_control(void)
{
    clear_tui_screen();
    printf("Scanning I2C eyes and pinging the ESP-NOW mouth...\n");
    fflush(stdout);
    face_discovery_t discovery = discover_face_devices();
    face_targets_t targets;
    if (!select_face_targets(&discovery, &targets)) {
        clear_tui_screen();
        printf("Face display connection cancelled.\n");
        return;
    }

    const size_t state_count = sizeof(FACE_STATES) / sizeof(FACE_STATES[0]);
    size_t cursor = 0;
    char status[192] = "";
    while (true) {
        render_state_selector(&targets, cursor, status);
        tui_key_t key = read_tui_key();
        if (key == TUI_KEY_UP) {
            cursor = cursor == 0 ? state_count - 1 : cursor - 1;
        } else if (key == TUI_KEY_DOWN) {
            cursor = (cursor + 1) % state_count;
        } else if (key == TUI_KEY_ENTER || key == TUI_KEY_SPACE) {
            send_face_state(&targets, &FACE_STATES[cursor],
                            status, sizeof(status));
            char results[128];
            strlcpy(results, status, sizeof(results));
            snprintf(status, sizeof(status), "Sent 0x%02X %.16s: %.127s",
                     FACE_STATES[cursor].id, FACE_STATES[cursor].name, results);
        } else if (key == TUI_KEY_QUIT || key == TUI_KEY_ESCAPE) {
            clear_tui_screen();
            printf("Face state sender closed.\n");
            return;
        }
    }
}

static void print_tui_menu(void)
{
    printf("\n");
    printf("================ Manual test menu ================\n");
    printf(" Power                              Interfaces\n");
    printf("  1  +5V buck ON                     w  Wi-Fi scan\n");
    printf("  2  +5V buck OFF                    c  Wi-Fi connect\n");
    printf("  3  5VHP buck ON                    s  Wi-Fi status\n");
    printf("  4  5VHP buck OFF                   d  Wi-Fi disconnect\n");
    printf("  5  charger enable                  i  I2C scan/read\n");
    printf("  6  charger disable                 e  W5500 test\n");
    printf("                                      p  speaker tone\n");
    printf("                                      m  microphone capture\n");
    printf(" Displays                            a  combined I2S audio\n");
    printf("  l  left eye (I2C)                  v  all displays\n");
    printf("  r  right eye (I2C)                 o  mouth (ESP-NOW)\n");
    printf("  f  connect/select face displays and send states\n");
    printf(" Other\n");
    printf("  g  GPIO status     x  rerun all     h  show menu\n");
    printf("==================================================\n");
}

static void run_tui(void)
{
    char command[32];
    print_tui_menu();
    while (true) {
        if (!read_line("\nbrain-test> ", command, sizeof(command))) {
            continue;
        }

        switch (command[0]) {
        case '1':
            manual_set_output(PIN_5V_EN, 1, "+5V buck",
                              "Measure +5V at J2/J4/J5/J6 pin 1.");
            break;
        case '2':
            manual_set_output(PIN_5V_EN, 0, "+5V buck", NULL);
            break;
        case '3':
            manual_set_output(PIN_5VHP_EN, 1, "5VHP buck",
                              "Measure 5VHP at J1 pin 1.");
            break;
        case '4':
            manual_set_output(PIN_5VHP_EN, 0, "5VHP buck", NULL);
            break;
        case '5':
            manual_set_charger(true);
            break;
        case '6':
            manual_set_charger(false);
            break;
        case 'w':
        case 'W':
            manual_wifi_scan();
            break;
        case 'c':
        case 'C':
            manual_wifi_connect();
            break;
        case 's':
        case 'S':
            manual_wifi_status();
            break;
        case 'd':
        case 'D':
            manual_wifi_disconnect();
            break;
        case 'i':
        case 'I':
            test_i2c();
            break;
        case 'e':
        case 'E':
            test_w5500();
            break;
        case 'a':
        case 'A':
            test_audio();
            break;
        case 'p':
        case 'P':
            manual_test_speaker();
            break;
        case 'm':
        case 'M':
            manual_test_microphone();
            break;
        case 'l':
        case 'L':
            test_display_controller(I2C_ADDR_LEFT_EYE, 0, "left eye");
            break;
        case 'r':
        case 'R':
            test_display_controller(I2C_ADDR_RIGHT_EYE, 1, "right eye");
            break;
        case 'o':
        case 'O':
            test_mouth_espnow();
            break;
        case 'v':
        case 'V':
            test_all_displays();
            test_mouth_espnow();
            break;
        case 'f':
        case 'F':
            run_face_control();
            break;
        case 'g':
        case 'G':
            manual_gpio_status();
            break;
        case 'x':
        case 'X':
            run_self_test();
            break;
        case 'h':
        case 'H':
        case '?':
            print_tui_menu();
            break;
        case '\0':
            break;
        default:
            printf("Unknown command '%s'. Enter h for the menu.\n", command);
            break;
        }
    }
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(600));

    usb_serial_jtag_driver_config_t usb_config =
        USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    esp_err_t usb_err = usb_serial_jtag_driver_install(&usb_config);
    if (usb_err == ESP_OK) {
        usb_serial_jtag_vfs_use_driver();
        g_usb_driver_ready = true;
    }

    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("\n\n");
    printf("============================================================\n");
    printf(" SarcasmOS Brain PCB self-test\n");
    printf(" ESP-IDF %s | results: PASS / WARN / FAIL / SKIP\n", esp_get_idf_version());
    printf("============================================================\n");
    printf("WARN means firmware exercised the control path but the PCB has\n");
    printf("no electrical feedback for an automatic end-to-end check.\n");
    if (usb_err != ESP_OK) {
        printf("[WARN] USB byte driver unavailable (%s); input echo may be delayed.\n",
               esp_err_to_name(usb_err));
    }

    g_service_status = initialize_system_services();

    run_self_test();
    printf("\nInitial checks complete. USB serial manual controls are ready.\n");
    run_tui();
}
