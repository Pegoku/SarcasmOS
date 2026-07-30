#pragma once

#include <cstdint>

namespace eye_device_identity {

constexpr uint32_t kScratchMagic = 0x45594500u;
constexpr uint32_t kWatchdogScratch0Address = 0x4005800cu;
constexpr uint32_t kI2c0SlaveAddressRegister = 0x40044008u;

constexpr uint32_t marker(uint8_t role) {
    return kScratchMagic | role;
}

}  // namespace eye_device_identity
