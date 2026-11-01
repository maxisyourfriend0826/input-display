#include "overlay.hpp"
#include "draw.hpp"
#include "image.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;
using idisp::Settings;

namespace {

constexpr int kFirstCrossThickness = 3;
constexpr int kSecondCrossThickness = 4;
constexpr int kSecondCrossLength = 12;
constexpr float kThirdCrossAngle = 65.0f;
constexpr float kPi = 3.14159265358979323846f;

// The "Target" shape: its own colors and proportions (fractions of the outer
// ring radius), measured from the community's reference image.
constexpr SDL_Color kTargetBlue{ 74, 140, 247, 255 }, kTargetDark{ 27, 74, 160, 255 };
constexpr SDL_Color kTargetBlack{ 0, 0, 0, 255 }, kTargetRed{ 240, 64, 32, 255 };
struct TargetGeo {
    float band, edgeIn, lightIn, lightOut, darkIn, darkOut, plusLen;
    int crossThick, plusThick;
};
TargetGeo targetGeo(int R) {
    TargetGeo g;
    g.band = R - std::max(2.0f, std::round(R * 0.028f));     // blue: band..R
    g.edgeIn = g.band - std::max(2.0f, std::round(R * 0.024f));  // black edge: edgeIn..band
    g.lightIn = R * 0.432f, g.lightOut = R * 0.670f;        // top and bottom arcs
    g.darkIn = R * 0.278f, g.darkOut = R * 0.481f;          // side arcs
    g.plusLen = std::max(4.0f, std::round(R * 0.122f));
    g.crossThick = std::max(1, (int)std::lround(R * 0.016f));
    g.plusThick = std::max(2, (int)std::lround(R * 0.038f));
    return g;
}
void drawTargetBand(SDL_Renderer* r, int cx, int cy, int R, SDL_Color blue, SDL_Color black) {
    const TargetGeo g = targetGeo(R);
    draw::annulusSector(r, cx, cy, g.band, (float)R, 0, 360, blue);
    draw::annulusSector(r, cx, cy, g.edgeIn, g.band, 0, 360, black);
}

SDL_Color col(const idisp::Rgba& c) { return SDL_Color{ (Uint8)c.r, (Uint8)c.g, (Uint8)c.b, (Uint8)c.a }; }
SDL_Color opaque(const idisp::Rgba& c) { return SDL_Color{ (Uint8)c.r, (Uint8)c.g, (Uint8)c.b, 255 }; }

// Sub-folders of `dir` that contain `requiredFile`, sorted by name. Sorting by
// name matches the order v4.43 got from Windows, so saved indexes still point
// at the same set.
std::vector<std::string> listSets(const std::string& dir, const char* requiredFile) {
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (e.is_directory() && fs::exists(e.path() / requiredFile)) out.push_back(e.path().filename().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

void destroy(SDL_Texture*& t) {
    if (t) SDL_DestroyTexture(t);
    t = nullptr;
}

void drawCentered(SDL_Renderer* r, SDL_Texture* t, int cx, int cy, float scale = 1.0f) {
    if (!t) return;
    float w = 0, h = 0;
    SDL_GetTextureSize(t, &w, &h);
    int sw = (int)(w * scale), sh = (int)(h * scale);
    SDL_FRect dst{ (float)(cx - sw / 2), (float)(cy - sh / 2), (float)sw, (float)sh };
    SDL_RenderTexture(r, t, nullptr, &dst);
}

SDL_FRect frect(const idisp::Rect& r) { return SDL_FRect{ (float)r.x, (float)r.y, (float)r.w, (float)r.h }; }

} // namespace

bool Overlay::init(SDL_Renderer* renderer, const std::string& assetsDir) {
    r_ = renderer;
    assets_ = assetsDir;
    buttonSets_ = listSets(assets_ + "sets", "buttA.png");
    SDL_Log("Found %d button sets", (int)buttonSets_.size());
    text_.load(r_, assets_ + "ARLRDBD.TTF", 17.0f);
    return true;
}

void Overlay::shutdown() {
    for (auto& t : buttonTex_) destroy(t);
    destroyShapes();
    destroy(stickLayer_);
    destroy(canvas_);
    for (auto& [_, t] : images_) destroy(t);
    images_.clear();
    text_.destroy();
}

void Overlay::loadButtonSet(int index) {
    for (auto& t : buttonTex_) destroy(t);
    if (buttonSets_.empty()) return;
    const std::string dir = assets_ + "sets/" + buttonSets_[std::clamp(index, 0, (int)buttonSets_.size() - 1)] + "/";
    for (size_t i = 0; i < idisp::kButtonNames.size(); ++i) {
        const std::string path = dir + "butt" + idisp::kButtonNames[i] + ".png";
        // Sets without art for a button (e.g. Select, L3, R3) just don't draw it.
        if (SDL_GetPathInfo(path.c_str(), nullptr)) buttonTex_[i] = loadTexture(r_, path);
    }
}

const idisp::Layout& Overlay::layout(const Settings& s) const {
    const int n = (int)layouts_.layouts.size();
    return layouts_.layouts[std::clamp(s.buttonLayoutIndex, 0, n - 1)];
}

SDL_Texture* Overlay::makeTarget(int w, int h) {
    SDL_Texture* t = SDL_CreateTexture(r_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
    SDL_SetRenderTarget(r_, t);
    SDL_SetRenderDrawColor(r_, 0, 0, 0, 0);
    SDL_RenderClear(r_);
    return t;
}

void Overlay::destroyShapes() {
    for (ShapeTex& t : shapes_) {
        for (SDL_Texture** p : { &t.base, &t.outer, &t.inner, &t.outerWhite, &t.innerWhite }) destroy(*p);
        t.built = false;
    }
}

SDL_Texture* Overlay::image(const std::string& file) {
    auto it = images_.find(file);
    if (it != images_.end()) return it->second;
    SDL_Texture* t = nullptr;
    if (idisp::safeImageFile(file) && !imagesDir_.empty()) {
        const std::string path = imagesDir_ + file;
        if (SDL_GetPathInfo(path.c_str(), nullptr)) t = loadTexture(r_, path);
        if (t) SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
    }
    images_[file] = t;  // remember a failure too, so a missing file isn't retried every frame
    return t;
}

void Overlay::buildRings(const Settings& s, int shape, ShapeTex& out) {
    const SDL_Color white{ 255, 255, 255, 255 };
    auto ringTex = [&](int radius, int thick, SDL_Color fill, SDL_Color outline) {
        int size = (radius + thick) * 2;
        SDL_Texture* t = makeTarget(size, size);
        draw::ring(r_, size / 2, size / 2, radius, thick, fill, outline);
        return t;
    };
    auto octTex = [&](int radius, int thick, SDL_Color c) {
        int size = (radius + thick + 4) * 2;
        SDL_Texture* t = makeTarget(size, size);
        draw::octagon(r_, size / 2, size / 2, radius, thick, c, col(s.ringOutlineColor));
        return t;
    };
    switch (shape) {
    case 0: case 1: case 4: case 5:
        out.outer = ringTex(s.ringOuterRadius, s.ringThickness, col(s.ringColor), col(s.ringOutlineColor));
        out.inner = ringTex(s.secondRingOuterRadius, s.secondRingThickness, col(s.secondRingColor), col(s.secondRingOutlineColor));
        out.outerWhite = ringTex(s.ringOuterRadius, s.ringThickness, white, white);
        out.innerWhite = ringTex(s.secondRingOuterRadius, s.secondRingThickness, white, white);
        break;
    case 2:
        out.outer = octTex(s.ringOuterRadius - 4, s.ringThickness - 4, col(s.ringColor));
        out.inner = octTex(s.secondRingOuterRadius - 4, s.secondRingThickness - 1, col(s.secondRingColor));
        out.outerWhite = octTex(s.ringOuterRadius - 4, s.ringThickness - 4, white);
        out.innerWhite = octTex(s.secondRingOuterRadius - 4, s.secondRingThickness - 1, white);
        break;
    case idisp::kShapeTarget: {
        // The outer band, so the M-speed glow pulses it like the other rings.
        const int size = (s.ringOuterRadius + 2) * 2;
        out.outer = makeTarget(size, size);
        drawTargetBand(r_, size / 2, size / 2, s.ringOuterRadius, kTargetBlue, kTargetBlack);
        out.outerWhite = makeTarget(size, size);
        drawTargetBand(r_, size / 2, size / 2, s.ringOuterRadius, white, white);
        break;
    }
    default:  // shape 3: filled circle only, no ring textures (as in v4.43); 6: dot only
        break;
    }
}

int Overlay::stickHalf(const Settings& s) const {
    // Room for the pulsing rings, the octagon's outline and the dot at full travel.
    const float R = (float)std::max(1, s.ringOuterRadius);
    const float rings = (s.ringOuterRadius + s.ringThickness + 4) * (R + s.pulseAmplitude) / R;
    const float dot = 128.0f + s.circleSize + 4;
    return (int)std::ceil(std::max(rings, dot)) + 2;
}

// A shape's textures, built the first time a stick uses it.
Overlay::ShapeTex& Overlay::shape(const Settings& s, int index) {
    index = std::clamp(index, 0, idisp::kShapeCount - 1);
    ShapeTex& t = shapes_[index];
    if (!t.built) {
        SDL_Texture* prev = SDL_GetRenderTarget(r_);
        buildBase(s, index, t);
        buildRings(s, index, t);
        t.built = true;
        SDL_SetRenderTarget(r_, prev);
        SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
    }
    return t;
}

void Overlay::buildBase(const Settings& s, int shape, ShapeTex& out) {
    if (shape == idisp::kShapeDotOnly) return;  // nothing under the dot
    const int size = half_ * 2;
    out.base = makeTarget(size, size);
    const int cx = half_, cy = half_;
    const int firstCrossLength = s.secondRingOuterRadius + 4;
    const SDL_Color fill = col(s.fill), cross = col(s.firstCrossColor), plus = col(s.secondCrossColor);

    auto eightRays = [&] {
        for (int i = 0; i < 8; i++) {
            float a = i * (kPi / 4.0f);
            draw::thickLineRect(r_, cx, cy, cx + (int)(firstCrossLength * std::cos(a)),
                                cy + (int)(firstCrossLength * std::sin(a)), kFirstCrossThickness, cross);
        }
    };

    switch (shape) {
    case 0: case 1:
        draw::filledCircle(r_, cx, cy, s.ringOuterRadius, fill);
        draw::annulusCross(r_, cx, cy, s.ringOuterRadius, s.ringThickness, kFirstCrossThickness, cross, kThirdCrossAngle);
        eightRays();
        draw::plusCross(r_, cx, cy, kSecondCrossLength, kSecondCrossThickness, plus);
        break;
    case 2:
        draw::octagon(r_, cx, cy, s.ringOuterRadius - 4, s.ringThickness - 4, col(s.ringColor), col(s.ringOutlineColor));
        eightRays();
        draw::plusCross(r_, cx, cy, kSecondCrossLength, kSecondCrossThickness, plus);
        draw::octagon(r_, cx, cy, s.secondRingOuterRadius - 4, s.secondRingThickness - 1, col(s.secondRingColor), col(s.ringOutlineColor));
        break;
    case 3:
        draw::filledCircle(r_, cx, cy, s.ringOuterRadius, fill);
        draw::circleOutline(r_, cx, cy, s.ringOuterRadius, col(s.ringOutlineColor));
        draw::plusCross(r_, cx, cy, kSecondCrossLength, kSecondCrossThickness, plus);
        break;
    case 4:
        draw::filledCircle(r_, cx, cy, s.ringOuterRadius, fill);
        draw::plusCross(r_, cx, cy, kSecondCrossLength, kSecondCrossThickness, plus);
        break;
    case 5: {
        draw::filledCircle(r_, cx, cy, s.ringOuterRadius, fill);
        draw::plusCross(r_, cx, cy, kSecondCrossLength, kSecondCrossThickness, plus);
        // Clock markers: 12 long ticks, 4 short ones between each.
        const int base = s.ringOuterRadius - s.ringThickness;
        auto tick = [&](float a, int length) {
            int sx = cx + (int)(base * std::cos(a)), sy = cy + (int)(base * std::sin(a));
            int ex = cx + (int)((base - length) * std::cos(a)), ey = cy + (int)((base - length) * std::sin(a));
            float ox = 2.0f * -std::sin(a), oy = 2.0f * std::cos(a);  // thickness 4
            int rx = (int)std::round(ox), ry = (int)std::round(oy);
            SDL_Point q[4] = { { sx + rx, sy + ry }, { sx - rx, sy - ry }, { ex - rx, ey - ry }, { ex + rx, ey + ry } };
            draw::filledQuad(r_, q, col(s.ringColor));
        };
        for (int i = 0; i < 12; i++) {
            float a = i * (kPi / 6.0f);
            tick(a, 20);
            for (int j = 1; j <= 4; j++) tick(a + j * (kPi / 30.0f), 4);
        }
        break;
    }
    case idisp::kShapeTarget: {
        const int R = s.ringOuterRadius;
        const TargetGeo g = targetGeo(R);
        draw::filledCircle(r_, cx, cy, (int)g.edgeIn, fill);
        drawTargetBand(r_, cx, cy, R, kTargetBlue, kTargetBlack);
        draw::annulusSector(r_, cx, cy, g.lightIn, g.lightOut, 45, 135, kTargetBlue);
        draw::annulusSector(r_, cx, cy, g.lightIn, g.lightOut, 225, 315, kTargetBlue);
        draw::annulusSector(r_, cx, cy, g.darkIn, g.darkOut, -45, 45, kTargetDark);
        draw::annulusSector(r_, cx, cy, g.darkIn, g.darkOut, 135, 225, kTargetDark);
        const int e = (int)g.edgeIn;
        draw::thickLineRect(r_, cx - e, cy, cx + e, cy, g.crossThick, kTargetBlack);
        draw::thickLineRect(r_, cx, cy - e, cx, cy + e, g.crossThick, kTargetBlack);
        const int pl = (int)g.plusLen;  // no outline, unlike the other shapes' plus
        draw::thickLineRect(r_, cx - pl, cy, cx + pl, cy, g.plusThick, kTargetRed);
        draw::thickLineRect(r_, cx, cy - pl, cx, cy + pl, g.plusThick, kTargetRed);
        break;
    }
    default:
        break;
    }
}

const idisp::Layout& Overlay::current(const Settings& s) const {
    return editing_ ? *editing_ : layout(s);
}

static SDL_FRect fitRect(int canvasW, int canvasH, int outW, int outH) {
    if (canvasW <= 0 || canvasH <= 0 || outW <= 0 || outH <= 0) return SDL_FRect{ 0, 0, 0, 0 };
    const float scale = std::min((float)outW / canvasW, (float)outH / canvasH);
    const float w = std::round(canvasW * scale), h = std::round(canvasH * scale);
    return SDL_FRect{ std::floor((outW - w) / 2), std::floor((outH - h) / 2), w, h };
}

void Overlay::render(const Settings& s, const FrameInput& in) {
    if (layouts_.layouts.empty()) return;
    if (dirty_) {
        destroyShapes();
        destroy(stickLayer_);
        half_ = stickHalf(s);
        stickLayer_ = SDL_CreateTexture(r_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, half_ * 2, half_ * 2);
        if (stickLayer_) {
            // Drawn into from a transparent clear, so its colors are premultiplied.
            SDL_SetTextureBlendMode(stickLayer_, SDL_BLENDMODE_BLEND_PREMULTIPLIED);
            SDL_SetTextureScaleMode(stickLayer_, SDL_SCALEMODE_LINEAR);
        }
        // Image files can change on disk (added in the editor): reload them.
        for (auto& [_, t] : images_) destroy(t);
        images_.clear();
        dirty_ = false;
    }
    const idisp::Layout& L = current(s);
    if (canvas_ && (canvasW_ != L.width || canvasH_ != L.height)) destroy(canvas_);
    if (!canvas_) {
        canvas_ = SDL_CreateTexture(r_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, L.width, L.height);
        if (!canvas_) return;
        canvasW_ = L.width;
        canvasH_ = L.height;
    }

    // Background: the chosen color for the person playing, but alpha 0 so OBS
    // ("Allow Transparency") captures it as transparent. The letterbox space
    // around the canvas gets the same clear. (SDL3 needs the draw color set
    // again after switching targets.) A transparent window is composited as
    // premultiplied, so a colored alpha-0 pixel still shows: clear to black.
    const Uint8 bgR = s.transparentWindow ? 0 : (Uint8)s.bgColor.r;
    const Uint8 bgG = s.transparentWindow ? 0 : (Uint8)s.bgColor.g;
    const Uint8 bgB = s.transparentWindow ? 0 : (Uint8)s.bgColor.b;
    SDL_SetRenderTarget(r_, canvas_);
    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r_, bgR, bgG, bgB, 0);
    SDL_RenderClear(r_);
    drawCanvas(s, L, in);
    SDL_SetRenderTarget(r_, nullptr);
    SDL_SetRenderDrawColor(r_, bgR, bgG, bgB, 0);
    SDL_RenderClear(r_);

    // Scale the canvas uniformly to the window. The canvas already holds the
    // final pixels (blended over the background), so it is copied, not blended
    // again; at 100% the window gets exactly the pixels v4.43 drew. Whole-number
    // zooms stay crisp; in-between sizes are smoothed.
    int outW = 0, outH = 0;
    SDL_GetCurrentRenderOutputSize(r_, &outW, &outH);
    const SDL_FRect dst = fitRect(L.width, L.height, outW, outH);
    const float zoom = dst.w / L.width;
    SDL_SetTextureBlendMode(canvas_, SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(canvas_, zoom == std::floor(zoom) && dst.h == L.height * zoom ? SDL_SCALEMODE_NEAREST
                                                                                         : SDL_SCALEMODE_LINEAR);
    SDL_RenderTexture(r_, canvas_, nullptr, &dst);
}

void Overlay::drawStickBase(const Settings& s, int shapeIndex, int cx, int cy, bool lit) {
    ShapeTex& sh = shape(s, shapeIndex);
    const SDL_FRect st{ (float)(cx - half_), (float)(cy - half_), (float)(half_ * 2), (float)(half_ * 2) };
    if (sh.base) SDL_RenderTexture(r_, sh.base, nullptr, &st);
    if (lit) {
        const SDL_Color g1 = opaque(s.joystickGlowColor), g2 = opaque(s.secondGlowColor);
        // double: a float loses precision after a few hours of uptime and the pulse stutters.
        float t = (float)std::fmod((double)SDL_GetTicks() * s.pulseRate, 2.0);
        float interp = t < 1.0f ? t : 2.0f - t;
        int extra = (int)(s.pulseAmplitude * interp);
        SDL_Color blend{ (Uint8)(g1.r * (1 - interp) + g2.r * interp), (Uint8)(g1.g * (1 - interp) + g2.g * interp),
                         (Uint8)(g1.b * (1 - interp) + g2.b * interp), 255 };
        auto pulse = [&](SDL_Texture* base, SDL_Texture* white, int radius) {
            if (!base || !white || radius <= 0) return;
            float scale = (float)(radius + extra) / (float)radius;
            SDL_SetTextureColorMod(base, g1.r, g1.g, g1.b);
            drawCentered(r_, base, cx, cy);
            SDL_SetTextureColorMod(white, 255, 255, 255);
            drawCentered(r_, white, cx, cy, scale);
            SDL_SetTextureColorMod(white, blend.r, blend.g, blend.b);
            drawCentered(r_, white, cx, cy, scale);
        };
        pulse(sh.outer, sh.outerWhite, s.ringOuterRadius);
        pulse(sh.inner, sh.innerWhite, s.secondRingOuterRadius);
    } else {
        for (SDL_Texture* t : { sh.outer, sh.inner }) {
            if (!t) continue;
            SDL_SetTextureColorMod(t, 255, 255, 255);
            drawCentered(r_, t, cx, cy);
        }
        if (shapeIndex == 1)
            draw::annulusCross(r_, cx, cy, s.ringOuterRadius, s.ringThickness, kFirstCrossThickness,
                               col(s.firstCrossColor), kThirdCrossAngle);
    }
}

void Overlay::drawStickTop(const Settings& s, int cx, int cy, int dx, int dy, bool lit) {
    int capX = dx, capY = dy;
    if (s.clampStickToRing) {
        float rad = std::fmin(std::sqrt((float)(capX * capX + capY * capY)), 128.0f);
        float ang = std::atan2((float)capY, (float)capX);
        capX = (int)(rad * std::cos(ang));
        capY = (int)(rad * std::sin(ang));
    }
    const int dotX = cx + capX, dotY = cy - capY;
    draw::thickLineRect(r_, cx, cy, dotX, dotY, 3, opaque(s.stickColor));
    draw::filledCircle(r_, dotX, dotY, s.circleSize, lit ? opaque(s.joystickGlowColor) : opaque(s.joystickCircleColor));
    draw::circleOutline(r_, dotX, dotY, s.circleSize, SDL_Color{ 0, 0, 0, 255 });
}

// Draws one stick pass at a position and scale. At scale 1 it draws straight
// onto the canvas (the exact v4.43 pixels); otherwise it draws into the stick
// layer and scales that onto the canvas.
template <class Pass>
void Overlay::stickPass(int cx, int cy, float scale, Pass pass) {
    if (scale == 1.0f || !stickLayer_) {
        pass(cx, cy);
        return;
    }
    SDL_SetRenderTarget(r_, stickLayer_);
    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r_, 0, 0, 0, 0);
    SDL_RenderClear(r_);
    pass(half_, half_);
    SDL_SetRenderTarget(r_, canvas_);
    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
    const float size = half_ * 2 * scale;
    const SDL_FRect d{ cx - half_ * scale, cy - half_ * scale, size, size };
    SDL_RenderTexture(r_, stickLayer_, nullptr, &d);
}

void Overlay::drawCanvas(const Settings& s, const idisp::Layout& L, const FrameInput& in) {
    const bool lit = in.ruleLit && s.showRuleIndicator;

    // The two sticks: place, scale, live input and options.
    struct StickDraw {
        const char* id;
        int cx, cy;
        float scale;
        int dx, dy;
        bool lit;
        int shape;
    };
    auto stickOf = [&](int i) {
        const idisp::StickOptions& o = L.sticks[i];
        const int sh = o.shape >= 0 ? o.shape : s.shapeIndex;
        if (i == 0) return StickDraw{ idisp::kStick, L.ringCenterX, L.ringCenterY, L.stickScale, in.drawX, in.drawY, lit && o.light, sh };
        return StickDraw{ idisp::kStickR, L.rightX, L.rightY, L.rightScale, in.drawXR, in.drawYR,
                          in.ruleLitR && s.showRuleIndicator && o.light, sh };
    };

    for (const std::string& id : L.drawOrder()) {
        if (!L.shown(id)) continue;
        if (id == idisp::kStick || id == idisp::kStickR) {
            const StickDraw d = stickOf(id == idisp::kStick ? 0 : 1);
            stickPass(d.cx, d.cy, d.scale, [&](int cx, int cy) {
                drawStickBase(s, d.shape, cx, cy, d.lit);
                if (!s.stickDotOnTop) drawStickTop(s, cx, cy, d.dx, d.dy, d.lit);
            });
        } else if (id == idisp::kCoords) {
            if (!text_) continue;
            const idisp::Rect cr = L.coordsRect();
            const float k = (float)cr.w / idisp::kCoordsW;
            if (s.showCoordinates) {
                char x[48], y[48];
                std::snprintf(x, sizeof x, "X:  %.2f", in.unitsX);
                std::snprintf(y, sizeof y, "    Y:   %.2f", in.unitsY);
                text_.draw(x, (float)cr.x, cr.y + 2 * k, k, k);
                text_.draw(y, cr.x + 84 * k, cr.y + 2 * k, k, k);
            }
            if (s.showRuleIndicator) {
                const std::string label = "  " + in.ruleLabel + "  ";
                const float w = text_.width(label), h = text_.lineHeight();
                if (w > 0 && h > 0) text_.draw(label, cr.x + 196 * k, (float)cr.y, 34.0f * k / w, 20.0f * k / h);
            }
        } else if (const idisp::LayoutImage* im = L.image(id)) {
            if (SDL_Texture* t = image(im->file)) {
                const SDL_FRect d = frect(im->rect);
                SDL_RenderTexture(r_, t, nullptr, &d);
            }
        } else {
            // A button: full when pressed, faint when ghost buttons are on.
            size_t i = 0;
            while (i < idisp::kButtonCount && id != idisp::kButtonNames[i]) ++i;
            auto it = L.buttons.find(id);
            if (i == idisp::kButtonCount || it == L.buttons.end() || !buttonTex_[i]) continue;
            const SDL_FRect d = frect(it->second);
            if (in.buttons[i]) {
                SDL_RenderTexture(r_, buttonTex_[i], nullptr, &d);
            } else if (s.staticButtonOverlay) {
                SDL_SetTextureAlphaMod(buttonTex_[i], (Uint8)s.staticButtonOpacity);
                SDL_RenderTexture(r_, buttonTex_[i], nullptr, &d);
                SDL_SetTextureAlphaMod(buttonTex_[i], 255);
            }
        }
    }

    // Dots and lines above every layer, so nothing hides the input.
    if (s.stickDotOnTop)
        for (int i = 0; i < 2; ++i) {
            const StickDraw d = stickOf(i);
            if (!L.shown(d.id)) continue;
            stickPass(d.cx, d.cy, d.scale, [&](int cx, int cy) { drawStickTop(s, cx, cy, d.dx, d.dy, d.lit); });
        }

    if (!in.sourceNote.empty() && text_) text_.draw(in.sourceNote, 4, 2, 0.8f, 0.8f);
}
