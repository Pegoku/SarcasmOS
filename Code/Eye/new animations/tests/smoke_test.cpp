#include <cassert>
#include <cstdint>
#include <initializer_list>

#include "../procedural_animations.hpp"

namespace {

class CountingCanvas final : public bot_animations::Canvas {
public:
    unsigned calls = 0;
    void fillScreen(uint16_t) override { ++calls; }
    void drawPixel(int16_t, int16_t, uint16_t) override { ++calls; }
    void drawLine(int16_t, int16_t, int16_t, int16_t, uint16_t) override { ++calls; }
    void drawFastHLine(int16_t, int16_t, int16_t, uint16_t) override { ++calls; }
    void fillRect(int16_t, int16_t, int16_t, int16_t, uint16_t) override { ++calls; }
    void fillRoundRect(int16_t, int16_t, int16_t, int16_t, int16_t,
                       uint16_t) override { ++calls; }
    void drawCircle(int16_t, int16_t, int16_t, uint16_t) override { ++calls; }
    void fillCircle(int16_t, int16_t, int16_t, uint16_t) override { ++calls; }
    void fillTriangle(int16_t, int16_t, int16_t, int16_t, int16_t, int16_t,
                      uint16_t) override { ++calls; }
    void drawArc(int16_t, int16_t, int16_t, int16_t, int16_t, int16_t,
                 uint16_t, uint16_t) override { ++calls; }
    void drawCenteredText(const char*, int16_t, int16_t, uint8_t, uint16_t,
                          uint16_t) override { ++calls; }
};

template <typename Enum>
void renderRange(bot_animations::Renderer& renderer, CountingCanvas& canvas,
                 bot_animations::State& state, bot_animations::ScreenMode screen,
                 Enum bot_animations::State::*member, uint8_t count) {
    state.screen = screen;
    for (uint8_t value = 0; value < count; ++value) {
        state.*member = static_cast<Enum>(value);
        for (bot_animations::Side side : {bot_animations::Side::Left,
                                          bot_animations::Side::Right}) {
            const unsigned before = canvas.calls;
            renderer.render(state, side, 2345);
            assert(canvas.calls > before);
        }
    }
}

}  // namespace

int main() {
    CountingCanvas canvas;
    bot_animations::Renderer renderer(canvas);
    bot_animations::State state;
    state.modeStartedMs = 1000;

    renderRange(renderer, canvas, state, bot_animations::ScreenMode::Eyes,
                &bot_animations::State::eye, bot_animations::kEyeStateCount);
    renderRange(renderer, canvas, state, bot_animations::ScreenMode::Music,
                &bot_animations::State::music, bot_animations::kMusicModeCount);
    renderRange(renderer, canvas, state, bot_animations::ScreenMode::Clock,
                &bot_animations::State::clock, bot_animations::kClockModeCount);
    renderRange(renderer, canvas, state, bot_animations::ScreenMode::Weather,
                &bot_animations::State::weather,
                bot_animations::kWeatherModeCount);
    renderRange(renderer, canvas, state, bot_animations::ScreenMode::System,
                &bot_animations::State::system,
                bot_animations::kSystemModeCount);
    static_assert(bot_animations::kVariantCount == 44,
                  "the extracted catalog must stay complete");
    return 0;
}
