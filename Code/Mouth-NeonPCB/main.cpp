// SPDX-License-Identifier: MIT

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "protocol.hpp"

#ifndef ESPNOW_CHANNEL
#define ESPNOW_CHANNEL 1
#endif

namespace {

using namespace mouth_protocol;

constexpr int kPanelWidth = 64;
constexpr int kPanelHeight = 32;
constexpr int kPanelChain = 1;

// Custom driver PCB pin mapping, traced through U5/U1 to HUB75.
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
constexpr int kEPin = -1;
constexpr int kLatchPin = 11;
constexpr int kOePin = 13;
constexpr int kClockPin = 12;

constexpr uint8_t kFirmwareMajor = 2;
constexpr uint8_t kFirmwareMinor = 0;
constexpr uint8_t kDefaultBrightness = 64;
constexpr uint8_t kDefaultMouthIntensity = 120;
constexpr uint32_t kFrameIntervalMs = 40;

struct PendingPacket {
    uint8_t source[6];
    uint8_t data[kMaxPacketSize];
    size_t length;
};

MatrixPanel_I2S_DMA *matrix = nullptr;
portMUX_TYPE receiveMux = portMUX_INITIALIZER_UNLOCKED;
PendingPacket pendingPacket = {};
volatile bool packetReady = false;

uint8_t currentAnimation = kAnimIdle;
uint8_t brightness = kDefaultBrightness;
uint8_t mouthIntensity = kDefaultMouthIntensity;
uint8_t lastSequence = 0;
uint8_t lastError = kErrorNone;
uint32_t syncPhaseMs = 0;
uint32_t lastFrameMs = 0;
uint8_t lastSender[6] = {};
bool haveLastSender = false;

uint16_t rgb(uint8_t red, uint8_t green, uint8_t blue) {
    return matrix->color565(red, green, blue);
}

void present() {
    matrix->flipDMABuffer();
}

void drawMouth(uint32_t tick) {
    matrix->clearScreen();
    if (currentAnimation == kAnimSleep) {
        present();
        return;
    }

    uint16_t color = rgb(255, 110, 24);
    if (currentAnimation == kAnimHappy) color = rgb(40, 255, 70);
    if (currentAnimation == kAnimAngry || currentAnimation == kAnimError) {
        color = rgb(255, 0, 0);
    }
    if (currentAnimation == kAnimListening) color = rgb(0, 120, 255);
    if (currentAnimation == kAnimThinking ||
        currentAnimation == kAnimThinkingAudio ||
        currentAnimation == kAnimThinkingLong) {
        color = rgb(150, 0, 255);
    }

    if (currentAnimation == kAnimSpeaking) {
        int open = 4 + ((tick / 3) % 10);
        if ((tick / 23) & 1) open = 15 - open;
        open = constrain((open * mouthIntensity) / 160, 2, 15);
        matrix->fillRect(8, 16 - open / 2, 48, open, color);
        matrix->fillRect(10, 14 - open / 2, 44, open + 4, color);
    } else if (currentAnimation == kAnimError) {
        for (int i = 0; i < 9; ++i) {
            const int y = (tick + i * 7) % kPanelHeight;
            matrix->fillRect(i * 8, y, 5, 3, color);
        }
    } else if (currentAnimation == kAnimHappy) {
        for (int x = 9; x < 55; ++x) {
            const int dx = x - 32;
            const int y = 13 + (dx * dx) / 85;
            matrix->fillRect(x, y, 2, 2, color);
        }
    } else {
        int y = 16;
        if (currentAnimation == kAnimThinking ||
            currentAnimation == kAnimThinkingAudio ||
            currentAnimation == kAnimThinkingLong) {
            y += ((tick / 8) % 7) - 3;
        }
        matrix->fillRect(9, y - 2, 46, 4, color);
    }
    present();
}

bool sameMac(const uint8_t *left, const uint8_t *right) {
    return memcmp(left, right, 6) == 0;
}

void printMac(const uint8_t *mac) {
    Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

bool ensurePeer(const uint8_t *mac) {
    if (esp_now_is_peer_exist(mac)) return true;

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    const esp_err_t result = esp_now_add_peer(&peer);
    if (result != ESP_OK) {
        Serial.printf("ESP-NOW add peer failed: %s\n", esp_err_to_name(result));
        return false;
    }
    return true;
}

void sendStatus(const uint8_t *destination, uint8_t command) {
    if (!ensurePeer(destination)) return;

    const uint8_t payload[kStatusPayloadSize] = {
        kApplicationVersion,
        kRoleMouth,
        kFirmwareMajor,
        kFirmwareMinor,
        currentAnimation,
        lastSequence,
        lastError,
        brightness,
        mouthIntensity,
        ESPNOW_CHANNEL,
    };
    uint8_t response[kMaxPacketSize];
    const size_t length = encodePacket(
        response, sizeof(response), kTypeStatus, kRoleMouth, lastSequence,
        command, payload, sizeof(payload));
    const esp_err_t result = esp_now_send(destination, response, length);
    if (result != ESP_OK) {
        Serial.printf("ESP-NOW status queue failed: %s\n", esp_err_to_name(result));
    }
}

void onDataReceived(const uint8_t *mac, const uint8_t *data, int length) {
    if (mac == nullptr || data == nullptr || length <= 0 ||
        length > static_cast<int>(kMaxPacketSize)) {
        return;
    }

    portENTER_CRITICAL(&receiveMux);
    if (!packetReady) {
        memcpy(pendingPacket.source, mac, 6);
        memcpy(pendingPacket.data, data, length);
        pendingPacket.length = static_cast<size_t>(length);
        packetReady = true;
    }
    portEXIT_CRITICAL(&receiveMux);
}

bool takePendingPacket(PendingPacket &packet) {
    bool available = false;
    portENTER_CRITICAL(&receiveMux);
    if (packetReady) {
        packet = pendingPacket;
        packetReady = false;
        available = true;
    }
    portEXIT_CRITICAL(&receiveMux);
    return available;
}

bool requirePayload(const PacketView &packet, uint8_t minimum) {
    if (packet.payloadLength >= minimum) return true;
    lastError = kErrorInvalidPayload;
    return false;
}

bool processCommand(const PacketView &packet) {
    switch (packet.command) {
    case kCmdPing:
    case kCmdGetInfo:
        return true;
    case kCmdSetBrightness:
        if (!requirePayload(packet, 1)) return false;
        brightness = packet.payload[0];
        matrix->setBrightness8(brightness);
        return true;
    case kCmdSetAnimation:
    case kCmdSetExpression:
        if (!requirePayload(packet, 1)) return false;
        if (packet.payload[0] > kAnimSleep) {
            lastError = kErrorInvalidPayload;
            return false;
        }
        currentAnimation = packet.payload[0];
        return true;
    case kCmdSync:
        if (!requirePayload(packet, 4)) return false;
        syncPhaseMs = static_cast<uint32_t>(packet.payload[0]) |
                      (static_cast<uint32_t>(packet.payload[1]) << 8) |
                      (static_cast<uint32_t>(packet.payload[2]) << 16) |
                      (static_cast<uint32_t>(packet.payload[3]) << 24);
        return true;
    case kCmdStop:
        currentAnimation = kAnimSleep;
        return true;
    case kCmdSetParam:
        if (!requirePayload(packet, 2)) return false;
        if (packet.payload[0] != kParamMouthIntensity) {
            lastError = kErrorInvalidPayload;
            return false;
        }
        mouthIntensity = packet.payload[1];
        return true;
    case kCmdReset:
        return true;
    default:
        lastError = kErrorUnknownCommand;
        return false;
    }
}

void handlePacket(const PendingPacket &received) {
    PacketView packet = {};
    if (!decodePacket(received.data, received.length, packet)) {
        Serial.print("Ignored malformed ESP-NOW packet from ");
        printMac(received.source);
        Serial.println();
        return;
    }
    if (packet.type != kTypeCommand ||
        (packet.role != kRoleMouth && packet.role != kRoleAny)) {
        return;
    }

    const bool duplicate = haveLastSender &&
                           sameMac(lastSender, received.source) &&
                           packet.sequence == lastSequence;
    if (!duplicate) {
        memcpy(lastSender, received.source, sizeof(lastSender));
        haveLastSender = true;
        lastSequence = packet.sequence;
        lastError = kErrorNone;
        processCommand(packet);
        Serial.printf("ESP-NOW command 0x%02X sequence %u from ",
                      packet.command, packet.sequence);
        printMac(received.source);
        Serial.printf(" result=%u\n", lastError);
    }

    sendStatus(received.source, packet.command);
    if (!duplicate && packet.command == kCmdReset && lastError == kErrorNone) {
        delay(50);
        ESP.restart();
    }
}

bool initializeMatrix() {
    HUB75_I2S_CFG::i2s_pins pins = {
        kR1Pin, kG1Pin, kB1Pin, kR2Pin, kG2Pin, kB2Pin, kAPin,
        kBPin,  kCPin,  kDPin,  kEPin,  kLatchPin, kOePin, kClockPin,
    };
    HUB75_I2S_CFG config(kPanelWidth, kPanelHeight, kPanelChain, pins);
    config.double_buff = true;

    matrix = new MatrixPanel_I2S_DMA(config);
    if (matrix == nullptr || !matrix->begin()) return false;
    matrix->setBrightness8(brightness);
    matrix->clearScreen();
    present();
    return true;
}

bool initializeEspNow() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.setSleep(false);
    if (esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
        return false;
    }
    if (esp_now_init() != ESP_OK) return false;
    return esp_now_register_recv_cb(onDataReceived) == ESP_OK;
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(250);
    Serial.println("\nSarcasmOS ESP-NOW mouth starting");

    if (!initializeMatrix()) {
        Serial.println("ERROR: matrix DMA initialization failed");
        while (true) delay(1000);
    }
    if (!initializeEspNow()) {
        Serial.println("ERROR: ESP-NOW initialization failed");
        currentAnimation = kAnimError;
    } else {
        Serial.print("ESP-NOW mouth MAC ");
        Serial.print(WiFi.macAddress());
        Serial.printf(", channel %d\n", ESPNOW_CHANNEL);
    }
    drawMouth(0);
}

void loop() {
    PendingPacket received = {};
    while (takePendingPacket(received)) {
        handlePacket(received);
    }

    const uint32_t now = millis();
    if (now - lastFrameMs >= kFrameIntervalMs) {
        lastFrameMs = now;
        drawMouth((now + syncPhaseMs) / 16);
    }
    delay(1);
}
