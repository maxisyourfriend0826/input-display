// Minimal text drawing for the overlay (coordinates and the rule label),
// using stb_truetype. Glyphs are drawn without anti-aliasing, like v4.43's
// TTF_RenderText_Solid.
#pragma once
#include <SDL3/SDL.h>
#include <stb_truetype.h>
#include <string>

class TextRenderer {
public:
    bool load(SDL_Renderer* renderer, const std::string& ttfPath, float pixelHeight);
    void destroy();
    float width(const std::string& s) const;
    float lineHeight() const { return lineHeight_; }
    // Draw with the top-left at (x, y), scaled by (sx, sy).
    void draw(const std::string& s, float x, float y, float sx = 1, float sy = 1) const;
    explicit operator bool() const { return atlas_ != nullptr; }

private:
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* atlas_ = nullptr;
    stbtt_bakedchar chars_[96]{};
    float ascent_ = 0, lineHeight_ = 0;
    static constexpr int kAtlas = 256;
};
