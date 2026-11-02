#define STB_TRUETYPE_IMPLEMENTATION
#include "text.hpp"
#include "image.hpp"

#include <vector>

bool TextRenderer::load(SDL_Renderer* renderer, const std::string& ttfPath, float pixelHeight) {
    destroy();
    renderer_ = renderer;
    std::string ttf;
    if (!readFile(ttfPath, ttf)) {
        SDL_Log("Font not found: %s", ttfPath.c_str());
        return false;
    }
    const auto* data = reinterpret_cast<const unsigned char*>(ttf.data());
    std::vector<unsigned char> mono(kAtlas * kAtlas);
    if (stbtt_BakeFontBitmap(data, 0, pixelHeight, mono.data(), kAtlas, kAtlas, 32, 96, chars_) <= 0) {
        SDL_Log("Font atlas too small for %s", ttfPath.c_str());
        return false;
    }
    stbtt_fontinfo info;
    stbtt_InitFont(&info, data, stbtt_GetFontOffsetForIndex(data, 0));
    int asc = 0, desc = 0, gap = 0;
    stbtt_GetFontVMetrics(&info, &asc, &desc, &gap);
    float scale = stbtt_ScaleForPixelHeight(&info, pixelHeight);
    ascent_ = asc * scale;
    lineHeight_ = (asc - desc) * scale;

    // White glyphs with smooth edges. (A hard 50% cutoff, as v4.43 did with
    // Arial Rounded, breaks the thinner strokes of M PLUS Rounded apart.)
    std::vector<uint32_t> rgba(kAtlas * kAtlas);
    for (size_t i = 0; i < mono.size(); ++i) rgba[i] = ((uint32_t)mono[i] << 24) | 0x00FFFFFFu;
    atlas_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, kAtlas, kAtlas);
    if (!atlas_) return false;
    SDL_UpdateTexture(atlas_, nullptr, rgba.data(), kAtlas * 4);
    SDL_SetTextureBlendMode(atlas_, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(atlas_, SDL_SCALEMODE_NEAREST);
    return true;
}

void TextRenderer::destroy() {
    if (atlas_) SDL_DestroyTexture(atlas_);
    atlas_ = nullptr;
}

float TextRenderer::width(const std::string& s) const {
    float w = 0;
    for (unsigned char c : s)
        if (c >= 32 && c < 128) w += chars_[c - 32].xadvance;
    return w;
}

void TextRenderer::draw(const std::string& s, float x, float y, float sx, float sy) const {
    if (!atlas_) return;
    float penX = 0, penY = ascent_;
    for (unsigned char c : s) {
        if (c < 32 || c >= 128) continue;
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(chars_, kAtlas, kAtlas, c - 32, &penX, &penY, &q, 1);
        SDL_FRect src{ q.s0 * kAtlas, q.t0 * kAtlas, (q.s1 - q.s0) * kAtlas, (q.t1 - q.t0) * kAtlas };
        SDL_FRect dst{ x + q.x0 * sx, y + q.y0 * sy, (q.x1 - q.x0) * sx, (q.y1 - q.y0) * sy };
        SDL_RenderTexture(renderer_, atlas_, &src, &dst);
    }
}
