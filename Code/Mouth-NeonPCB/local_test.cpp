// SPDX-License-Identifier: MIT

#include <Arduino.h>

#include "mouth_display.hpp"
#include "protocol.hpp"

namespace {

using namespace mouth_protocol;

constexpr uint32_t kAutoStepMs = 2500;
constexpr uint8_t kBrightnessStep = 16;
constexpr uint8_t kIntensityStep = 16;
constexpr int8_t kTestTemperatureCelsius = 30;

const char *const kAnimationNames[] = {
    "idle",        "listening",  "thinking",   "thinking_audio",
    "thinking_long", "speaking", "happy_fake", "angry",
    "error",       "asleep",     "tool",       "left",
    "right",       "up",         "down",       "center",
    "neutral",     "sarcastic",  "suspicious", "tired",
    "surprised",   "bored",      "dramatic",   "watch",
    "party",       "battery_low", "sunny",     "rainy",
    "cloudy",      "stormy",     "snowy",
};
static_assert(sizeof(kAnimationNames) / sizeof(kAnimationNames[0]) ==
              kAnimCount);

bool autoPlay = true;
bool showingDiagnostic = false;
uint8_t autoAnimation = kAnimIdle;
uint8_t diagnostic = 0;
uint32_t lastAutoStepMs = 0;
uint8_t escapeSequenceState = 0;

void printHelp() {
    Serial.println("\nLocal mouth animation test");
    Serial.println("  0..9  select a legacy state");
    Serial.println("  Left  previous state and pause autoplay");
    Serial.println("  Right next state and pause autoplay");
    Serial.println("  a     toggle automatic all-state cycle");
    Serial.println("  n     advance to the next state");
    Serial.println("  p     next RGB/panel diagnostic");
    Serial.println("  + / - increase/decrease brightness");
    Serial.println("  ] / [ increase/decrease speaking intensity");
    Serial.println("  . / , increase/decrease weather temperature");
    Serial.println("  h     show this help");
}

void selectAnimation(uint8_t animation) {
    if (!isValidAnimation(animation)) return;
    autoAnimation = animation;
    showingDiagnostic = false;
    mouth_display::setAnimation(animation);
    mouth_display::showNow();
    lastAutoStepMs = millis();
    Serial.printf("Animation %u: %s\n", animation, kAnimationNames[animation]);
}

void nextAnimation() {
    selectAnimation(static_cast<uint8_t>((autoAnimation + 1) % kAnimCount));
}

void previousAnimation() {
    selectAnimation(static_cast<uint8_t>(
        autoAnimation == kAnimIdle ? kAnimCount - 1 : autoAnimation - 1));
}

void adjustBrightness(int change) {
    const int value = constrain(
        static_cast<int>(mouth_display::brightness()) + change, 0, 255);
    mouth_display::setBrightness(static_cast<uint8_t>(value));
    if (!showingDiagnostic) mouth_display::showNow();
    Serial.printf("Brightness: %d/255\n", value);
}

void adjustIntensity(int change) {
    const int value = constrain(
        static_cast<int>(mouth_display::mouthIntensity()) + change, 0, 255);
    mouth_display::setMouthIntensity(static_cast<uint8_t>(value));
    if (!showingDiagnostic) mouth_display::showNow();
    Serial.printf("Speaking intensity: %d/255\n", value);
}

void adjustTemperature(int change) {
    const int value = constrain(
        static_cast<int>(mouth_display::temperatureCelsius()) + change,
        -127, 127);
    mouth_display::setTemperatureCelsius(static_cast<int8_t>(value));
    if (!showingDiagnostic) mouth_display::showNow();
    Serial.printf("Weather temperature: %d C\n", value);
}

void nextDiagnostic() {
    autoPlay = false;
    showingDiagnostic = true;
    switch (diagnostic++ % 7) {
    case 0:
        Serial.println("Diagnostic: solid red");
        mouth_display::showSolid(255, 0, 0);
        break;
    case 1:
        Serial.println("Diagnostic: solid green");
        mouth_display::showSolid(0, 255, 0);
        break;
    case 2:
        Serial.println("Diagnostic: solid blue");
        mouth_display::showSolid(0, 0, 255);
        break;
    case 3:
        Serial.println("Diagnostic: solid white");
        mouth_display::showSolid(255, 255, 255);
        break;
    case 4:
        Serial.println("Diagnostic: eight color bars");
        mouth_display::showColorBars();
        break;
    case 5:
        Serial.println("Diagnostic: RGB rows");
        mouth_display::showRgbRows();
        break;
    default:
        Serial.println("Diagnostic: geometry and orientation");
        mouth_display::showGeometryTest();
        break;
    }
}

void handleSerial(char command) {
    // Terminal arrow keys arrive as ANSI escape sequences:
    // Left = ESC [ D, Right = ESC [ C.
    if (escapeSequenceState == 1) {
        escapeSequenceState = (command == '[' || command == 'O') ? 2 : 0;
        return;
    }
    if (escapeSequenceState == 2) {
        escapeSequenceState = 0;
        if (command == 'C' || command == 'D') {
            autoPlay = false;
            if (command == 'C') {
                nextAnimation();
            } else {
                previousAnimation();
            }
        }
        return;
    }
    if (command == '\x1b') {
        escapeSequenceState = 1;
        return;
    }

    if (command >= '0' && command <= '9') {
        autoPlay = false;
        selectAnimation(command - '0');
        return;
    }
    switch (command) {
    case 'a':
    case 'A':
        autoPlay = !autoPlay;
        showingDiagnostic = false;
        lastAutoStepMs = millis();
        mouth_display::showNow();
        Serial.printf("Automatic all-state cycle: %s\n",
                      autoPlay ? "on" : "off");
        break;
    case 'n':
    case 'N':
        autoPlay = false;
        nextAnimation();
        break;
    case 'p':
    case 'P':
        nextDiagnostic();
        break;
    case '+':
        adjustBrightness(kBrightnessStep);
        break;
    case '-':
        adjustBrightness(-kBrightnessStep);
        break;
    case ']':
        adjustIntensity(kIntensityStep);
        break;
    case '[':
        adjustIntensity(-kIntensityStep);
        break;
    case '.':
        adjustTemperature(1);
        break;
    case ',':
        adjustTemperature(-1);
        break;
    case 'h':
    case 'H':
    case '?':
        printHelp();
        break;
    default:
        break;
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(250);
    if (!mouth_display::begin()) {
        Serial.println("ERROR: matrix DMA initialization failed");
        while (true) delay(1000);
    }

    printHelp();
    mouth_display::setTemperatureCelsius(kTestTemperatureCelsius);
    selectAnimation(kAnimIdle);
    Serial.println("Automatic all-state cycle: on");
}

void loop() {
    while (Serial.available()) {
        handleSerial(static_cast<char>(Serial.read()));
    }

    if (autoPlay && millis() - lastAutoStepMs >= kAutoStepMs) {
        nextAnimation();
    }
    if (!showingDiagnostic) mouth_display::update();
    delay(1);
}
