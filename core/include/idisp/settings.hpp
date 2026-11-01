// User settings. One schema table drives loading, saving and the settings UI,
// so a new setting is added in exactly two places: the struct and the table.
// The file format is the v4.43 settings.ini (key=value, same key names), so
// an existing settings.ini can be dropped in.
#pragma once
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace idisp {

struct Rgba {
    int r = 255, g = 255, b = 255, a = 255;
};

struct Settings {
    // Display toggles
    bool showCoordinates = false;
    bool showSourceNote = false;     // draw a source note when the game value is unavailable
    bool showRuleIndicator = false;  // "M-speed light mode"
    bool dpadMovesStick = false;
    bool staticButtonOverlay = false;
    int staticButtonOpacity = 27;

    // Stick indicator
    int circleSize = 4;
    Rgba joystickCircleColor{ 255, 255, 255, 255 };
    Rgba stickColor{ 0, 0, 0, 255 };
    bool stickSmoothing = false;     // v4.43 always smoothed (factor 0.7); off = exact position
    bool clampStickToRing = true;    // v4.43 always clamped the drawn dot to radius 128
    bool stickDotOnTop = true;       // dots and lines above every layer; off = drawn with their ring's layer

    // Rule (M-speed) glow
    float pulseRate = 0.005f;
    int pulseAmplitude = 6;
    Rgba joystickGlowColor{ 255, 0, 0, 255 };
    Rgba secondGlowColor{ 177, 223, 255, 255 };
    int hitHoldMs = 0;               // extra time a hit stays lit; 0 = next frame only

    // Rings and crosshairs
    int shapeIndex = 0;              // idisp::kShapeCount shapes; 6 = dot only, 7 = target
    int ringOuterRadius = 128;
    int ringThickness = 8;
    Rgba ringColor{ 255, 255, 255, 255 };
    Rgba ringOutlineColor{ 0, 0, 0, 255 };
    int secondRingOuterRadius = 40;
    int secondRingThickness = 4;
    Rgba secondRingColor{ 255, 255, 255, 255 };
    Rgba secondRingOutlineColor{ 0, 0, 0, 255 };
    Rgba firstCrossColor{ 255, 0, 0, 255 };
    Rgba secondCrossColor{ 255, 255, 255, 255 };
    Rgba fill{ 163, 163, 163, 255 };

    // Window
    Rgba bgColor{ 0, 0, 0, 255 };
    bool transparentWindow = false;  // transparent OS window; restart needed
    int overlayWidth = 0;            // overlay window size as last left; 0 = the layout canvas size
    int overlayHeight = 0;

    // Asset selection
    int buttonSetIndex = 0;
    int buttonLayoutIndex = 0;

    // Input
    std::string profile = "sonic-generations-50";
    bool windowsXInputOnly = true;   // read Xbox pads through XInput, like most PC games; restart needed
    bool showGameStick = false;      // draw the game's own stick value (memory) instead of the pad's

    // Game stick (values -1..1, as the game stores them)
    bool gameApplyDeadzone = true;   // draw the game value with the deadzone added back, so it lines up with the pad
    float gameDeadzone = 0.265f;     // the game's per-axis deadzone: for that drawing and pad M-speed (probe fit 2026-10-31)
    float gameRingMin = 0.03f;       // M-speed on the game value: distance from centre between these
    float gameRingMax = 0.077f;

    // Keys we don't recognize (e.g. v4.43 threshold keys), written back unchanged.
    std::map<std::string, std::string> unknown;
};

using Member = std::variant<bool Settings::*, int Settings::*, float Settings::*,
                            Rgba Settings::*, std::string Settings::*>;

struct Field {
    const char* key;    // ini key; for colors the prefix of <key>R/G/B(/A)
    const char* label;
    const char* group;
    Member member;
    float min = 0, max = 0;  // for int/float; both 0 = unbounded
    bool hasAlpha = false;   // colors only
};

const std::vector<Field>& settingsSchema();

Settings parseSettings(const std::string& text);
std::string serializeSettings(const Settings& s);
void clampSettings(Settings& s);

} // namespace idisp
