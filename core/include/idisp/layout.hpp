// Overlay layouts: the presets in assets/layouts.json and the user's own in
// layouts.user.json (same format).
#pragma once
#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace idisp {

// Button names used in layouts and as image names (sets/<n>/butt<Name>.png).
// A button whose image a set lacks isn't drawn.
inline const std::array<const char*, 16> kButtonNames = {
    "A", "B", "X", "Y", "L", "R", "LT", "RT", "S", "Up", "Down", "Left", "Right", "Select", "L3", "R3",
};
inline constexpr size_t kButtonCount = kButtonNames.size();

// Element ids that aren't buttons.
inline constexpr const char* kStick = "stick";    // the left stick
inline constexpr const char* kStickR = "stickR";  // the right stick (hidden unless a layout places it)
inline constexpr const char* kToon = "toon";
inline constexpr const char* kCoords = "coords";

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
    bool operator==(const Rect&) const = default;
};

// Stick shapes, as in Settings::shapeIndex: Rays, Rays + cross, Octagon,
// Plain, Ring, Clock, Dot only (no ring: just the dot and its line), Target
// (blue ring, offset quarter arcs, black cross, red plus; fixed colors).
inline constexpr int kShapeCount = 8;
inline constexpr int kShapeDotOnly = 6;
inline constexpr int kShapeTarget = 7;

// Per-stick options set in the layout editor.
struct StickOptions {
    int shape = -1;      // -1: match the Shape setting; otherwise a shape index
    bool light = true;   // show the M-speed light on this stick
    bool operator==(const StickOptions&) const = default;
};

// A PNG the user added. `file` is a plain file name in the user's images
// folder (next to settings.ini); ids look like "img1".
struct LayoutImage {
    std::string id;
    std::string file;
    Rect rect;
    bool operator==(const LayoutImage&) const = default;
};
inline bool isImageId(const std::string& id) { return id.rfind("img", 0) == 0; }
// A file name with no folder parts, safe to join to the images folder.
bool safeImageFile(const std::string& name);

// The coordinates text block at scale 1: "X: 0.00", "Y: 0.00" and the rule
// label, as v4.43 placed them relative to the ring center.
inline constexpr int kCoordsW = 230, kCoordsH = 22;

struct Layout {
    std::string name;
    bool preset = false;            // from assets/layouts.json: read-only
    int width = 420, height = 300;  // canvas size
    int ringCenterX = 288;
    int ringCenterY = 144;
    float stickScale = 1.0f;        // the whole stick (rings, dot, travel) scaled around its center
    int rightX = 96, rightY = 204;  // right stick center
    float rightScale = 0.5f;
    std::array<StickOptions, 2> sticks;  // [0] left, [1] right
    Rect toon;                      // w == 0: v4.43 default around the ring
    Rect coords;                    // w == 0: v4.43 default under the ring
    std::map<std::string, Rect> buttons;  // keyed by kButtonNames
    std::set<std::string> hidden;         // element ids (kStick, kStickR, kToon, kCoords, button names, image ids)
    std::vector<LayoutImage> images;
    std::vector<std::string> order;       // layers, bottom first; empty = the default order

    bool shown(const std::string& id) const { return hidden.count(id) == 0; }
    Rect toonRect() const;
    Rect coordsRect() const;
    const LayoutImage* image(const std::string& id) const;
    // Every element id, bottom layer first. Ids missing from `order` are
    // added: images at the bottom, everything else on top, in the default
    // order (images, buttons, right stick, left stick, coordinates, toon).
    std::vector<std::string> drawOrder() const;
    bool operator==(const Layout&) const = default;
};

struct LayoutFile {
    int width = 420;   // default canvas size for layouts that don't set one
    int height = 300;
    std::vector<Layout> layouts;
};

// Throws std::runtime_error with a readable message on bad input. The presets
// file needs at least one layout; a user file may be empty. With `skipped`,
// a malformed layout entry is left out (its error added there) instead of
// failing the whole file.
LayoutFile parseLayouts(const std::string& json, bool allowEmpty = false, std::vector<std::string>* skipped = nullptr);
// Writes layouts in the same format (the user file).
std::string serializeLayouts(const std::vector<Layout>& layouts);

} // namespace idisp
