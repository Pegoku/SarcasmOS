// SPDX-FileCopyrightText: 2020 Jeff Epler for Adafruit Industries
// SPDX-License-Identifier: MIT

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

namespace {

constexpr int kPanelWidth = 64;
constexpr int kPanelHeight = 32;
constexpr int kPanelChain = 1;
constexpr int kCellCount = kPanelWidth * kPanelHeight;

// Adafruit MatrixPortal S3 HUB75 pin mapping. If this is a different ESP32-S3
// board, replace these values with the GPIOs wired to the HUB75 connector.
constexpr int kR1Pin = 42;
constexpr int kG1Pin = 41;
constexpr int kB1Pin = 40;
constexpr int kR2Pin = 38;
constexpr int kG2Pin = 39;
constexpr int kB2Pin = 37;
constexpr int kAPin = 45;
constexpr int kBPin = 36;
constexpr int kCPin = 48;
constexpr int kDPin = 35;
constexpr int kEPin = -1;  // A 64x32, 1/16-scan panel does not use E.
constexpr int kLatchPin = 47;
constexpr int kOePin = 14;
constexpr int kClockPin = 2;

constexpr uint8_t kBrightness = 96;
constexpr uint32_t kGenerationIntervalMs = 75;
constexpr uint16_t kGenerationsBeforeRestart = 400;

MatrixPanel_I2S_DMA *matrix = nullptr;
bool lifeBoards[2][kCellCount] = {};
uint8_t visibleBoard = 0;
uint16_t generation = 0;
uint8_t colorIndex = 0;
uint32_t lastGenerationMs = 0;

const uint8_t kLifeColors[][3] = {
    {255, 255, 255},
    {255, 0, 0},
    {0, 255, 0},
    {0, 0, 255},
    {255, 255, 0},
    {0, 255, 255},
    {255, 0, 255},
};

uint16_t rgb(uint8_t red, uint8_t green, uint8_t blue) {
    return matrix->color565(red, green, blue);
}

void present() {
    matrix->flipDMABuffer();
}

void showSolid(uint8_t red, uint8_t green, uint8_t blue, uint32_t durationMs) {
    matrix->fillScreen(rgb(red, green, blue));
    present();
    delay(durationMs);
}

void showColorBars() {
    const uint8_t colors[][3] = {
        {255, 0, 0},     {0, 255, 0},   {0, 0, 255},   {255, 255, 0},
        {0, 255, 255},   {255, 0, 255}, {255, 255, 255}, {0, 0, 0},
    };

    matrix->clearScreen();
    const int barWidth = kPanelWidth / 8;
    for (int bar = 0; bar < 8; ++bar) {
        matrix->fillRect(bar * barWidth, 0, barWidth, kPanelHeight,
                         rgb(colors[bar][0], colors[bar][1], colors[bar][2]));
    }
    present();
    delay(1500);
}

void showGeometryTest() {
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

    // Unique corner colors make rotation and channel-order faults obvious.
    matrix->fillRect(1, 1, 4, 4, rgb(255, 0, 0));
    matrix->fillRect(kPanelWidth - 5, 1, 4, 4, rgb(0, 255, 0));
    matrix->fillRect(1, kPanelHeight - 5, 4, 4, rgb(0, 0, 255));
    matrix->fillRect(kPanelWidth - 5, kPanelHeight - 5, 4, 4,
                     rgb(255, 255, 255));
    present();
    delay(2000);
}

void runPanelTest() {
    Serial.println("LED matrix test: red");
    showSolid(255, 0, 0, 700);
    Serial.println("LED matrix test: green");
    showSolid(0, 255, 0, 700);
    Serial.println("LED matrix test: blue");
    showSolid(0, 0, 255, 700);
    Serial.println("LED matrix test: white (limited brightness)");
    showSolid(255, 255, 255, 700);
    Serial.println("LED matrix test: color bars");
    showColorBars();
    Serial.println("LED matrix test: geometry and corners");
    showGeometryTest();
    matrix->clearScreen();
    present();
}

void clearLife(bool *board) {
    for (int i = 0; i < kCellCount; ++i) {
        board[i] = false;
    }
}

void seedConwayTribute(bool *board) {
    static const char *const tribute[] = {
        "  +++   ",
        "  + +   ",
        "  + +   ",
        "   +    ",
        "+ +++   ",
        " + + +  ",
        "   +  + ",
        "  + +   ",
        "  + +   ",
    };

    clearLife(board);
    constexpr int tributeWidth = 8;
    constexpr int tributeHeight = sizeof(tribute) / sizeof(tribute[0]);
    const int startX = (kPanelWidth - tributeWidth) / 2;
    const int startY = kPanelHeight - tributeHeight - 2;
    for (int y = 0; y < tributeHeight; ++y) {
        for (int x = 0; x < tributeWidth; ++x) {
            board[(startY + y) * kPanelWidth + startX + x] =
                tribute[y][x] == '+';
        }
    }
}

void seedRandom(bool *board) {
    for (int i = 0; i < kCellCount; ++i) {
        board[i] = random(10000) < 3300;
    }
}

void applyLifeRule(const bool *oldBoard, bool *newBoard) {
    for (int y = 0; y < kPanelHeight; ++y) {
        const int yMinus = (y + kPanelHeight - 1) % kPanelHeight;
        const int yPlus = (y + 1) % kPanelHeight;
        for (int x = 0; x < kPanelWidth; ++x) {
            const int xMinus = (x + kPanelWidth - 1) % kPanelWidth;
            const int xPlus = (x + 1) % kPanelWidth;
            const int neighbors =
                oldBoard[yMinus * kPanelWidth + xMinus] +
                oldBoard[yMinus * kPanelWidth + x] +
                oldBoard[yMinus * kPanelWidth + xPlus] +
                oldBoard[y * kPanelWidth + xMinus] +
                oldBoard[y * kPanelWidth + xPlus] +
                oldBoard[yPlus * kPanelWidth + xMinus] +
                oldBoard[yPlus * kPanelWidth + x] +
                oldBoard[yPlus * kPanelWidth + xPlus];
            const bool alive = oldBoard[y * kPanelWidth + x];
            newBoard[y * kPanelWidth + x] =
                neighbors == 3 || (neighbors == 2 && alive);
        }
    }
}

void drawLife(const bool *board) {
    const auto &color = kLifeColors[colorIndex];
    const uint16_t liveColor = rgb(color[0], color[1], color[2]);
    matrix->clearScreen();
    for (int y = 0; y < kPanelHeight; ++y) {
        for (int x = 0; x < kPanelWidth; ++x) {
            if (board[y * kPanelWidth + x]) {
                matrix->drawPixel(x, y, liveColor);
            }
        }
    }
    present();
}

void beginLifeDemo() {
    seedConwayTribute(lifeBoards[0]);
    clearLife(lifeBoards[1]);
    visibleBoard = 0;
    generation = 0;
    colorIndex = 0;
    drawLife(lifeBoards[visibleBoard]);
    Serial.println("Conway tribute");
    delay(3000);
    lastGenerationMs = millis();
}

void updateLifeDemo() {
    const uint32_t now = millis();
    if (now - lastGenerationMs < kGenerationIntervalMs) {
        return;
    }
    lastGenerationMs = now;

    const uint8_t nextBoard = visibleBoard ^ 1;
    applyLifeRule(lifeBoards[visibleBoard], lifeBoards[nextBoard]);
    visibleBoard = nextBoard;
    drawLife(lifeBoards[visibleBoard]);

    if (++generation >= kGenerationsBeforeRestart) {
        seedRandom(lifeBoards[visibleBoard]);
        colorIndex = (colorIndex + 1) %
                     (sizeof(kLifeColors) / sizeof(kLifeColors[0]));
        generation = 0;
        Serial.printf("New random Life board, color %u\n", colorIndex);
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(250);
    Serial.println("\n64x32 HUB75 matrix test starting");

    HUB75_I2S_CFG::i2s_pins pins = {
        kR1Pin,  kG1Pin,    kB1Pin, kR2Pin, kG2Pin, kB2Pin, kAPin,
        kBPin,   kCPin,     kDPin,  kEPin,  kLatchPin, kOePin, kClockPin,
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

    randomSeed(esp_random());
    runPanelTest();
    beginLifeDemo();
}

void loop() {
    updateLifeDemo();
    delay(1);
}
