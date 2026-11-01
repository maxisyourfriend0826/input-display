// The captured overlay window's drawing. Ported from v4.43's render loop.
#pragma once
#include "idisp/layout.hpp"
#include "idisp/settings.hpp"
#include "text.hpp"

#include <SDL3/SDL.h>
#include <array>
#include <map>
#include <string>
#include <vector>

// Everything the overlay needs to draw one frame.
struct FrameInput {
    float unitsX = 0, unitsY = 0;  // exact stick units (shown as coordinates)
    int drawX = 0, drawY = 0;      // integer position for the dot (maybe smoothed)
    int drawXR = 0, drawYR = 0;    // the right stick's dot
    std::array<bool, idisp::kButtonCount> buttons{}; // in idisp::kButtonNames order
    bool ruleLit = false;
    bool ruleLitR = false;          // the ring rule on the right stick's pad value
    std::string ruleLabel = "M";
    std::string sourceNote;         // e.g. "pad (game not found)"; drawn top-left when set
};

class Overlay {
public:
    bool init(SDL_Renderer* renderer, const std::string& assetsDir);
    void shutdown();

    void setLayouts(const idisp::LayoutFile& f) { layouts_ = f; }
    const idisp::LayoutFile& layouts() const { return layouts_; }

    const std::vector<std::string>& buttonSets() const { return buttonSets_; }
    const std::vector<std::string>& toonSets() const { return toonSets_; }
    void loadButtonSet(int index);
    void loadToonSet(int index);

    // Folder holding the layouts' images (user data, next to settings.ini).
    void setImagesDir(const std::string& dir) { imagesDir_ = dir; }

    // Call after any setting that changes the static parts (shape, colors, layout).
    void markDirty() { dirty_ = true; }

    // While the layout editor is open the overlay draws its working copy
    // (nullptr: the layout chosen in settings).
    void setEditing(const idisp::Layout* l) { editing_ = l; }
    // The layout being drawn now.
    const idisp::Layout& current(const idisp::Settings& s) const;

    // Draws the layout canvas, scaled uniformly to fit the window (centered;
    // the space left over stays transparent).
    void render(const idisp::Settings& s, const FrameInput& in);

private:
    // One shape's prebuilt textures (built on first use, dropped on markDirty).
    struct ShapeTex {
        bool built = false;
        SDL_Texture* base = nullptr;  // fill, crosses, ticks
        SDL_Texture *outer = nullptr, *inner = nullptr, *outerWhite = nullptr, *innerWhite = nullptr;
    };
    void drawCanvas(const idisp::Settings& s, const idisp::Layout& L, const FrameInput& in);
    void drawStickBase(const idisp::Settings& s, int shape, int cx, int cy, bool lit);
    void drawStickTop(const idisp::Settings& s, int cx, int cy, int dx, int dy, bool lit);
    template <class Pass>
    void stickPass(int cx, int cy, float scale, Pass pass);
    int stickHalf(const idisp::Settings& s) const;
    ShapeTex& shape(const idisp::Settings& s, int index);
    void buildBase(const idisp::Settings& s, int shape, ShapeTex& t);
    void buildRings(const idisp::Settings& s, int shape, ShapeTex& t);
    void destroyShapes();
    SDL_Texture* image(const std::string& file);
    const idisp::Layout& layout(const idisp::Settings& s) const;
    SDL_Texture* makeTarget(int w, int h);

    SDL_Renderer* r_ = nullptr;
    std::string assets_;
    idisp::LayoutFile layouts_;
    std::vector<std::string> buttonSets_, toonSets_;
    std::array<SDL_Texture*, idisp::kButtonCount> buttonTex_{};
    std::array<SDL_Texture*, 8> toon_{}, toonM_{};
    std::array<ShapeTex, idisp::kShapeCount> shapes_{};
    std::string imagesDir_;
    std::map<std::string, SDL_Texture*> images_;  // by file name; nullptr = couldn't load
    SDL_Texture* canvas_ = nullptr;  // layout-sized target, then scaled to the window
    int canvasW_ = 0, canvasH_ = 0;
    SDL_Texture* stickLayer_ = nullptr;  // a stick drawn at scale 1, for a scaled stick
    int half_ = 0;                       // half the size of the shape textures and stickLayer_
    const idisp::Layout* editing_ = nullptr;
    TextRenderer text_;
    bool dirty_ = true;
};
