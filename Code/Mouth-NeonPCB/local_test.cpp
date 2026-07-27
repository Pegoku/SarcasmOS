// SPDX-License-Identifier: MIT

#include <Arduino.h>

#include "mouth_display.hpp"
#include "protocol.hpp"

namespace {

using namespace mouth_protocol;

constexpr uint32_t kAutoStepMs = 2500;
constexpr uint8_t kBrightnessStep = 16;
constexpr uint8_t kIntensityStep = 16;

const char *const kAnimationNames[] = {
    "idle",          "listening", "thinking", "thinking audio", "thinking long",
    "speaking",      "happy",     "angry",    "error",          "sleep",
};

bool autoPlay = true;
bool showingDiagnostic = false;
uint8_t autoAnimation = kAnimIdle;
uint8_t diagnostic = 0;
uint32_t lastAutoStepMs = 0;

void printHelp() {
    Serial.println("\nLocal mouth animation test");
    Serial.println("  0..9  select animation");
    Serial.println("  a     toggle automatic animation cycle");
    Serial.println("  n     next animation");
    Serial.println("  p     next RGB/panel diagnostic");
    Serial.println("  + / - increase/decrease brightness");
    Serial.println("  ] / [ increase/decrease speaking intensity");
    Serial.println("  h     show this help");
}

void selectAnimation(uint8_t animation) {
    if (animation > kAnimSleep) return;
    autoAnimation = animation;
    showingDiagnostic = false;
    mouth_display::setAnimation(animation);
    mouth_display::showNow();
    lastAutoStepMs = millis();
    Serial.printf("Animation %u: %s\n", animation, kAnimationNames[animation]);
}

void nextAnimation() {
    selectAnimation((autoAnimation + 1) % (kAnimSleep + 1));
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
        Serial.printf("Automatic animation cycle: %s\n",
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
    selectAnimation(kAnimIdle);
    Serial.println("Automatic animation cycle: on");
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
