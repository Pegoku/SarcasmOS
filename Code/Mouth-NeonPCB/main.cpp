// SPDX-License-Identifier: MIT

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

namespace {

constexpr int kPanelWidth = 64;
constexpr int kPanelHeight = 32;
constexpr int kPanelChain = 1;

// Custom driver PCB pin mapping, traced from the ESP32-S3-MINI-1-N8
// through U5/U1 (SN74AHCT245) to the HUB75 connector.
constexpr int kR1Pin = 1;
constexpr int kG1Pin = 2;
constexpr int kB1Pin = 3;
constexpr int kR2Pin = 5;
constexpr int kG2Pin = 4;
constexpr int kB2Pin = 6;
constexpr int kAPin = 8;
constexpr int kBPin = 7;
constexpr int kCPin = 10;
constexpr int kDPin = 9;
constexpr int kEPin = -1;  // A 64x32, 1/16-scan panel does not use E.
constexpr int kLatchPin = 11;  // STROBE on the schematic.
constexpr int kOePin = 13;     // OE- is active low.
constexpr int kClockPin = 12;

constexpr uint8_t kBrightness = 64;
constexpr uint32_t kSolidDurationMs = 1200;
constexpr uint32_t kPatternDurationMs = 2500;

MatrixPanel_I2S_DMA *matrix = nullptr;
uint32_t testCycle = 0;

uint16_t rgb(uint8_t red, uint8_t green, uint8_t blue) {
    return matrix->color565(red, green, blue);
}

void present() {
    matrix->flipDMABuffer();
}

void announce(const char *name) {
    Serial.printf("RGB panel test: %s\n", name);
}

void showSolid(const char *name, uint8_t red, uint8_t green, uint8_t blue,
               uint32_t durationMs = kSolidDurationMs) {
    announce(name);
    matrix->fillScreen(rgb(red, green, blue));
    present();
    delay(durationMs);
}

void showColorBars() {
    announce("eight color bars");
    constexpr uint8_t colors[][3] = {
        {255, 0, 0},     {0, 255, 0},   {0, 0, 255}, {255, 255, 0},
        {0, 255, 255},   {255, 0, 255}, {255, 255, 255}, {0, 0, 0},
    };

    matrix->clearScreen();
    constexpr int barWidth = kPanelWidth / 8;
    for (int bar = 0; bar < 8; ++bar) {
        matrix->fillRect(bar * barWidth, 0, barWidth, kPanelHeight,
                         rgb(colors[bar][0], colors[bar][1], colors[bar][2]));
    }
    present();
    delay(kPatternDurationMs);
}

void showRgbRows() {
    announce("RGB horizontal rows");
    matrix->clearScreen();
    matrix->fillRect(0, 0, kPanelWidth, 10, rgb(255, 0, 0));
    matrix->fillRect(0, 10, kPanelWidth, 11, rgb(0, 255, 0));
    matrix->fillRect(0, 21, kPanelWidth, 11, rgb(0, 0, 255));
    present();
    delay(kPatternDurationMs);
}

void showCheckerboard() {
    announce("RGB checkerboard");
    const uint16_t colors[] = {
        rgb(255, 0, 0),
        rgb(0, 255, 0),
        rgb(0, 0, 255),
        rgb(255, 255, 255),
    };

    matrix->clearScreen();
    constexpr int tileSize = 4;
    for (int y = 0; y < kPanelHeight; y += tileSize) {
        for (int x = 0; x < kPanelWidth; x += tileSize) {
            const int index = ((x / tileSize) + (y / tileSize)) % 4;
            matrix->fillRect(x, y, tileSize, tileSize, colors[index]);
        }
    }
    present();
    delay(kPatternDurationMs);
}

void showGeometryTest() {
    announce("geometry and corner orientation");
    matrix->clearScreen();

    matrix->drawRect(0, 0, kPanelWidth, kPanelHeight, rgb(255, 255, 255));
    matrix->drawLine(0, 0, kPanelWidth - 1, kPanelHeight - 1, rgb(255, 0, 0));
    matrix->drawLine(kPanelWidth - 1, 0, 0, kPanelHeight - 1, rgb(0, 255, 0));

    for (int x = 8; x < kPanelWidth; x += 8) {
        matrix->drawFastVLine(x, 0, kPanelHeight, rgb(0, 0, 96));
    }
    for (int y = 8; y < kPanelHeight; y += 8) {
        matrix->drawFastHLine(0, y, kPanelWidth, rgb(0, 0, 96));
    }

    // Red: top-left, green: top-right, blue: bottom-left, white: bottom-right.
    matrix->fillRect(1, 1, 5, 5, rgb(255, 0, 0));
    matrix->fillRect(kPanelWidth - 6, 1, 5, 5, rgb(0, 255, 0));
    matrix->fillRect(1, kPanelHeight - 6, 5, 5, rgb(0, 0, 255));
    matrix->fillRect(kPanelWidth - 6, kPanelHeight - 6, 5, 5,
                     rgb(255, 255, 255));
    present();
    delay(kPatternDurationMs);
}

void runRgbPanelTest() {
    Serial.printf("\nStarting RGB panel test cycle %lu\n",
                  static_cast<unsigned long>(++testCycle));
    showSolid("RED", 255, 0, 0);
    showSolid("GREEN", 0, 255, 0);
    showSolid("BLUE", 0, 0, 255);
    showSolid("WHITE (brightness limited)", 255, 255, 255);
    showSolid("BLACK", 0, 0, 0, 500);
    showColorBars();
    showRgbRows();
    showCheckerboard();
    showGeometryTest();
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(250);

    Serial.println("\n64x32 HUB75 continuous RGB panel test");

    HUB75_I2S_CFG::i2s_pins pins = {
        kR1Pin, kG1Pin, kB1Pin, kR2Pin, kG2Pin, kB2Pin, kAPin,
        kBPin,  kCPin,  kDPin,  kEPin,  kLatchPin, kOePin, kClockPin,
    };
    HUB75_I2S_CFG config(kPanelWidth, kPanelHeight, kPanelChain, pins);
    config.double_buff = true;

    matrix = new MatrixPanel_I2S_DMA(config);
    if (matrix == nullptr || !matrix->begin()) {
        Serial.println("ERROR: matrix DMA initialization failed");
        while (true) {
            delay(1000);
        }
    }

    matrix->setBrightness8(kBrightness);
    matrix->clearScreen();
    present();
    Serial.println("Matrix DMA initialized");
}

void loop() {
    runRgbPanelTest();
}
