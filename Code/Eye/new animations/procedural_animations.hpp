#pragma once

#include <cstdint>

namespace bot_animations {

constexpr int16_t kWidth = 240;
constexpr int16_t kHeight = 240;
constexpr uint32_t kFrameIntervalMs = 33;

enum class Side : uint8_t { Left, Right };
enum class ScreenMode : uint8_t { Eyes, Music, Clock, Weather, System };
enum class EyeState : uint8_t {
    Idle, Look, Alert, Angry, Surprised, Happy, Suspicious,
    Confused, Dizzy, Listening, Glitch, Sleep, Off,
};
enum class MusicMode : uint8_t {
    Play, Pause, Stop, PrevNext, VolumePair, Equalizer, Wave,
};
enum class ClockMode : uint8_t {
    Digital, Analog, Seconds, Minimal, Stopwatch, Timer, Alarm,
};
enum class WeatherMode : uint8_t {
    Sun, Clouds, Rain, Storm, Snow, Wind, Fog, Night,
};
enum class SystemMode : uint8_t {
    PowerOn, PowerOff, Update, Install, Wifi, Battery, Scan, Ok, Error,
};

constexpr uint8_t kEyeStateCount = 13;
constexpr uint8_t kMusicModeCount = 7;
constexpr uint8_t kClockModeCount = 7;
constexpr uint8_t kWeatherModeCount = 8;
constexpr uint8_t kSystemModeCount = 9;
constexpr uint8_t kVariantCount = kEyeStateCount + kMusicModeCount +
    kClockModeCount + kWeatherModeCount + kSystemModeCount;

struct State {
    ScreenMode screen = ScreenMode::Eyes;
    EyeState eye = EyeState::Idle;
    MusicMode music = MusicMode::Equalizer;
    ClockMode clock = ClockMode::Digital;
    WeatherMode weather = WeatherMode::Sun;
    SystemMode system = SystemMode::PowerOn;
    uint32_t modeStartedMs = 0;
};

// Implement this small primitive API for the target display library. Coordinates
// and colors match TFT_eSPI: colors are RGB565 and text is centered at x/y.
class Canvas {
public:
    virtual ~Canvas() = default;
    virtual void fillScreen(uint16_t color) = 0;
    virtual void drawPixel(int16_t x, int16_t y, uint16_t color) = 0;
    virtual void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                          uint16_t color) = 0;
    virtual void drawFastHLine(int16_t x, int16_t y, int16_t width,
                               uint16_t color) = 0;
    virtual void fillRect(int16_t x, int16_t y, int16_t width, int16_t height,
                          uint16_t color) = 0;
    virtual void fillRoundRect(int16_t x, int16_t y, int16_t width,
                               int16_t height, int16_t radius,
                               uint16_t color) = 0;
    virtual void drawCircle(int16_t x, int16_t y, int16_t radius,
                            uint16_t color) = 0;
    virtual void fillCircle(int16_t x, int16_t y, int16_t radius,
                            uint16_t color) = 0;
    virtual void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                              int16_t x2, int16_t y2, uint16_t color) = 0;
    virtual void drawArc(int16_t x, int16_t y, int16_t outerRadius,
                         int16_t innerRadius, int16_t startAngle,
                         int16_t endAngle, uint16_t foreground,
                         uint16_t background) = 0;
    virtual void drawCenteredText(const char* text, int16_t x, int16_t y,
                                  uint8_t font, uint16_t foreground,
                                  uint16_t background) = 0;
};

class Renderer {
public:
    explicit Renderer(Canvas& canvas) : canvas_(canvas) {}

    // nowMs is supplied by the caller, making previews and synchronized eyes
    // deterministic. Render once for each physical display at about 30 FPS.
    void render(const State& state, Side side, uint32_t nowMs);

private:
    void drawEye(const State& state, bool left, uint32_t nowMs);
    void drawMusic(const State& state, bool left, uint32_t nowMs);
    void drawClock(const State& state, bool left, uint32_t nowMs);
    void drawWeather(const State& state, bool left, uint32_t nowMs);
    void drawSystem(const State& state, bool left, uint32_t nowMs);
    void drawEqualizer(bool withTime, uint32_t nowMs);
    void drawPowerEye(bool left, bool on, uint32_t elapsedMs);
    void drawRoundBackground(uint16_t color);
    void drawBezel();
    void fillSlantedTop(float amount, int16_t slant);
    void fillSlantedBottom(float amount, int16_t slant);
    void drawPupil(int16_t x, int16_t y, int16_t width, int16_t height);

    Canvas& canvas_;
};

const char* name(EyeState value);
const char* name(MusicMode value);
const char* name(ClockMode value);
const char* name(WeatherMode value);
const char* name(SystemMode value);

}  // namespace bot_animations
