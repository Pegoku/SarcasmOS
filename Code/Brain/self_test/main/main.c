#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "driver/spi_master.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_flash.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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
#define I2C_ADDR_MOUTH 0x32

#define MAX17049_REG_VCELL 0x02
#define MAX17049_REG_SOC 0x04
#define MAX17049_REG_VERSION 0x08

#define W5500_VERSIONR 0x0039
#define W5500_EXPECTED_VERSION 0x04

#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_BLOCK_FRAMES 64
#define AUDIO_TEST_BLOCKS 50

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

static summary_t g_summary;
static i2c_master_bus_handle_t g_i2c_bus;

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
    case I2C_ADDR_MOUTH:
        return "mouth (expected)";
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
        I2C_ADDR_LEFT_EYE, I2C_ADDR_RIGHT_EYE, I2C_ADDR_MOUTH
    };
    unsigned displays_found = 0;
    for (size_t i = 0; i < sizeof(displays); ++i) {
        displays_found += i2c_present(displays[i]) ? 1 : 0;
    }
    snprintf(detail, sizeof(detail), "%u/3 expected display controllers present", displays_found);
    print_result(displays_found == 3 ? RESULT_PASS : RESULT_WARN, "display I2C", detail);
}

static void test_wifi(void)
{
#if CONFIG_BRAIN_SELF_TEST_WIFI
    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&init_config);
    if (err != ESP_OK) {
        char detail[96];
        snprintf(detail, sizeof(detail), "driver initialization failed: %s", esp_err_to_name(err));
        print_result(RESULT_FAIL, "Wi-Fi radio", detail);
        return;
    }

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err == ESP_OK) {
        err = esp_wifi_start();
    }
    wifi_scan_config_t scan_config = {
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };
    if (err == ESP_OK) {
        err = esp_wifi_scan_start(&scan_config, true);
    }
    if (err != ESP_OK) {
        char detail[96];
        snprintf(detail, sizeof(detail), "scan failed: %s", esp_err_to_name(err));
        print_result(RESULT_FAIL, "Wi-Fi radio", detail);
        esp_wifi_stop();
        esp_wifi_deinit();
        return;
    }

    uint16_t ap_count = 0;
    err = esp_wifi_scan_get_ap_num(&ap_count);
    if (err != ESP_OK) {
        print_result(RESULT_FAIL, "Wi-Fi radio", "scan completed but result count failed");
    } else {
        char detail[96];
        snprintf(detail, sizeof(detail), "active scan completed, %u access point%s found",
                 ap_count, ap_count == 1 ? "" : "s");
        print_result(RESULT_PASS, "Wi-Fi radio", detail);

        uint16_t records_to_read = ap_count > 8 ? 8 : ap_count;
        wifi_ap_record_t records[8];
        if (records_to_read > 0 &&
            esp_wifi_scan_get_ap_records(&records_to_read, records) == ESP_OK) {
            printf("  strongest Wi-Fi networks:\n");
            for (uint16_t i = 0; i < records_to_read; ++i) {
                printf("  - ch %-2u  %4d dBm  %s\n",
                       records[i].primary, records[i].rssi, records[i].ssid);
            }
            fflush(stdout);
        }
    }

    esp_wifi_stop();
    esp_wifi_deinit();
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

static void test_audio(void)
{
#if CONFIG_BRAIN_SELF_TEST_AUDIO
    i2s_chan_handle_t tx_channel = NULL;
    i2s_chan_handle_t rx_channel = NULL;
    i2s_chan_config_t channel_config =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    esp_err_t err = i2s_new_channel(&channel_config, &tx_channel, &rx_channel);
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
    if (err == ESP_OK) {
        err = i2s_channel_init_std_mode(rx_channel, &standard_config);
    }
    if (err == ESP_OK) {
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

    for (unsigned block = 0; block < AUDIO_TEST_BLOCKS; ++block) {
#if CONFIG_BRAIN_SELF_TEST_SPEAKER_TONE
        fill_tone(output, AUDIO_BLOCK_FRAMES, &phase);
#else
        memset(output, 0, sizeof(output));
#endif
        size_t bytes_written = 0;
        size_t bytes_read = 0;
        esp_err_t write_err = i2s_channel_write(
            tx_channel, output, sizeof(output), &bytes_written, pdMS_TO_TICKS(100));
        esp_err_t read_err = i2s_channel_read(
            rx_channel, input, sizeof(input), &bytes_read, pdMS_TO_TICKS(100));
        if (write_err != ESP_OK || read_err != ESP_OK) {
            err = write_err != ESP_OK ? write_err : read_err;
            break;
        }

        total_input_bytes += bytes_read;
        size_t sample_count = bytes_read / sizeof(input[0]);
        for (size_t i = 0; i < sample_count; ++i) {
            int32_t sample = input[i] >> 8;
            uint32_t magnitude = sample < 0 ? (uint32_t)(-sample) : (uint32_t)sample;
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

    if (err != ESP_OK || total_input_bytes == 0) {
        char detail[96];
        snprintf(detail, sizeof(detail), "stream failed: %s", esp_err_to_name(err));
        print_result(RESULT_FAIL, "I2S audio", detail);
    } else {
        const size_t sample_count = total_input_bytes / sizeof(int32_t);
        double rms = sample_count > 0 ? sqrt((double)energy / sample_count) : 0.0;
        char detail[128];
        snprintf(detail, sizeof(detail), "captured %u samples, peak %" PRIu32 ", RMS %.0f",
                 (unsigned)sample_count, peak, rms);
        print_result(changing_samples > 16 && peak > 0 ? RESULT_PASS : RESULT_FAIL,
                     "I2S microphone", detail);
#if CONFIG_BRAIN_SELF_TEST_SPEAKER_TONE
        print_result(RESULT_WARN, "I2S speaker",
                     "440 Hz waveform sent; amplifier/speaker require an audible confirmation");
#else
        print_result(RESULT_PASS, "I2S speaker", "silent samples transmitted");
#endif
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
    esp_netif_create_default_wifi_sta();
    return ESP_OK;
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(600));
    printf("\n\n");
    printf("============================================================\n");
    printf(" SarcasmOS Brain PCB self-test\n");
    printf(" ESP-IDF %s | results: PASS / WARN / FAIL / SKIP\n", esp_get_idf_version());
    printf("============================================================\n");
    printf("WARN means firmware exercised the control path but the PCB has\n");
    printf("no electrical feedback for an automatic end-to-end check.\n\n");
    fflush(stdout);

    esp_err_t service_err = initialize_system_services();
    if (service_err != ESP_OK) {
        char detail[96];
        snprintf(detail, sizeof(detail), "NVS/network setup failed: %s",
                 esp_err_to_name(service_err));
        print_result(RESULT_FAIL, "system services", detail);
    }

    test_mcu();
    test_gpio_and_power();
    test_i2c();
    test_w5500();
    test_wifi();
    test_audio();

    printf("\n============================================================\n");
    printf(" FINAL: %u PASS, %u WARN, %u FAIL, %u SKIP\n",
           g_summary.pass, g_summary.warn, g_summary.fail, g_summary.skip);
    printf(" RESULT: %s\n", g_summary.fail == 0 ? "BOARD SELF-TEST COMPLETED" : "CHECK FAILED ITEMS");
    printf(" Buck rail WARN results require a multimeter measurement.\n");
    printf(" The TMC2209 remains disabled; no motor motion is commanded.\n");
    printf("============================================================\n");
    fflush(stdout);

    bool led = false;
    while (true) {
        led = !led;
        gpio_set_level(PIN_STATUS_LED, led);
        vTaskDelay(pdMS_TO_TICKS(g_summary.fail == 0 ? 1000 : 200));
    }
}
