#pragma once

#include <cstdint>

namespace mouth_transition {

inline constexpr uint16_t packRgb565(
    uint8_t red, uint8_t green, uint8_t blue
) {
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(red & 0xf8) << 8) |
        (static_cast<uint16_t>(green & 0xfc) << 3) |
        (blue >> 3)
    );
}

inline constexpr uint8_t expand5(uint8_t value) {
    return static_cast<uint8_t>((value << 3) | (value >> 2));
}

inline constexpr uint8_t expand6(uint8_t value) {
    return static_cast<uint8_t>((value << 2) | (value >> 4));
}

inline constexpr uint8_t blendChannel(
    uint8_t from, uint8_t to, uint8_t amount
) {
    return static_cast<uint8_t>(
        (static_cast<uint16_t>(from) * (255 - amount) +
         static_cast<uint16_t>(to) * amount + 127) / 255
    );
}

inline uint16_t blendRgb565(
    uint16_t from, uint16_t to, uint8_t amount
) {
    const uint8_t fromRed = expand5((from >> 11) & 0x1f);
    const uint8_t fromGreen = expand6((from >> 5) & 0x3f);
    const uint8_t fromBlue = expand5(from & 0x1f);
    const uint8_t toRed = expand5((to >> 11) & 0x1f);
    const uint8_t toGreen = expand6((to >> 5) & 0x3f);
    const uint8_t toBlue = expand5(to & 0x1f);
    return packRgb565(
        blendChannel(fromRed, toRed, amount),
        blendChannel(fromGreen, toGreen, amount),
        blendChannel(fromBlue, toBlue, amount)
    );
}

}  // namespace mouth_transition
