#include "procedural_animations.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace bot_animations;

namespace {

class BitmapCanvas final : public Canvas {
public:
    BitmapCanvas() : pixels_(kWidth * kHeight, 0) {}

    const std::vector<uint16_t>& pixels() const { return pixels_; }

    void fillScreen(uint16_t color) override {
        std::fill(pixels_.begin(), pixels_.end(), color);
    }
    void drawPixel(int16_t x, int16_t y, uint16_t color) override {
        if (x >= 0 && x < kWidth && y >= 0 && y < kHeight)
            pixels_[y * kWidth + x] = color;
    }
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                  uint16_t color) override {
        int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int error = dx + dy;
        while (true) {
            drawPixel(x0, y0, color);
            if (x0 == x1 && y0 == y1) break;
            const int twice = error * 2;
            if (twice >= dy) { error += dy; x0 += sx; }
            if (twice <= dx) { error += dx; y0 += sy; }
        }
    }
    void drawFastHLine(int16_t x, int16_t y, int16_t width,
                       uint16_t color) override {
        fillRect(x, y, width, 1, color);
    }
    void fillRect(int16_t x, int16_t y, int16_t width, int16_t height,
                  uint16_t color) override {
        const int x0 = std::max(0, static_cast<int>(x));
        const int y0 = std::max(0, static_cast<int>(y));
        const int x1 = std::min(static_cast<int>(kWidth),
                                static_cast<int>(x) + width);
        const int y1 = std::min(static_cast<int>(kHeight),
                                static_cast<int>(y) + height);
        for (int row = y0; row < y1; ++row)
            std::fill(pixels_.begin() + row * kWidth + x0,
                      pixels_.begin() + row * kWidth + x1, color);
    }
    void fillRoundRect(int16_t x, int16_t y, int16_t width, int16_t height,
                       int16_t radius, uint16_t color) override {
        radius = std::max<int16_t>(0, std::min<int16_t>(
            radius, std::min(width, height) / 2));
        fillRect(x + radius, y, width - radius * 2, height, color);
        fillRect(x, y + radius, radius, height - radius * 2, color);
        fillRect(x + width - radius, y + radius, radius,
                 height - radius * 2, color);
        fillCircle(x + radius, y + radius, radius, color);
        fillCircle(x + width - radius - 1, y + radius, radius, color);
        fillCircle(x + radius, y + height - radius - 1, radius, color);
        fillCircle(x + width - radius - 1, y + height - radius - 1,
                   radius, color);
    }
    void drawCircle(int16_t x, int16_t y, int16_t radius,
                    uint16_t color) override {
        int px = radius, py = 0, error = 1 - radius;
        while (px >= py) {
            for (const auto& point : circlePoints(x, y, px, py))
                drawPixel(point.first, point.second, color);
            ++py;
            if (error < 0) error += 2 * py + 1;
            else { --px; error += 2 * (py - px) + 1; }
        }
    }
    void fillCircle(int16_t x, int16_t y, int16_t radius,
                    uint16_t color) override {
        if (radius < 0) return;
        for (int dy = -radius; dy <= radius; ++dy) {
            const int span = static_cast<int>(
                std::sqrt(std::max(0, radius * radius - dy * dy)));
            fillRect(x - span, y + dy, span * 2 + 1, 1, color);
        }
    }
    void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, uint16_t color) override {
        const int minX = std::max(0, std::min({static_cast<int>(x0),
            static_cast<int>(x1), static_cast<int>(x2)}));
        const int maxX = std::min(static_cast<int>(kWidth) - 1,
            std::max({static_cast<int>(x0), static_cast<int>(x1),
                      static_cast<int>(x2)}));
        const int minY = std::max(0, std::min({static_cast<int>(y0),
            static_cast<int>(y1), static_cast<int>(y2)}));
        const int maxY = std::min(static_cast<int>(kHeight) - 1,
            std::max({static_cast<int>(y0), static_cast<int>(y1),
                      static_cast<int>(y2)}));
        const int area = edge(x0, y0, x1, y1, x2, y2);
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                const int a = edge(x0, y0, x1, y1, x, y);
                const int b = edge(x1, y1, x2, y2, x, y);
                const int c = edge(x2, y2, x0, y0, x, y);
                if ((area >= 0 && a >= 0 && b >= 0 && c >= 0) ||
                    (area < 0 && a <= 0 && b <= 0 && c <= 0))
                    drawPixel(x, y, color);
            }
        }
    }
    void drawArc(int16_t x, int16_t y, int16_t outerRadius,
                 int16_t innerRadius, int16_t startAngle, int16_t endAngle,
                 uint16_t foreground, uint16_t) override {
        if (endAngle < startAngle) std::swap(startAngle, endAngle);
        constexpr float kRadians = 3.14159265358979323846f / 180.0f;
        for (int angle = startAngle; angle <= endAngle; ++angle) {
            const float radians = angle * kRadians;
            for (int radius = innerRadius; radius <= outerRadius; ++radius)
                drawPixel(x + std::cos(radians) * radius,
                          y + std::sin(radians) * radius, foreground);
        }
    }
    void drawCenteredText(const char* text, int16_t x, int16_t y,
                          uint8_t font, uint16_t foreground,
                          uint16_t background) override {
        const int scale = font <= 2 ? 2 : font <= 4 ? 4 : font <= 6 ? 5 : 6;
        const int length = static_cast<int>(std::char_traits<char>::length(text));
        const int width = std::max(0, length * 4 * scale - scale);
        const int height = 5 * scale;
        fillRect(x - width / 2, y - height / 2, width, height, background);
        int cursor = x - width / 2;
        for (int index = 0; index < length; ++index) {
            const char* bits = glyph(text[index]);
            for (int row = 0; row < 5; ++row)
                for (int column = 0; column < 3; ++column)
                    if (bits[row * 3 + column] == '1')
                        fillRect(cursor + column * scale,
                                 y - height / 2 + row * scale,
                                 scale, scale, foreground);
            cursor += 4 * scale;
        }
    }

private:
    static int edge(int ax, int ay, int bx, int by, int px, int py) {
        return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
    }
    static std::vector<std::pair<int, int>> circlePoints(
        int x, int y, int px, int py
    ) {
        return {{x + px, y + py}, {x + py, y + px},
                {x - py, y + px}, {x - px, y + py},
                {x - px, y - py}, {x - py, y - px},
                {x + py, y - px}, {x + px, y - py}};
    }
    static const char* glyph(char value) {
        switch (value) {
        case 'A': return "010101111101101"; case 'B': return "110101110101110";
        case 'C': return "011100100100011"; case 'D': return "110101101101110";
        case 'E': return "111100110100111"; case 'F': return "111100110100100";
        case 'G': return "011100101101011"; case 'H': return "101101111101101";
        case 'I': return "111010010010111"; case 'J': return "001001001101010";
        case 'K': return "101101110101101"; case 'L': return "100100100100111";
        case 'M': return "101111111101101"; case 'N': return "101111111111101";
        case 'O': return "010101101101010"; case 'P': return "110101110100100";
        case 'Q': return "010101101111011"; case 'R': return "110101110101101";
        case 'S': return "011100010001110"; case 'T': return "111010010010010";
        case 'U': return "101101101101111"; case 'V': return "101101101101010";
        case 'W': return "101101111111101"; case 'X': return "101101010101101";
        case 'Y': return "101101010010010"; case 'Z': return "111001010100111";
        case '0': return "111101101101111"; case '1': return "010110010010111";
        case '2': return "110001111100111"; case '3': return "110001111001110";
        case '4': return "101101111001001"; case '5': return "111100110001110";
        case '6': return "011100111101111"; case '7': return "111001010010010";
        case '8': return "111101111101111"; case '9': return "111101111001110";
        case ':': return "000010000010000"; case '%': return "101001010100101";
        case '+': return "000010111010000"; case '-': return "000000111000000";
        case ')': return "100010001010100"; case ' ': return "000000000000000";
        default: return "111001010000010";
        }
    }

    std::vector<uint16_t> pixels_;
};

struct AnimationSpec {
    const char* name;
    ScreenMode screen;
    uint8_t mode;
    uint8_t frames;
    uint16_t frameMs;
    const char* playback;
};

constexpr AnimationSpec kAnimations[] = {
    {"eye-idle", ScreenMode::Eyes, 0, 8, 250, "loop"},
    {"eye-look", ScreenMode::Eyes, 1, 8, 825, "loop"},
    {"eye-alert", ScreenMode::Eyes, 2, 6, 105, "loop"},
    {"eye-angry", ScreenMode::Eyes, 3, 6, 327, "loop"},
    {"eye-surprised", ScreenMode::Eyes, 4, 1, 1000, "loop"},
    {"eye-happy", ScreenMode::Eyes, 5, 1, 1000, "loop"},
    {"eye-suspicious", ScreenMode::Eyes, 6, 6, 873, "loop"},
    {"eye-confused", ScreenMode::Eyes, 7, 1, 1000, "loop"},
    {"eye-dizzy", ScreenMode::Eyes, 8, 8, 231, "loop"},
    {"eye-listening", ScreenMode::Eyes, 9, 8, 196, "loop"},
    {"eye-glitch", ScreenMode::Eyes, 10, 8, 66, "loop"},
    {"eye-sleep", ScreenMode::Eyes, 11, 1, 1000, "loop"},
    {"eye-off", ScreenMode::Eyes, 12, 1, 1000, "loop"},
    {"music-play", ScreenMode::Music, 0, 6, 150, "loop"},
    {"music-pause", ScreenMode::Music, 1, 6, 150, "loop"},
    {"music-stop", ScreenMode::Music, 2, 6, 150, "loop"},
    {"music-prev-next", ScreenMode::Music, 3, 6, 100, "loop"},
    {"music-volume-pair", ScreenMode::Music, 4, 1, 1000, "loop"},
    {"music-equalizer", ScreenMode::Music, 5, 6, 150, "loop"},
    {"music-wave", ScreenMode::Music, 6, 8, 100, "loop"},
    {"clock-digital", ScreenMode::Clock, 0, 1, 1000, "loop"},
    {"clock-analog", ScreenMode::Clock, 1, 1, 1000, "loop"},
    {"clock-seconds", ScreenMode::Clock, 2, 1, 1000, "loop"},
    {"clock-minimal", ScreenMode::Clock, 3, 1, 1000, "loop"},
    {"clock-stopwatch", ScreenMode::Clock, 4, 6, 1000, "loop"},
    {"clock-timer", ScreenMode::Clock, 5, 6, 1000, "loop"},
    {"clock-alarm", ScreenMode::Clock, 6, 8, 625, "loop"},
    {"weather-sun", ScreenMode::Weather, 0, 8, 150, "loop"},
    {"weather-clouds", ScreenMode::Weather, 1, 1, 1000, "loop"},
    {"weather-rain", ScreenMode::Weather, 2, 1, 1000, "loop"},
    {"weather-storm", ScreenMode::Weather, 3, 1, 1000, "loop"},
    {"weather-snow", ScreenMode::Weather, 4, 1, 1000, "loop"},
    {"weather-wind", ScreenMode::Weather, 5, 8, 225, "loop"},
    {"weather-fog", ScreenMode::Weather, 6, 8, 525, "loop"},
    {"weather-night", ScreenMode::Weather, 7, 1, 1000, "loop"},
    {"system-power-on", ScreenMode::System, 0, 16, 333, "loop"},
    {"system-power-off", ScreenMode::System, 1, 16, 333, "loop"},
    {"system-update", ScreenMode::System, 2, 1, 1000, "loop"},
    {"system-install", ScreenMode::System, 3, 1, 1000, "loop"},
    {"system-wifi", ScreenMode::System, 4, 1, 1000, "loop"},
    {"system-battery", ScreenMode::System, 5, 1, 1000, "loop"},
    {"system-scan", ScreenMode::System, 6, 1, 1000, "loop"},
    {"system-ok", ScreenMode::System, 7, 1, 1000, "loop"},
    {"system-error", ScreenMode::System, 8, 1, 1000, "loop"},
};

void setMode(State& state, const AnimationSpec& spec) {
    state.screen = spec.screen;
    switch (spec.screen) {
    case ScreenMode::Eyes: state.eye = static_cast<EyeState>(spec.mode); break;
    case ScreenMode::Music: state.music = static_cast<MusicMode>(spec.mode); break;
    case ScreenMode::Clock: state.clock = static_cast<ClockMode>(spec.mode); break;
    case ScreenMode::Weather: state.weather = static_cast<WeatherMode>(spec.mode); break;
    case ScreenMode::System: state.system = static_cast<SystemMode>(spec.mode); break;
    }
}

uint8_t expand5(uint8_t value) { return (value << 3) | (value >> 2); }
uint8_t expand6(uint8_t value) { return (value << 2) | (value >> 4); }

void writePpm(const fs::path& path, const BitmapCanvas& canvas) {
    std::ofstream output(path, std::ios::binary);
    output << "P6\n" << kWidth << ' ' << kHeight << "\n255\n";
    for (uint16_t color : canvas.pixels()) {
        const uint8_t rgb[] = {
            expand5((color >> 11) & 0x1f),
            expand6((color >> 5) & 0x3f),
            expand5(color & 0x1f),
        };
        output.write(reinterpret_cast<const char*>(rgb), sizeof(rgb));
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const fs::path outputDirectory = argv[1];
    fs::create_directories(outputDirectory);
    std::ofstream manifest(outputDirectory / "manifest.tsv");
    manifest << "name\tframes\tframe_ms\tplayback\n";
    for (const AnimationSpec& spec : kAnimations) {
        manifest << spec.name << '\t' << static_cast<int>(spec.frames) << '\t'
                 << spec.frameMs << '\t' << spec.playback << '\n';
        State state;
        setMode(state, spec);
        state.modeStartedMs = 0;
        for (uint8_t frame = 0; frame < spec.frames; ++frame) {
            const uint32_t nowMs = frame * spec.frameMs;
            for (Side side : {Side::Left, Side::Right}) {
                BitmapCanvas canvas;
                Renderer renderer(canvas);
                renderer.render(state, side, nowMs);
                const char* role = side == Side::Left ? "left" : "right";
                const std::string filename = std::string(spec.name) + "-" +
                    role + "-" + (frame < 10 ? "0" : "") +
                    std::to_string(frame) + ".ppm";
                writePpm(outputDirectory / filename, canvas);
            }
        }
    }
    return 0;
}
