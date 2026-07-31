// SPDX-License-Identifier: MIT

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "mouth_display.hpp"
#include "channel_button.hpp"
#include "protocol.hpp"

#ifndef ESPNOW_CHANNEL
#define ESPNOW_CHANNEL 1
#endif

namespace {

using namespace mouth_protocol;

constexpr uint8_t kFirmwareMajor = 3;
constexpr uint8_t kFirmwareMinor = 3;
constexpr uint8_t kChannelButtonPin = 0;
constexpr char kPreferencesNamespace[] = "mouth-radio";
constexpr char kChannelPreferenceKey[] = "channel";

struct PendingPacket {
    uint8_t source[6];
    uint8_t data[kMaxPacketSize];
    size_t length;
};

portMUX_TYPE receiveMux = portMUX_INITIALIZER_UNLOCKED;
PendingPacket pendingPacket = {};
volatile bool packetReady = false;

uint8_t lastSequence = 0;
uint8_t lastError = kErrorNone;
uint8_t lastSender[6] = {};
bool haveLastSender = false;
bool espNowReady = false;
bool preferencesReady = false;
uint8_t espNowChannel = ESPNOW_CHANNEL;
Preferences preferences;
mouth_channel::ButtonState channelButton;

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
    peer.channel = espNowChannel;
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
        mouth_display::animation(),
        lastSequence,
        lastError,
        mouth_display::brightness(),
        mouth_display::mouthIntensity(),
        espNowChannel,
        mouth_display::transitionToken(),
        static_cast<uint8_t>(mouth_display::transitionActive() ? 1 : 0),
        mouth_display::transitionProgress(),
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
        mouth_display::setBrightness(packet.payload[0]);
        return true;
    case kCmdSetAnimation:
    case kCmdSetExpression:
        if (!requirePayload(packet, 1)) return false;
        if (!isValidAnimation(packet.payload[0])) {
            lastError = kErrorInvalidPayload;
            return false;
        }
        mouth_display::requestAnimation(
            packet.payload[0],
            packet.payloadLength >= 2 ? packet.payload[1] : 0,
            packet.payloadLength >= 3 && packet.payload[2] != 0
                ? static_cast<uint16_t>(packet.payload[2]) *
                      mouth_display::kRendererFrameIntervalMs
                : mouth_display::kDefaultTransitionDurationMs
        );
        return true;
    case kCmdSync:
        if (!requirePayload(packet, 4)) return false;
        mouth_display::setSyncPhase(
            static_cast<uint32_t>(packet.payload[0]) |
            (static_cast<uint32_t>(packet.payload[1]) << 8) |
            (static_cast<uint32_t>(packet.payload[2]) << 16) |
            (static_cast<uint32_t>(packet.payload[3]) << 24));
        return true;
    case kCmdStop:
        mouth_display::requestAnimation(kAnimSleep);
        return true;
    case kCmdSetParam:
        if (!requirePayload(packet, 2)) return false;
        if (packet.payload[0] == kParamMouthIntensity) {
            mouth_display::setMouthIntensity(packet.payload[1]);
            return true;
        }
        if (packet.payload[0] == kParamTemperatureCelsius) {
            const int16_t signedValue = packet.payload[1] <= INT8_MAX
                                            ? packet.payload[1]
                                            : packet.payload[1] - 256;
            mouth_display::setTemperatureCelsius(
                static_cast<int8_t>(signedValue));
            return true;
        }
        lastError = kErrorInvalidPayload;
        return false;
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

uint8_t loadEspNowChannel() {
    preferencesReady = preferences.begin(kPreferencesNamespace, false);
    const uint8_t stored = preferencesReady
                               ? preferences.getUChar(
                                     kChannelPreferenceKey, ESPNOW_CHANNEL)
                               : ESPNOW_CHANNEL;
    return mouth_channel::isValidChannel(stored)
               ? stored
               : mouth_channel::kMinimumChannel;
}

void clearCurrentPeer() {
    if (haveLastSender && esp_now_is_peer_exist(lastSender)) {
        esp_now_del_peer(lastSender);
    }
    haveLastSender = false;
}

bool applyEspNowChannel(uint8_t channel, bool persist) {
    if (!mouth_channel::isValidChannel(channel)) return false;
    clearCurrentPeer();
    const esp_err_t result =
        esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    if (result != ESP_OK) {
        Serial.printf(
            "ESP-NOW channel %u failed: %s\n",
            channel, esp_err_to_name(result)
        );
        return false;
    }
    espNowChannel = channel;
    if (persist && preferencesReady) {
        preferences.putUChar(kChannelPreferenceKey, channel);
    }
    mouth_display::showChannel(channel);
    Serial.printf("ESP-NOW channel %u\n", channel);
    return true;
}

void updateChannelButton() {
    const mouth_channel::ButtonAction action = mouth_channel::updateButton(
        channelButton, digitalRead(kChannelButtonPin) == LOW, millis()
    );
    if (action == mouth_channel::ButtonAction::NextChannel) {
        applyEspNowChannel(mouth_channel::nextChannel(espNowChannel), true);
    } else if (action == mouth_channel::ButtonAction::ResetChannel) {
        applyEspNowChannel(mouth_channel::kMinimumChannel, true);
    }
}

bool initializeEspNow() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.setSleep(false);
    // Spain permits 2.4 GHz channels 1 through 13.
    if (esp_wifi_set_country_code("ES", true) != ESP_OK ||
        esp_wifi_set_channel(espNowChannel, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
        return false;
    }
    if (esp_now_init() != ESP_OK) return false;
    espNowReady = esp_now_register_recv_cb(onDataReceived) == ESP_OK;
    return espNowReady;
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(250);
    Serial.println("\nSarcasmOS ESP-NOW mouth starting");
    pinMode(kChannelButtonPin, INPUT_PULLUP);
    espNowChannel = loadEspNowChannel();
    if (!preferencesReady) {
        Serial.println("WARNING: channel preference storage unavailable");
    }

    if (!mouth_display::begin()) {
        Serial.println("ERROR: matrix DMA initialization failed");
        while (true) delay(1000);
    }
    if (!initializeEspNow()) {
        Serial.println("ERROR: ESP-NOW initialization failed");
        mouth_display::requestAnimation(kAnimError);
    } else {
        Serial.print("ESP-NOW mouth MAC ");
        Serial.print(WiFi.macAddress());
        Serial.printf(", channel %u\n", espNowChannel);
    }
    mouth_display::showNow();
}

void loop() {
    updateChannelButton();
    PendingPacket received = {};
    while (takePendingPacket(received)) {
        handlePacket(received);
    }

    mouth_display::update();
    delay(1);
}
