#include "idisp/settings.hpp"

#include <algorithm>
#include <cstdlib>
#include <set>
#include <sstream>

namespace idisp {

const std::vector<Field>& settingsSchema() {
    using S = Settings;
    static const std::vector<Field> fields = {
        { "showCoordinatesSwitch", "Show coordinates (F1)", "Display", &S::showCoordinates },
        { "showSourceNote", "Show the game-not-found note", "Display", &S::showSourceNote },
        { "toonSwitch", "Show toon (F2)", "Display", &S::showToon },
        { "showMSpeedSwitch", "Rule light mode (F3)", "Display", &S::showRuleIndicator },
        { "dPadSwitch", "D-pad moves stick (F4)", "Display", &S::dpadMovesStick },
        { "staticButtonOverlaySwitch", "Ghost buttons (F6)", "Display", &S::staticButtonOverlay },
        { "staticButtonOpacity", "Ghost button opacity", "Display", &S::staticButtonOpacity, 0, 255 },

        { "circleSize", "Dot size", "Stick", &S::circleSize, 1, 20 },
        { "joystickCircleColor", "Dot color", "Stick", &S::joystickCircleColor },
        { "stick", "Stick line color", "Stick", &S::stickColor },
        { "stickSmoothing", "Smooth drawn stick (not exact)", "Stick", &S::stickSmoothing },
        { "clampStickToRing", "Clamp drawn stick to ring", "Stick", &S::clampStickToRing },
        { "stickDotOnTop", "Stick dots above every layer", "Stick", &S::stickDotOnTop },

        { "pulseRate", "Pulse rate", "Rule glow", &S::pulseRate, 0.0001f, 0.05f },
        { "pulseAmplitude", "Pulse amplitude", "Rule glow", &S::pulseAmplitude, 0, 30 },
        { "joystickGlowColor", "Glow color", "Rule glow", &S::joystickGlowColor },
        { "secondGlowColor", "Second glow color", "Rule glow", &S::secondGlowColor },
        { "hitHoldMs", "Extra hold after a hit (ms)", "Rule glow", &S::hitHoldMs, 0, 100 },

        { "shapeIndex", "Shape (F5)", "Rings", &S::shapeIndex, 0, 7 },
        { "ringOuterRadius", "Outer ring radius", "Rings", &S::ringOuterRadius, 20, 140 },
        { "ringThickness", "Outer ring thickness", "Rings", &S::ringThickness, 5, 40 },
        { "ringColor", "Outer ring color", "Rings", &S::ringColor },
        { "ringOutlineColor", "Outer ring outline", "Rings", &S::ringOutlineColor },
        { "secondRingOuterRadius", "Inner ring radius", "Rings", &S::secondRingOuterRadius, 5, 120 },
        { "secondRingThickness", "Inner ring thickness", "Rings", &S::secondRingThickness, 2, 40 },
        { "secondRingColor", "Inner ring color", "Rings", &S::secondRingColor },
        { "secondRingOutlineColor", "Inner ring outline", "Rings", &S::secondRingOutlineColor },
        { "firstCrossColor", "Cross color", "Rings", &S::firstCrossColor, 0, 0, true },
        { "secondCrossColor", "Plus color", "Rings", &S::secondCrossColor },
        { "fill", "Fill color", "Rings", &S::fill, 0, 0, true },

        { "bg", "Background color", "Window", &S::bgColor },
        { "transparentWindow", "See-through window (restart)", "Window", &S::transparentWindow },
        { "overlayWidth", "Overlay window width", "Window", &S::overlayWidth, 0, 0 },
        { "overlayHeight", "Overlay window height", "Window", &S::overlayHeight, 0, 0 },

        { "ButtonSetIndex", "Button set (Left/Right)", "Assets", &S::buttonSetIndex, 0, 0 },
        { "ToonIndex", "Toon (Up/Down)", "Assets", &S::toonIndex, 0, 0 },
        { "buttonSetLayoutIndex", "Layout (F7)", "Assets", &S::buttonLayoutIndex, 0, 0 },

        { "profile", "Rule profile", "Input", &S::profile },
        { "windowsXInputOnly", "Windows: read Xbox pads via XInput (restart)", "Input", &S::windowsXInputOnly },
        { "showGameStick", "Show the game's stick value (F8)", "Input", &S::showGameStick },

        { "gameApplyDeadzone", "Apply deadzone to drawn game stick", "Game stick", &S::gameApplyDeadzone },
        { "gameDeadzone", "Game deadzone (per axis, also pad M-speed)", "Game stick", &S::gameDeadzone, 0.0f, 0.5f },
        { "gameRingMin", "M-speed ring: min distance", "Game stick", &S::gameRingMin, 0.0f, 0.5f },
        { "gameRingMax", "M-speed ring: max distance", "Game stick", &S::gameRingMax, 0.0f, 0.5f },
    };
    return fields;
}

static int toInt(const std::string& v, int fallback) {
    char* end = nullptr;
    long n = std::strtol(v.c_str(), &end, 10);
    return end == v.c_str() ? fallback : static_cast<int>(n);
}

static float toFloat(const std::string& v, float fallback) {
    char* end = nullptr;
    float f = std::strtof(v.c_str(), &end);
    return end == v.c_str() ? fallback : f;
}

Settings parseSettings(const std::string& text) {
    std::map<std::string, std::string> kv;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto eq = line.find('=');
        if (eq == std::string::npos || eq == 0) continue;
        kv[line.substr(0, eq)] = line.substr(eq + 1);
    }

    Settings s;
    std::set<std::string> used;
    auto take = [&](const std::string& key, std::string& out) {
        auto it = kv.find(key);
        if (it == kv.end()) return false;
        used.insert(key);
        out = it->second;
        return true;
    };

    for (const auto& f : settingsSchema()) {
        std::visit([&](auto m) {
            using T = std::decay_t<decltype(s.*m)>;
            std::string v;
            if constexpr (std::is_same_v<T, bool>) {
                if (take(f.key, v)) s.*m = toInt(v, s.*m ? 1 : 0) != 0;
            } else if constexpr (std::is_same_v<T, int>) {
                if (take(f.key, v)) s.*m = toInt(v, s.*m);
            } else if constexpr (std::is_same_v<T, float>) {
                if (take(f.key, v)) s.*m = toFloat(v, s.*m);
            } else if constexpr (std::is_same_v<T, std::string>) {
                if (take(f.key, v)) s.*m = v;
            } else {  // Rgba
                Rgba& c = s.*m;
                const std::string k = f.key;
                if (take(k + "R", v)) c.r = toInt(v, c.r);
                if (take(k + "G", v)) c.g = toInt(v, c.g);
                if (take(k + "B", v)) c.b = toInt(v, c.b);
                if (f.hasAlpha && take(k + "A", v)) c.a = toInt(v, c.a);
            }
        }, f.member);
    }
    for (const auto& [k, v] : kv)
        if (!used.count(k)) s.unknown[k] = v;
    clampSettings(s);
    return s;
}

std::string serializeSettings(const Settings& s) {
    std::ostringstream out;
    for (const auto& f : settingsSchema()) {
        std::visit([&](auto m) {
            using T = std::decay_t<decltype(s.*m)>;
            if constexpr (std::is_same_v<T, bool>) {
                out << f.key << '=' << (s.*m ? 1 : 0) << '\n';
            } else if constexpr (std::is_same_v<T, Rgba>) {
                const Rgba& c = s.*m;
                out << f.key << "R=" << c.r << '\n' << f.key << "G=" << c.g << '\n' << f.key << "B=" << c.b << '\n';
                if (f.hasAlpha) out << f.key << "A=" << c.a << '\n';
            } else {
                out << f.key << '=' << s.*m << '\n';
            }
        }, f.member);
    }
    for (const auto& [k, v] : s.unknown) out << k << '=' << v << '\n';
    return out.str();
}

void clampSettings(Settings& s) {
    auto ch = [](int& v) { v = std::clamp(v, 0, 255); };
    for (const auto& f : settingsSchema()) {
        std::visit([&](auto m) {
            using T = std::decay_t<decltype(s.*m)>;
            if constexpr (std::is_same_v<T, int> || std::is_same_v<T, float>) {
                if (f.min != 0 || f.max != 0) s.*m = std::clamp<T>(s.*m, static_cast<T>(f.min), static_cast<T>(f.max));
                else if (s.*m < 0) s.*m = 0;
            } else if constexpr (std::is_same_v<T, Rgba>) {
                Rgba& c = s.*m;
                ch(c.r); ch(c.g); ch(c.b); ch(c.a);
            }
        }, f.member);
    }
}

} // namespace idisp
