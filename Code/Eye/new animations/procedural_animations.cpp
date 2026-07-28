#include "procedural_animations.hpp"

#include <cmath>
#include <cstdio>

namespace bot_animations {
namespace {

constexpr int16_t kCenterX = 120;
constexpr int16_t kCenterY = 120;
constexpr float kPi = 3.14159265358979323846f;

constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xffff;
constexpr uint16_t kEye = 0xef13;
constexpr uint16_t kRed = 0xd800;
constexpr uint16_t kCyan = 0x57ff;
constexpr uint16_t kAmber = 0xfea0;
constexpr uint16_t kGray = 0x8410;
constexpr uint16_t kDark = 0x1082;
constexpr uint16_t kNight = 0x0841;
constexpr uint16_t kWeather = 0x1a8b;

float seconds(uint32_t milliseconds) {
    return static_cast<float>(milliseconds) * 0.001f;
}

float clamp01(float value) {
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

float easeOutCubic(float value) {
    const float remaining = 1.0f - clamp01(value);
    return 1.0f - remaining * remaining * remaining;
}

int16_t wave(float time, float amplitude, float speed = 1.0f,
             float phase = 0.0f) {
    return static_cast<int16_t>(std::sin(time * speed + phase) * amplitude);
}

float musicBar(uint8_t index, float time) {
    return 0.2f + 0.75f * std::fabs(
        std::sin(time * (1.8f + index * 0.6f) + index));
}

template <typename Enum, size_t Size>
const char* enumName(Enum value, const char* const (&names)[Size]) {
    const size_t index = static_cast<size_t>(value);
    return index < Size ? names[index] : "unknown";
}

}  // namespace

void Renderer::render(const State& state, Side side, uint32_t nowMs) {
    const bool left = side == Side::Left;
    switch (state.screen) {
    case ScreenMode::Eyes: drawEye(state, left, nowMs); break;
    case ScreenMode::Music: drawMusic(state, left, nowMs); break;
    case ScreenMode::Clock: drawClock(state, left, nowMs); break;
    case ScreenMode::Weather: drawWeather(state, left, nowMs); break;
    case ScreenMode::System: drawSystem(state, left, nowMs); break;
    }
}

void Renderer::drawRoundBackground(uint16_t color) {
    canvas_.fillScreen(kBlack);
    canvas_.fillCircle(kCenterX, kCenterY, 119, color);
}

void Renderer::drawBezel() {
    canvas_.drawCircle(kCenterX, kCenterY, 119, 0x39e7);
    canvas_.drawCircle(kCenterX, kCenterY, 118, 0x2104);
}

void Renderer::fillSlantedTop(float amount, int16_t slant) {
    const int16_t y = static_cast<int16_t>(amount * kHeight);
    canvas_.fillTriangle(0, 0, kWidth, 0, 0, y - slant, kBlack);
    canvas_.fillTriangle(kWidth, 0, kWidth, y + slant, 0, y - slant, kBlack);
}

void Renderer::fillSlantedBottom(float amount, int16_t slant) {
    const int16_t y = kHeight - static_cast<int16_t>(amount * kHeight);
    canvas_.fillTriangle(0, kHeight, kWidth, kHeight, 0, y - slant, kBlack);
    canvas_.fillTriangle(kWidth, kHeight, kWidth, y + slant, 0, y - slant,
                         kBlack);
}

void Renderer::drawPupil(int16_t x, int16_t y, int16_t width, int16_t height) {
    if (width <= 0 || height <= 0) return;
    const int16_t radius = height / 2 < 4 ? height / 2 : 4;
    canvas_.fillRoundRect(x - width / 2, y - height / 2, width, height,
                          radius, kBlack);
}

void Renderer::drawEye(const State& state, bool left, uint32_t nowMs) {
    const float time = seconds(nowMs);
    float top = 0.08f;
    float bottom = 0.06f;
    int16_t pupilX = kCenterX + (left ? -2 : 2);
    int16_t pupilY = kCenterY;
    int16_t pupilWidth = 30;
    int16_t pupilHeight = 30;
    int16_t topSlant = 0;
    int16_t bottomSlant = 0;
    uint16_t background = kEye;

    switch (state.eye) {
    case EyeState::Idle:
        pupilX += wave(time, 15, 0.55f);
        pupilY += wave(time, 5, 0.38f, 0.8f);
        break;
    case EyeState::Look:
        top = 0.06f; bottom = 0.05f;
        pupilX += wave(time, 34, 0.95f) + (left ? -3 : 3);
        pupilY += wave(time, 12, 0.72f, 0.4f);
        break;
    case EyeState::Alert:
        background = kRed; top = 0.14f; bottom = 0.10f;
        pupilWidth = pupilHeight = 34 + wave(time, 4, 10, left ? 0.4f : 2.1f);
        pupilX += wave(time, 8, 9, left ? 0.4f : 2.1f);
        break;
    case EyeState::Angry:
        top = 0.38f; bottom = 0.34f; pupilWidth = 34; pupilHeight = 26;
        topSlant = left ? 18 : -18;
        bottomSlant = left ? -12 : 12;
        pupilX += (left ? 10 : -10) + wave(time, 3, 3.2f);
        pupilY -= 3;
        break;
    case EyeState::Surprised:
        top = 0; bottom = 0; pupilWidth = pupilHeight = 42;
        pupilX += left ? -18 : 18; pupilY -= 2;
        break;
    case EyeState::Happy:
        top = 0.17f; bottom = 0.30f; pupilWidth = 42; pupilHeight = 10;
        pupilY += 12; bottomSlant = left ? -10 : 10;
        break;
    case EyeState::Suspicious:
        top = 0.28f; bottom = 0.24f; pupilWidth = 26; pupilHeight = 18;
        pupilY -= 2; topSlant = left ? -8 : 8;
        bottomSlant = left ? 4 : -4;
        pupilX += 24 + (left ? -2 : 2) + wave(time, 4, 1.2f);
        break;
    case EyeState::Confused:
        top = left ? 0.10f : 0.30f;
        bottom = left ? 0.14f : 0.08f;
        pupilWidth = left ? 40 : 24; pupilHeight = left ? 40 : 20;
        pupilX += left ? -18 : 18; pupilY += left ? -6 : 8;
        break;
    case EyeState::Dizzy: {
        top = 0.07f; bottom = 0.07f; pupilWidth = pupilHeight = 18;
        const float phase = left ? 0.3f : 2.1f;
        pupilX += static_cast<int16_t>(std::cos(time * 3.4f + phase) * 22);
        pupilY += static_cast<int16_t>(std::sin(time * 3.4f + phase) * 16);
        break;
    }
    case EyeState::Listening:
        top = 0.12f; bottom = 0.12f;
        pupilWidth = 16 + std::abs(wave(time, 10, 4));
        pupilHeight = 26 + std::abs(wave(time, 18, 3.2f));
        pupilX += wave(time, 8, 1.4f);
        break;
    case EyeState::Glitch:
        background = (nowMs / 90) % 2 ? kRed : kEye;
        top = 0.12f; bottom = 0.12f;
        pupilWidth = 24 + (nowMs / 33) % 36;
        pupilHeight = 6 + (nowMs / 47) % 22;
        pupilX += static_cast<int16_t>((nowMs / 17) % 44) - 22;
        pupilY += static_cast<int16_t>((nowMs / 23) % 24) - 12;
        break;
    case EyeState::Sleep:
        top = 0.48f; bottom = 0.48f; pupilWidth = 54; pupilHeight = 5;
        break;
    case EyeState::Off:
        top = 0.50f; bottom = 0.50f; pupilWidth = pupilHeight = 0;
        break;
    }

    drawRoundBackground(background);
    fillSlantedTop(top, topSlant);
    fillSlantedBottom(bottom, bottomSlant);
    drawPupil(pupilX, pupilY, pupilWidth, pupilHeight);
    drawBezel();
}

void Renderer::drawEqualizer(bool withTime, uint32_t nowMs) {
    const float time = seconds(nowMs);
    drawRoundBackground(kDark);
    for (uint8_t index = 0; index < 7; ++index) {
        const int16_t height = 16 + static_cast<int16_t>(musicBar(index, time) * 82);
        const int16_t x = kCenterX - 66 + index * 22;
        canvas_.fillRect(x - 7, kCenterY - height / 2, 14, height, kCyan);
    }
    if (withTime) {
        canvas_.drawCenteredText("2:14", kCenterX, kCenterY + 58, 4,
                                 kWhite, kDark);
    }
    drawBezel();
}

void Renderer::drawMusicIcon(MusicMode mode, uint32_t nowMs) {
    const float pulse = 1.0f + std::fabs(std::sin(
        seconds(nowMs) * 2.0f * kPi)) * 0.04f;
    canvas_.drawCircle(kCenterX, kCenterY, 82, kCyan);
    canvas_.drawCircle(kCenterX, kCenterY, 81, kCyan);
    if (mode == MusicMode::Play) {
        canvas_.fillTriangle(
            kCenterX - 26 * pulse, kCenterY - 44 * pulse,
            kCenterX - 26 * pulse, kCenterY + 44 * pulse,
            kCenterX + 48 * pulse, kCenterY, kCyan);
    } else if (mode == MusicMode::Pause) {
        const int16_t height = 92 * pulse;
        canvas_.fillRoundRect(kCenterX - 36, kCenterY - height / 2,
                              24, height, 5, kCyan);
        canvas_.fillRoundRect(kCenterX + 12, kCenterY - height / 2,
                              24, height, 5, kCyan);
    } else if (mode == MusicMode::Stop) {
        const int16_t size = 76 * pulse;
        canvas_.fillRoundRect(kCenterX - size / 2, kCenterY - size / 2,
                              size, size, 8, kCyan);
    }
}

void Renderer::drawMusic(const State& state, bool left, uint32_t nowMs) {
    drawRoundBackground(kDark);
    const float time = seconds(nowMs);
    if (state.music == MusicMode::PrevNext) {
        const int16_t nudge = std::abs(wave(time, 5, 5));
        if (left) {
            canvas_.fillRect(kCenterX - 45 - nudge, kCenterY - 38, 16, 76, kCyan);
            canvas_.fillTriangle(kCenterX + 34 - nudge, kCenterY - 42,
                                 kCenterX + 34 - nudge, kCenterY + 42,
                                 kCenterX - 22 - nudge, kCenterY, kCyan);
        } else {
            canvas_.fillRect(kCenterX + 29 + nudge, kCenterY - 38, 16, 76, kCyan);
            canvas_.fillTriangle(kCenterX - 34 + nudge, kCenterY - 42,
                                 kCenterX - 34 + nudge, kCenterY + 42,
                                 kCenterX + 22 + nudge, kCenterY, kCyan);
        }
    } else if (state.music == MusicMode::VolumePair) {
        canvas_.fillRect(kCenterX - 52, kCenterY - 22, 22, 44, kCyan);
        canvas_.fillTriangle(kCenterX - 30, kCenterY - 34, kCenterX + 2,
                             kCenterY - 52, kCenterX + 2, kCenterY + 52, kCyan);
        canvas_.drawCenteredText(left ? "-" : "+", kCenterX + 52, kCenterY,
                                 7, kWhite, kDark);
    } else if (state.music == MusicMode::Equalizer) {
        drawEqualizer(!left, nowMs);
        return;
    } else if (state.music == MusicMode::Wave) {
        for (int16_t x = 0; x < kWidth; x += 3) {
            const int16_t y = kCenterY + static_cast<int16_t>(
                (left ? 26 : 40) * std::sin(x * 0.08f + time * 3));
            canvas_.drawPixel(x, y, kCyan);
            canvas_.drawPixel(x, y + 1, kCyan);
        }
    } else {
        drawMusicIcon(state.music, nowMs);
    }
    drawBezel();
}

void Renderer::drawClock(const State& state, bool left, uint32_t nowMs) {
    drawRoundBackground(kDark);
    const float time = seconds(nowMs);
    if (state.clock == ClockMode::Alarm) {
        if (left) {
            const float pulse = 0.5f + 0.5f * std::sin(time * 5);
            canvas_.drawCircle(kCenterX, kCenterY, 72, kGray);
            canvas_.drawArc(kCenterX, kCenterY, 78, 72, -90,
                            175 + static_cast<int16_t>(pulse * 18), kRed, kDark);
            canvas_.drawCenteredText("07:30", kCenterX, kCenterY - 4, 4,
                                     kWhite, kDark);
            canvas_.drawCenteredText("WAKE UP", kCenterX, kCenterY + 36, 2,
                                     kWhite, kDark);
        } else {
            canvas_.drawArc(kCenterX, kCenterY, 82, 76, -90,
                            static_cast<int16_t>(nowMs / 1000 * 72 % 360),
                            kRed, kDark);
            canvas_.drawCenteredText("ON", kCenterX, kCenterY - 6, 6,
                                     kWhite, kDark);
            canvas_.drawCenteredText("ALARM SET", kCenterX, kCenterY + 34, 2,
                                     kWhite, kDark);
        }
    } else if (state.clock == ClockMode::Stopwatch) {
        const uint32_t tenths = nowMs / 100;
        char buffer[10];
        std::snprintf(buffer, sizeof(buffer), "%02lu:%02lu",
                      static_cast<unsigned long>((tenths / 10) / 60),
                      static_cast<unsigned long>((tenths / 10) % 60));
        canvas_.drawCenteredText(left ? buffer : "START", kCenterX, kCenterY,
                                 left ? 6 : 4, kWhite, kDark);
    } else if (state.clock == ClockMode::Timer) {
        const uint16_t remaining = 300 - (nowMs / 1000) % 300;
        char buffer[10];
        std::snprintf(buffer, sizeof(buffer), "%u:%02u", remaining / 60,
                      remaining % 60);
        canvas_.drawCenteredText(left ? buffer : "TIMER", kCenterX, kCenterY,
                                 left ? 6 : 4, kWhite, kDark);
    } else {
        // Digital, Analog, Seconds and Minimal share this fallback in the source.
        canvas_.drawCenteredText(left ? "07:30" : "SEC", kCenterX, kCenterY,
                                 6, kWhite, kDark);
    }
    drawBezel();
}

void Renderer::drawWeather(const State& state, bool left, uint32_t nowMs) {
    drawRoundBackground(state.weather == WeatherMode::Night ? kNight : kWeather);
    const float time = seconds(nowMs);
    if (!left) {
        const char* temperature = state.weather == WeatherMode::Snow ? "2" :
            state.weather == WeatherMode::Rain ? "16" : "24";
        const char* label = state.weather == WeatherMode::Wind ? "WIND" :
            state.weather == WeatherMode::Fog ? "FOG" :
            state.weather == WeatherMode::Night ? "NIGHT" : "WEATHER";
        canvas_.drawCenteredText(temperature, kCenterX, kCenterY - 30, 7,
                                 kWhite, kDark);
        canvas_.drawCenteredText(label, kCenterX, kCenterY + 32, 2,
                                 kWhite, kDark);
        drawBezel();
        return;
    }
    switch (state.weather) {
    case WeatherMode::Sun:
        canvas_.fillCircle(kCenterX, kCenterY,
                           44 + std::abs(wave(time, 2, 2)), kAmber);
        for (uint8_t index = 0; index < 12; ++index) {
            const float angle = index * kPi / 6 + time * 0.3f;
            canvas_.drawLine(kCenterX + std::cos(angle) * 60,
                             kCenterY + std::sin(angle) * 60,
                             kCenterX + std::cos(angle) * 82,
                             kCenterY + std::sin(angle) * 82, kAmber);
        }
        break;
    case WeatherMode::Wind:
        for (uint8_t index = 0; index < 5; ++index)
            canvas_.drawFastHLine(34 + ((nowMs / 20 + index * 29) % 90) - 45,
                                  56 + index * 30, 160, kCyan);
        break;
    case WeatherMode::Fog:
        for (uint8_t index = 0; index < 8; ++index)
            canvas_.drawFastHLine(15 + ((nowMs / 60 + index * 31) % 70) - 35,
                                  48 + index * 22, 210, kGray);
        break;
    case WeatherMode::Night:
        canvas_.fillCircle(kCenterX - 12, kCenterY - 8, 46, 0xff76);
        canvas_.fillCircle(kCenterX + 10, kCenterY - 22, 42, kNight);
        for (uint8_t index = 0; index < 18; ++index)
            canvas_.drawPixel(22 + (index * 47) % 198,
                              26 + (index * 61) % 160, kWhite);
        break;
    default:
        // Clouds, Rain, Storm and Snow share this cloud placeholder upstream.
        canvas_.fillRoundRect(48, 82, 144, 72, 28, kWhite);
        break;
    }
    drawBezel();
}

void Renderer::drawPowerEye(bool left, bool on, uint32_t elapsedMs) {
    float progress = clamp01(seconds(elapsedMs) / 5.0f);
    if (!on) progress = 1.0f - progress;
    progress = easeOutCubic(progress);
    drawRoundBackground(kEye);
    fillSlantedTop(0.50f - 0.42f * progress, left ? 4 : -4);
    fillSlantedBottom(0.50f - 0.44f * progress, left ? -4 : 4);
    const int16_t size = static_cast<int16_t>(30 * progress);
    drawPupil(kCenterX + (left ? -2 : 2), kCenterY, size, size);
    drawBezel();
}

void Renderer::drawSystem(const State& state, bool left, uint32_t nowMs) {
    const uint32_t elapsed = nowMs - state.modeStartedMs;
    if (state.system == SystemMode::PowerOn) {
        drawPowerEye(left, true, elapsed); return;
    }
    if (state.system == SystemMode::PowerOff) {
        drawPowerEye(left, false, elapsed); return;
    }
    drawRoundBackground(kDark);
    const char* text;
    uint8_t font = 5;
    if (state.system == SystemMode::Wifi) { text = left ? ")))" : "WIFI"; font = 6; }
    else if (state.system == SystemMode::Battery) { text = left ? "BAT" : "82%"; font = 6; }
    else if (state.system == SystemMode::Ok) text = left ? "OK" : "READY";
    else if (state.system == SystemMode::Error) text = left ? "X" : "ERROR";
    else text = left ? "SCAN" : "75%";
    canvas_.drawCenteredText(text, kCenterX, kCenterY, font, kWhite, kDark);
    drawBezel();
}

const char* name(EyeState value) {
    static const char* const names[] = {"idle", "look", "alert", "angry",
        "surprised", "happy", "suspicious", "confused", "dizzy",
        "listening", "glitch", "sleep", "off"};
    return enumName(value, names);
}
const char* name(MusicMode value) {
    static const char* const names[] = {"play", "pause", "stop", "prev-next",
        "volume-pair", "equalizer", "wave"};
    return enumName(value, names);
}
const char* name(ClockMode value) {
    static const char* const names[] = {"digital", "analog", "seconds",
        "minimal", "stopwatch", "timer", "alarm"};
    return enumName(value, names);
}
const char* name(WeatherMode value) {
    static const char* const names[] = {"sun", "clouds", "rain", "storm",
        "snow", "wind", "fog", "night"};
    return enumName(value, names);
}
const char* name(SystemMode value) {
    static const char* const names[] = {"power-on", "power-off", "update",
        "install", "wifi", "battery", "scan", "ok", "error"};
    return enumName(value, names);
}

}  // namespace bot_animations
