#include <cassert>
#include <cstdint>

#include "../color_transition.hpp"

int main() {
    using mouth_transition::blendRgb565;
    using mouth_transition::packRgb565;

    const uint16_t black = packRgb565(0, 0, 0);
    const uint16_t white = packRgb565(255, 255, 255);
    const uint16_t red = packRgb565(255, 0, 0);
    const uint16_t blue = packRgb565(0, 0, 255);

    assert(blendRgb565(red, blue, 0) == red);
    assert(blendRgb565(red, blue, 255) == blue);
    assert(blendRgb565(black, white, 128) == packRgb565(128, 128, 128));

    const uint16_t midpoint = blendRgb565(red, blue, 128);
    assert((midpoint & 0x07e0) == 0);
    assert((midpoint & 0xf800) != 0);
    assert((midpoint & 0x001f) != 0);
    return 0;
}
