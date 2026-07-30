#pragma once

#include <cstdint>

namespace eye_display_orientation {

// Both panels use the same logical coordinate system and 180-degree rotation.
// Per-eye mirroring belongs to the generated FramePair selected from the
// emulator's flip_left/flip_right settings; applying a role-specific MADCTL
// transform here would reverse the right-eye artwork a second time.
constexpr uint8_t kMadctl = 0x88;

}  // namespace eye_display_orientation
