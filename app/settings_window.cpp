// Settings window, styled as a console system menu (see .impeccable/surfaces/
// app-settings-window-cpp.md): pale ground, white rounded panels, navy for
// text and chosen states, orange only for M-speed.
#include "settings_window.hpp"
#include "ui_theme.hpp"

#include "idisp/game.hpp"
#include "image.hpp"

#include <imgui.h>
#include <imgui_internal.h>  // TempInputIsActive
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <tuple>
#include <type_traits>

using idisp::Settings;
using namespace ui;

namespace {

const unsigned kLcd = 0xB9C9A6, kLcdEdge = 0x8FA37C, kLcdInk = 0x2B3424, kLcdDim = 0x3D4A32;

const float kRowH = 38, kHeadH = 44, kPad = 14, kRadius = 12;

// ---- small drawing helpers -------------------------------------------------

// Down-pointing (open) or right-pointing (closed) chevron.
void chevron(ImVec2 c, bool open, unsigned color) {
    const float s = 4.5f;
    if (open) dl()->AddTriangleFilled(ImVec2(c.x - s, c.y - s * 0.5f), ImVec2(c.x + s, c.y - s * 0.5f), ImVec2(c.x, c.y + s * 0.6f), rgb(color));
    else dl()->AddTriangleFilled(ImVec2(c.x - s * 0.5f, c.y - s), ImVec2(c.x - s * 0.5f, c.y + s), ImVec2(c.x + s * 0.6f, c.y), rgb(color));
}

// ---- panels ------------------------------------------------------------------

struct Panel {
    ImVec2 p;
    float w = 0;
    bool open = false, hovered = false;
    float cx = 0, cw = 0;  // content x and width
} P;

bool panelBegin(const char* id, const char* title, const std::string& summary, bool& open) {
    ImDrawList* d = dl();
    d->ChannelsSplit(2);
    d->ChannelsSetCurrent(1);
    ImGui::PushID(id);
    P.p = ImGui::GetCursorScreenPos();
    P.w = ImGui::GetContentRegionAvail().x;
    P.cx = P.p.x + kPad;
    P.cw = P.w - kPad * 2;
    if (ImGui::InvisibleButton("##head", ImVec2(P.w, kHeadH))) open = !open;
    P.hovered = ImGui::IsItemHovered();
    handCursorIfHovered();
    focusRing(P.p, ImVec2(P.p.x + P.w, P.p.y + kHeadH), kRadius);
    P.open = open;
    const unsigned tc = open ? kPanel : kNavy, sc = open ? kOnNavyMuted : kMuted;
    text(F->head, ImVec2(P.cx, P.p.y + (kHeadH - F->head->FontSize) * 0.5f - 1), tc, title);
    const ImVec2 ss = textSize(F->small, summary.c_str());
    text(F->small, ImVec2(P.p.x + P.w - kPad - 16 - ss.x, P.p.y + (kHeadH - F->small->FontSize) * 0.5f), sc, summary.c_str());
    chevron(ImVec2(P.p.x + P.w - kPad - 4, P.p.y + kHeadH * 0.5f), open, tc);
    return open;
}

void panelEnd() {
    if (P.open) ImGui::Dummy(ImVec2(0, 8));
    const float endY = ImGui::GetCursorScreenPos().y;
    ImDrawList* d = dl();
    d->ChannelsSetCurrent(0);
    const ImVec2 a = P.p, b(P.p.x + P.w, endY);
    d->AddRectFilled(a, b, rgb(kPanel), kRadius);
    if (P.open) d->AddRectFilled(a, ImVec2(b.x, a.y + kHeadH), rgb(kNavy), kRadius, ImDrawFlags_RoundCornersTop);
    else if (P.hovered) d->AddRectFilled(a, b, rgb(0xF4F8FC), kRadius);
    d->ChannelsMerge();
    ImGui::PopID();
    ImGui::Dummy(ImVec2(0, 10));
}

// ---- rows: label on the left, control in the right column ---------------------

float rowLine(float y) {
    dl()->AddLine(ImVec2(P.p.x, y), ImVec2(P.p.x + P.w, y), rgb(kLine));
    return y;
}

// A hint after a label: keycaps draw as keys, anything else as plain muted text.
bool isKeycap(const char* k) { return k && k[0] == 'F' && k[1] >= '0' && k[1] <= '9'; }
const float kGapKey = 7, kCapPad = 4;
float hintWidth(const char* k) { return k ? kGapKey + textSize(F->small, k).x + (isKeycap(k) ? kCapPad * 2 : 0) : 0; }
void hint(float x, float y, float h, const char* k) {
    if (!k) return;
    x += kGapKey;
    const float ty = y + (h - F->small->FontSize) * 0.5f;
    if (isKeycap(k)) {
        const ImVec2 ks = textSize(F->small, k);
        dl()->AddRect(ImVec2(x, ty - 1), ImVec2(x + ks.x + kCapPad * 2, ty + F->small->FontSize + 2), rgb(kMuted), 4, 0, 1);
        x += kCapPad;
    }
    text(F->small, ImVec2(x, ty), kMuted, k);
}

void label(float y, float h, const char* name, const char* key, bool struck = false) {
    const unsigned c = struck ? kMuted : kNavy;
    const float ty = y + (h - F->body->FontSize) * 0.5f - 1;
    text(F->body, ImVec2(P.cx, ty), c, name);
    const ImVec2 ns = textSize(F->body, name);
    if (struck) dl()->AddLine(ImVec2(P.cx, ty + ns.y * 0.55f), ImVec2(P.cx + ns.x, ty + ns.y * 0.55f), rgb(kMuted), 1.5f);
    hint(P.cx + ns.x, y, h, key);
}

float rowStart(const char* name, const char* key = nullptr, bool struck = false, float h = kRowH) {
    const float y = rowLine(ImGui::GetCursorScreenPos().y);
    label(y, h, name, key, struck);
    return y;
}
void rowEnd(float y, float h = kRowH) {
    ImGui::SetCursorScreenPos(ImVec2(P.cx, y + h));
    ImGui::Dummy(ImVec2(0, 0));
}
float colW() { return std::min(260.0f, P.cw * 0.5f); }
float right() { return P.cx + P.cw; }

// Sub-group label inside a panel.
void sub(const char* name, const char* extra = nullptr) {
    const float y = rowLine(ImGui::GetCursorScreenPos().y);
    text(F->pill, ImVec2(P.cx, y + 12), kNavy, name);
    hint(P.cx + textSize(F->pill, name).x, y + 12, F->pill->FontSize, extra);
    ImGui::SetCursorScreenPos(ImVec2(P.cx, y + 12 + F->pill->FontSize + 8));
    ImGui::Dummy(ImVec2(0, 0));
}

bool rowToggle(const char* id, const char* name, const char* key, bool& v) {
    ImGui::PushID(id);
    const float y = rowStart(name, key);
    const bool c = toggleAt(ImVec2(right() - 42, y + (kRowH - 24) * 0.5f), v);
    rowEnd(y);
    ImGui::PopID();
    return c;
}

template <class T>
bool rowSlider(const char* id, const char* name, const char* key, T& v, T lo, T hi, const char* fmt, bool enabled = true) {
    ImGui::PushID(id);
    const float y = rowStart(name, key, !enabled);
    // The value sits in its own fixed column right of the track, so the grab never covers it.
    const float valueW = 58;
    ImGui::SetCursorScreenPos(ImVec2(right() - colW(), y + (kRowH - ImGui::GetFrameHeight()) * 0.5f));
    ImGui::SetNextItemWidth(colW() - valueW - 8);
    ImGui::BeginDisabled(!enabled);
    // Ctrl+click typing keeps its text visible; otherwise the slider draws no text.
    const bool typing = ImGui::TempInputIsActive(ImGui::GetID("##v"));
    if (!typing) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 0));
    bool c;
    if constexpr (std::is_same_v<T, int>) c = ImGui::SliderInt("##v", &v, lo, hi, fmt, ImGuiSliderFlags_AlwaysClamp);
    else c = ImGui::SliderFloat("##v", &v, lo, hi, fmt, ImGuiSliderFlags_AlwaysClamp);
    if (!typing) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal) && !ImGui::IsItemActive() && !typing) ImGui::SetTooltip("Ctrl+click to type a value");
    ImGui::EndDisabled();
    char value[32];
    std::snprintf(value, sizeof value, fmt, v);
    const ImVec2 vs = textSize(F->body, value);
    text(F->body, ImVec2(right() - vs.x, y + (kRowH - F->body->FontSize) * 0.5f - 1), enabled ? kNavy : kMuted, value);
    rowEnd(y);
    ImGui::PopID();
    return c;
}

// A color chip; click opens a picker.
bool swatch(const char* id, const char* name, idisp::Rgba& c, bool alpha, ImVec2 p) {
    ImGui::PushID(id);
    const ImVec2 sz(24, 24), b(p.x + sz.x, p.y + sz.y);
    ImGui::SetCursorScreenPos(p);
    if (ImGui::InvisibleButton("##s", sz)) ImGui::OpenPopup("pick");
    handCursorIfHovered();
    if (ImGui::IsItemHovered() || (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible)) ImGui::SetTooltip("%s", name);
    dl()->AddRectFilled(p, b, IM_COL32(c.r, c.g, c.b, 255), 7);
    dl()->AddRect(p, b, ImGui::IsItemHovered() ? rgb(kNavy) : IM_COL32(29, 53, 87, 60), 7, 0, ImGui::IsItemHovered() ? 2.0f : 1.5f);
    focusRing(p, b, 7);
    bool changed = false;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
    if (ImGui::BeginPopup("pick")) {
        ImGui::PushFont(F->pill);
        ImGui::TextUnformatted(name);
        ImGui::PopFont();
        float v[4] = { c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f };
        ImGui::SetNextItemWidth(220);
        const ImGuiColorEditFlags fl = ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_DisplayHex |
                                       (alpha ? ImGuiColorEditFlags_AlphaBar : ImGuiColorEditFlags_NoAlpha);
        if (alpha ? ImGui::ColorPicker4("##p", v, fl) : ImGui::ColorPicker3("##p", v, fl)) {
            c = { (int)(v[0] * 255 + 0.5f), (int)(v[1] * 255 + 0.5f), (int)(v[2] * 255 + 0.5f), (int)(v[3] * 255 + 0.5f) };
            changed = true;
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopID();
    return changed;
}

struct Chip {
    const char* id;
    const char* name;
    idisp::Rgba* c;
    bool alpha = false;
};
bool rowSwatches(const char* name, std::initializer_list<Chip> chips) {
    const float y = rowStart(name);
    float x = right() - chips.size() * 24 - (chips.size() - 1) * 8;
    bool changed = false;
    for (const Chip& ch : chips) {
        changed |= swatch(ch.id, ch.name, *ch.c, ch.alpha, ImVec2(x, y + (kRowH - 24) * 0.5f));
        x += 32;
    }
    rowEnd(y);
    return changed;
}

// Segmented pills. Placed in the right column, or under the label if it doesn't fit.
bool rowSegmented(const char* id, const char* name, const char* key, const std::vector<std::string>& opts, int& idx) {
    ImGui::PushID(id);
    const float padX = 11, h = 28, inset = 3;
    float total = inset * 2;
    for (const auto& o : opts) total += textSize(F->small, o.c_str()).x + padX * 2;
    const ImVec2 ls = textSize(F->body, name);
    const float keyW = hintWidth(key);
    const bool stacked = total > P.cw - ls.x - keyW - 16;
    const float rowH = stacked ? kRowH + h + 6 : kRowH;
    const float y = rowLine(ImGui::GetCursorScreenPos().y);
    label(y, kRowH, name, key);
    ImVec2 p = stacked ? ImVec2(P.cx, y + kRowH - 4) : ImVec2(right() - total, y + (kRowH - h) * 0.5f);
    dl()->AddRectFilled(p, ImVec2(p.x + total, p.y + h), rgb(kTrack), h * 0.5f);
    bool changed = false;
    float x = p.x + inset;
    for (int i = 0; i < (int)opts.size(); ++i) {
        const float w = textSize(F->small, opts[i].c_str()).x + padX * 2;
        const ImVec2 a(x, p.y + inset), b(x + w, p.y + h - inset);
        ImGui::PushID(i);
        ImGui::SetCursorScreenPos(a);
        if (ImGui::InvisibleButton("##o", ImVec2(w, h - inset * 2)) && idx != i) {
            idx = i;
            changed = true;
        }
        handCursorIfHovered();
        const bool on = idx == i;
        if (on) dl()->AddRectFilled(a, b, rgb(kNavy), h * 0.5f);
        else if (ImGui::IsItemHovered()) dl()->AddRectFilled(a, b, rgb(kHover), h * 0.5f);
        text(F->small, ImVec2(x + padX, p.y + (h - F->small->FontSize) * 0.5f - 1), on ? kPanel : kNavy, opts[i].c_str());
        focusRing(a, b, h * 0.5f);
        ImGui::PopID();
        x += w;
    }
    rowEnd(y, rowH);
    ImGui::PopID();
    return changed;
}

// A row that shows or hides the rows under it.
bool disclosure(const char* id, const char* name, bool& open) {
    ImGui::PushID(id);
    const float y = rowLine(ImGui::GetCursorScreenPos().y);
    ImGui::SetCursorScreenPos(ImVec2(P.p.x, y));
    if (ImGui::InvisibleButton("##d", ImVec2(P.w, kRowH))) open = !open;
    handCursorIfHovered();
    if (ImGui::IsItemHovered()) dl()->AddRectFilled(ImVec2(P.p.x, y + 1), ImVec2(P.p.x + P.w, y + kRowH), rgb(kHover));
    focusRing(ImVec2(P.p.x + 4, y + 2), ImVec2(P.p.x + P.w - 4, y + kRowH - 2), 8);
    chevron(ImVec2(P.cx + 5, y + kRowH * 0.5f), open, kNavy);
    text(F->body, ImVec2(P.cx + 18, y + (kRowH - F->body->FontSize) * 0.5f - 1), kNavy, name);
    rowEnd(y);
    ImGui::PopID();
    return open;
}

bool rowButton(const char* id, const char* name, const char* button, bool primary = false) {
    ImGui::PushID(id);
    const float y = rowStart(name);
    const bool c = pillButton("##b", button, ImVec2(right() - pillWidth(button), y + (kRowH - 32) * 0.5f), primary);
    rowEnd(y);
    ImGui::PopID();
    return c;
}

void rowValue(const char* name, const std::string& value, bool strong = false) {
    const float y = rowStart(name);
    ImFont* f = strong ? F->pill : F->body;
    const ImVec2 vs = textSize(f, value.c_str());
    text(f, ImVec2(right() - vs.x, y + (kRowH - f->FontSize) * 0.5f - 1), kNavy, value.c_str());
    rowEnd(y);
}

// Wrapped muted note inside a panel.
void note(const std::string& s) {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    dl()->AddText(F->small, F->small->FontSize, ImVec2(P.cx, p.y + 2), rgb(kMuted), s.c_str(), nullptr, P.cw);
    const ImVec2 sz = F->small->CalcTextSizeA(F->small->FontSize, FLT_MAX, P.cw, s.c_str());
    ImGui::SetCursorScreenPos(ImVec2(P.cx, p.y + sz.y + 10));
    ImGui::Dummy(ImVec2(0, 0));
}

// Whole-cell picker grid. Returns the clicked index, or -1.
template <class Draw>
int cellGrid(const char* id, int count, int selected, float minW, float h, Draw draw) {
    ImGui::PushID(id);
    const float gap = 8;
    const int cols = std::max(1, (int)((P.cw + gap) / (minW + gap)));
    const float w = (P.cw - gap * (cols - 1)) / cols;
    const float y0 = ImGui::GetCursorScreenPos().y;
    int clicked = -1;
    for (int i = 0; i < count; ++i) {
        const ImVec2 a(P.cx + (i % cols) * (w + gap), y0 + (i / cols) * (h + gap)), b(a.x + w, a.y + h);
        ImGui::PushID(i);
        ImGui::SetCursorScreenPos(a);
        if (ImGui::InvisibleButton("##c", ImVec2(w, h))) clicked = i;
        handCursorIfHovered();
        const bool on = i == selected;
        dl()->AddRectFilled(a, b, rgb(on ? kPanel : ImGui::IsItemHovered() ? kHover : kTrack), 10);
        draw(i, a, b, on);
        if (on) dl()->AddRect(ImVec2(a.x + 1.5f, a.y + 1.5f), ImVec2(b.x - 1.5f, b.y - 1.5f), rgb(kNavy), 9, 0, 3);
        focusRing(a, b, 10);
        ImGui::PopID();
    }
    const int rows = (count + cols - 1) / cols;
    ImGui::SetCursorScreenPos(ImVec2(P.cx, y0 + rows * (h + gap) - gap + 12));
    ImGui::Dummy(ImVec2(0, 0));
    ImGui::PopID();
    return clicked;
}

// Centered cell caption.
void caption(ImVec2 a, ImVec2 b, float y, const char* s, bool on) {
    ImFont* f = on ? F->pill : F->small;
    const ImVec2 ts = textSize(f, s);
    dl()->PushClipRect(a, b, true);
    text(f, ImVec2(a.x + std::max(6.0f, (b.x - a.x - ts.x) * 0.5f), y), on ? kNavy : kMuted, s);
    dl()->PopClipRect();
}

void image(SDL_Texture* t, ImVec2 a, ImVec2 b, ImVec2 uv0 = ImVec2(0, 0), ImVec2 uv1 = ImVec2(1, 1)) {
    if (t) dl()->AddImage((ImTextureID)(intptr_t)t, a, b, uv0, uv1);
}

std::string capitalized(std::string s) {
    if (!s.empty() && s[0] >= 'a' && s[0] <= 'z') s[0] = (char)(s[0] - 'a' + 'A');
    return s;
}

char buf[256];
const char* fmt(const char* f, auto... a) {
    std::snprintf(buf, sizeof buf, f, a...);
    return buf;
}

} // namespace

// ---- window lifetime -----------------------------------------------------------

bool SettingsWindow::open(const std::string& assetsDir) {
    if (window_) {
        SDL_RaiseWindow(window_);
        return true;
    }
    if (!SDL_CreateWindowAndRenderer("Gens Input Display settings", 560, 820, SDL_WINDOW_RESIZABLE, &window_, &renderer_)) {
        SDL_Log("Could not open settings window: %s", SDL_GetError());
        return false;
    }
    SDL_SetWindowMinimumSize(window_, 480, 520);
    assets_ = assetsDir;
    ctx_ = ui::createContext(window_, renderer_, assets_, fonts_);
    return true;
}

void SettingsWindow::close() {
    if (!window_) return;
    freeThumbs();
    ui::destroyContext(ctx_);
    fonts_ = {};
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
    renderer_ = nullptr;
    window_ = nullptr;
}

bool SettingsWindow::processEvent(const SDL_Event& e) {
    if (!window_ || SDL_GetWindowFromEvent(&e) != window_) return false;
    if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        close();
        return true;
    }
    ImGui::SetCurrentContext(ctx_);
    ImGui_ImplSDL3_ProcessEvent(&e);
    return true;
}

void SettingsWindow::freeThumbs() {
    for (auto& set : setThumbs_)
        for (SDL_Texture* t : set)
            if (t) SDL_DestroyTexture(t);
    setThumbs_.clear();
}

void SettingsWindow::loadThumbs(const SettingsUiInput& in) {
    const size_t sets = in.buttonSets ? in.buttonSets->size() : 0;
    if (setThumbs_.size() == sets) return;
    freeThumbs();
    auto load = [&](const std::string& path) {
        SDL_Texture* t = loadTexture(renderer_, path);
        if (t) SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);  // thumbnails are scaled down
        return t;
    };
    for (size_t i = 0; i < sets; ++i) {
        const std::string dir = assets_ + "sets/" + (*in.buttonSets)[i] + "/butt";
        setThumbs_.push_back({ load(dir + "Y.png"), load(dir + "X.png"), load(dir + "B.png"), load(dir + "A.png") });
    }
}

// ---- top tile: live stick and M-speed state --------------------------------------

void SettingsWindow::drawTop(const Settings& s, const SettingsUiInput& in) {
    const ImVec2 header = ImGui::GetCursorScreenPos();
    const float headerW = ImGui::GetContentRegionAvail().x;
    if (ImGui::InvisibleButton("##practice", ImVec2(headerW, kHeadH))) practiceOpen_ = !practiceOpen_;
    handCursorIfHovered();
    dl()->AddRectFilled(header, ImVec2(header.x + headerW, header.y + kHeadH), rgb(kPanel), kRadius);
    text(F->head, ImVec2(header.x + kPad, header.y + (kHeadH - F->head->FontSize) * .5f - 1), kNavy, "Practice");
    chevron(ImVec2(header.x + headerW - kPad - 4, header.y + kHeadH * .5f), practiceOpen_, kNavy);
    if (!practiceOpen_) {
        ImGui::Dummy(ImVec2(0, 10));
        return;
    }
    const ImVec2 o = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x, h = 164;
    ImDrawList* d = dl();
    d->AddRectFilled(o, ImVec2(o.x + w, o.y + h), rgb(kPanel));
    // The bottom edge is the one place besides the tile that lights orange.
    d->AddRectFilled(ImVec2(o.x, o.y + h - 3), ImVec2(o.x + w, o.y + h), rgb(in.ruleLit ? kOrange : kLine));

    // Memory-card LCD. Pad space, zoomed to the deadzone: the dashed square is
    // the per-axis deadzone, the lit cells are exactly where pad M-speed holds.
    const float tile = 136, gx = o.x + 18, gy = o.y + 13;
    d->AddRectFilled(ImVec2(gx, gy), ImVec2(gx + tile, gy + tile), rgb(kLcd), 14);
    d->AddRect(ImVec2(gx + 1.5f, gy + 1.5f), ImVec2(gx + tile - 1.5f, gy + tile - 1.5f), rgb(kLcdEdge), 13, 0, 3);
    const int N = 60;
    const float cell = 2, R = N * cell * 0.5f;
    const ImVec2 c(gx + tile * 0.5f, gy + tile * 0.5f);
    const float dz = s.gameDeadzone, rmin = std::min(s.gameRingMin, s.gameRingMax),
                rmax = std::max(s.gameRingMin, s.gameRingMax);
    if (dz != lcdD_ || rmin != lcdMin_ || rmax != lcdMax_) {
        lcdD_ = dz;
        lcdMin_ = rmin;
        lcdMax_ = rmax;
        lcdView_ = std::clamp((dz + rmax * (1 - dz)) * 1.6f, 0.05f, 1.0f);  // band sits well inside the tile
        lcdCells_.assign(N * N, 0);
        for (int j = 0; j < N; ++j)
            for (int i = 0; i < N; ++i) {
                const double px = ((i + 0.5) / N * 2 - 1) * lcdView_, py = ((j + 0.5) / N * 2 - 1) * lcdView_;
                const double dist = std::hypot(idisp::gameFromPad(px, dz), idisp::gameFromPad(py, dz));
                lcdCells_[j * N + i] = dist >= rmin && dist <= rmax;
            }
    }
    const ImU32 band = in.ruleLit ? rgb(kOrange) : rgb(kLcdDim, 120);
    for (int j = 0; j < N; ++j)
        for (int i = 0; i < N; ++i)
            if (lcdCells_[j * N + i]) {
                const ImVec2 a(c.x - R + i * cell, c.y - R + j * cell);
                d->AddRectFilled(a, ImVec2(a.x + cell, a.y + cell), band);
            }
    // Dashed deadzone square.
    const float q = dz / lcdView_ * R;
    auto dashed = [&](ImVec2 a, ImVec2 b) {
        const float len = std::hypot(b.x - a.x, b.y - a.y);
        for (float t = 0; t < len; t += 6) {
            const float t2 = std::min(t + 3, len);
            d->AddLine(ImVec2(a.x + (b.x - a.x) * t / len, a.y + (b.y - a.y) * t / len),
                       ImVec2(a.x + (b.x - a.x) * t2 / len, a.y + (b.y - a.y) * t2 / len), rgb(kLcdDim), 1);
        }
    };
    dashed(ImVec2(c.x - q, c.y - q), ImVec2(c.x + q, c.y - q));
    dashed(ImVec2(c.x + q, c.y - q), ImVec2(c.x + q, c.y + q));
    dashed(ImVec2(c.x + q, c.y + q), ImVec2(c.x - q, c.y + q));
    dashed(ImVec2(c.x - q, c.y + q), ImVec2(c.x - q, c.y - q));
    d->AddLine(ImVec2(c.x - 4, c.y), ImVec2(c.x + 5, c.y), rgb(kLcdDim));
    d->AddLine(ImVec2(c.x, c.y - 4), ImVec2(c.x, c.y + 5), rgb(kLcdDim));
    // Stick dot; hollow and pinned to the edge when the stick is past the view.
    const float lim = lcdView_ * 0.96f;
    const bool outside = std::fabs(in.lcdX) > lim || std::fabs(in.lcdY) > lim;
    const float sx = std::clamp(in.lcdX, -lim, lim), sy = std::clamp(in.lcdY, -lim, lim);
    const ImVec2 dot(c.x + sx / lcdView_ * R, c.y - sy / lcdView_ * R);
    if (outside) d->AddCircle(dot, 4, rgb(kLcdInk), 0, 2);
    else d->AddCircleFilled(dot, 4.5f, rgb(kLcdInk));

    // State text.
    const float tx = gx + tile + 18, tw = o.x + w - 16 - tx;
    float y = o.y + 14;
    text(F->title, ImVec2(tx, y), kNavy, "Gens Input Display");
    y += F->title->FontSize + 8;
    {
        const char* pill = in.ruleLit ? "M-speed" : "Not in M-speed";
        const ImVec2 ts = textSize(F->pill, pill), a(tx, y), b(tx + ts.x + 26, y + 26);
        if (in.ruleLit) d->AddRectFilled(a, b, rgb(kOrangeText), 13);
        else d->AddRect(a, b, rgb(kMuted), 13, 0, 1.5f);
        text(F->pill, ImVec2(tx + 13, y + (26 - ts.y) * 0.5f), in.ruleLit ? kPanel : kMuted, pill);
        y += 26 + 10;
    }
    std::string src = in.showingGame ? "Source: game value" : in.gameMissing ? "No supported games detected" : "Source: pad";
    if (in.showingGame && !in.probe.game.empty()) src += " (" + in.probe.game + ")";
    const std::string lines[] = {
        src,
        fmt("Game distance %.3f \xC2\xB7 ring %.3f\xE2\x80\x93%.3f", in.gameDistance, s.gameRingMin, s.gameRingMax),
        in.controllerConnected ? "Controller: " + in.controllerName : std::string("No controller connected"),
    };
    for (const auto& l : lines) {
        d->PushClipRect(ImVec2(tx, y), ImVec2(tx + tw, y + 20), true);
        text(F->small, ImVec2(tx, y), (&l == &lines[2] && !in.controllerConnected) ? kNavy : kMuted, l.c_str());
        d->PopClipRect();
        y += F->small->FontSize + 5;
    }
    ImGui::Dummy(ImVec2(w, h));
}

// ---- the frame -------------------------------------------------------------------

SettingsUiResult SettingsWindow::frame(Settings& s, idisp::Profile& /*profile*/, const SettingsUiInput& in) {
    SettingsUiResult res;
    if (!window_) return res;
    ImGui::SetCurrentContext(ctx_);
    F = &fonts_;
    loadThumbs(in);
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("##settings", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoScrollWithMouse);

    drawTop(s, in);
    {
        const ImVec2 o = ImGui::GetCursorScreenPos();
        const float w = ImGui::GetContentRegionAvail().x;
        dl()->AddRectFilled(o, ImVec2(o.x + w, o.y + 32), rgb(kNavy));
        text(F->pill, ImVec2(o.x + 14, o.y + 8), kPanel, "Settings");
        ImGui::Dummy(ImVec2(w, 40));
    }

    bool changed = false;
    const float footH = 54;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
    ImGui::BeginChild("##body", ImVec2(0, ImGui::GetContentRegionAvail().y - footH), ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();

    // ---- Display studio ----
    if (panelBegin("display", "Display studio", "overlay, skin, layout, window", openDisplay_)) {
        sub("Overlay");
        static const std::vector<std::string> shapes = { "Rays", "Rays + cross", "Octagon", "Plain", "Ring", "Clock", "Dot only", "Target" };
        if (rowSegmented("shape", "Shape", nullptr, shapes, s.shapeIndex)) changed = true;
        changed |= rowSwatches("Outer ring: fill, outline", { { "rc", "Outer ring fill", &s.ringColor },
                                                              { "ro", "Outer ring outline", &s.ringOutlineColor } });
        changed |= rowSwatches("Inner ring: fill, outline", { { "ic", "Inner ring fill", &s.secondRingColor },
                                                              { "io", "Inner ring outline", &s.secondRingOutlineColor } });
        changed |= rowSwatches("Cross, plus and fill", { { "cr", "Cross", &s.firstCrossColor, true },
                                                         { "pl", "Plus", &s.secondCrossColor },
                                                         { "fi", "Fill", &s.fill, true } });
        changed |= rowSlider("dot", "Stick dot size", nullptr, s.circleSize, 1, 20, "%d px");
        changed |= rowSwatches("Stick dot and line", { { "dc", "Stick dot", &s.joystickCircleColor },
                                                       { "lc", "Stick line", &s.stickColor } });
        changed |= rowToggle("clamp", "Keep the drawn stick inside the ring", nullptr, s.clampStickToRing);
        changed |= rowToggle("dotTop", "Draw stick dots above images", nullptr, s.stickDotOnTop);
        if (disclosure("sizes", "Ring sizes", openRingSizes_)) {
            changed |= rowSlider("orr", "Outer ring radius", nullptr, s.ringOuterRadius, 20, 140, "%d px");
            changed |= rowSlider("ort", "Outer ring thickness", nullptr, s.ringThickness, 5, 40, "%d px");
            changed |= rowSlider("irr", "Inner ring radius", nullptr, s.secondRingOuterRadius, 5, 120, "%d px");
            changed |= rowSlider("irt", "Inner ring thickness", nullptr, s.secondRingThickness, 2, 40, "%d px");
        }

        sub("M-speed light");
        changed |= rowSwatches("Glow colors", { { "g1", "Glow", &s.joystickGlowColor }, { "g2", "Second glow", &s.secondGlowColor } });
        changed |= rowSlider("hold", "Keep lit after a hit", nullptr, s.hitHoldMs, 0, 100, "%d ms");

        sub("Buttons");
        changed |= rowToggle("ghost", "Ghost buttons", nullptr, s.staticButtonOverlay);
        {
            int pct = (s.staticButtonOpacity * 100 + 127) / 255;  // stored 0-255, shown as a percentage
            if (rowSlider("ghosta", "Ghost opacity", nullptr, pct, 0, 100, "%d%%", s.staticButtonOverlay)) {
                s.staticButtonOpacity = (pct * 255 + 50) / 100;
                changed = true;
            }
        }

        sub("Button set");
        const int nSets = in.buttonSets ? (int)in.buttonSets->size() : 0;
        if (nSets == 0) note("No button sets found in assets/sets.");
        // Display order: numeric folder names by number, then the rest by name. The saved
        // index still refers to the folder list, so nothing needs migrating.
        std::vector<int> order(nSets);
        for (int i = 0; i < nSets; ++i) order[i] = i;
        auto sortKey = [&](int i) {
            const std::string& n = (*in.buttonSets)[i];
            char* end = nullptr;
            const long v = std::strtol(n.c_str(), &end, 10);
            const bool numeric = !n.empty() && *end == '\0';
            return std::make_tuple(numeric ? 0 : 1, numeric ? v : 0L, n);
        };
        std::sort(order.begin(), order.end(), [&](int x, int y) { return sortKey(x) < sortKey(y); });
        const int setSel = (int)(std::find(order.begin(), order.end(), s.buttonSetIndex) - order.begin());
        int pick = cellGrid("sets", nSets, setSel, 104, 78, [&](int k, ImVec2 a, ImVec2 b, bool on) {
            const int i = order[k];
            const float t = 20, cx = (a.x + b.x) * 0.5f, cy = a.y + 31;
            const auto& th = setThumbs_[i];
            image(th[0], ImVec2(cx - t * 0.5f, cy - t * 1.1f), ImVec2(cx + t * 0.5f, cy - t * 0.1f));      // Y
            image(th[1], ImVec2(cx - t * 1.1f, cy - t * 0.5f), ImVec2(cx - t * 0.1f, cy + t * 0.5f));      // X
            image(th[2], ImVec2(cx + t * 0.1f, cy - t * 0.5f), ImVec2(cx + t * 1.1f, cy + t * 0.5f));      // B
            image(th[3], ImVec2(cx - t * 0.5f, cy + t * 0.1f), ImVec2(cx + t * 0.5f, cy + t * 1.1f));      // A
            caption(a, b, b.y - 19, fmt("Set %s", (*in.buttonSets)[i].c_str()), on);
        });
        if (pick >= 0 && order[pick] != s.buttonSetIndex) {
            s.buttonSetIndex = order[pick];
            changed = res.assetsChanged = true;
        }

        sub("Layout");
        if (in.layouts && !in.layouts->layouts.empty()) {
            const std::vector<idisp::Layout>& ls = in.layouts->layouts;
            const int n = (int)ls.size();
            // Every layout, then a "+ Custom layout" cell that copies the current one.
            pick = cellGrid("layouts", n + 1, s.buttonLayoutIndex, 150, 96, [&](int i, ImVec2 a, ImVec2 b, bool on) {
                if (i == n) {
                    const ImVec2 c((a.x + b.x) * 0.5f, a.y + 36);
                    dl()->AddLine(ImVec2(c.x - 9, c.y), ImVec2(c.x + 9, c.y), rgb(kNavy), 2.5f);
                    dl()->AddLine(ImVec2(c.x, c.y - 9), ImVec2(c.x, c.y + 9), rgb(kNavy), 2.5f);
                    caption(a, b, b.y - 21, "Custom layout", false);
                    return;
                }
                const idisp::Layout& L = ls[i];
                const float sh = 58, sc = std::min(sh / L.height, (b.x - a.x - 24) / L.width), sw = L.width * sc, h = L.height * sc;
                const ImVec2 m((a.x + b.x - sw) * 0.5f, a.y + 9 + (sh - h) * 0.5f);
                const unsigned ink = on ? kNavy : kMuted;
                dl()->PushClipRect(m, ImVec2(m.x + sw, m.y + h), true);
                if (L.shown(idisp::kStick))
                    dl()->AddCircle(ImVec2(m.x + L.ringCenterX * sc, m.y + L.ringCenterY * sc), 128 * sc * L.stickScale, rgb(ink), 0, 1.5f);
                for (const auto& [name, r] : L.buttons)
                    if (L.shown(name))
                        dl()->AddRectFilled(ImVec2(m.x + r.x * sc, m.y + r.y * sc), ImVec2(m.x + (r.x + r.w) * sc, m.y + (r.y + r.h) * sc),
                                            rgb(ink, on ? 200 : 130), 1.5f);
                dl()->PopClipRect();
                dl()->AddRect(m, ImVec2(m.x + sw, m.y + h), rgb(ink, 90), 4);
                caption(a, b, b.y - 21, L.name.c_str(), on);
            });
            if (pick == n) {
                res.duplicateLayout = s.buttonLayoutIndex;
            } else if (pick >= 0 && pick != s.buttonLayoutIndex) {
                s.buttonLayoutIndex = pick;
                confirmDelete_ = -1;
                changed = true;
            }
            // What can be done with the chosen layout.
            const int cur = std::clamp(s.buttonLayoutIndex, 0, n - 1);
            const idisp::Layout& L = ls[cur];
            const float y = rowStart(L.preset ? "Preset layouts are read-only" : L.name.c_str());
            const float by = y + (kRowH - 32) * 0.5f;
            const bool idle = !in.editorOpen;
            if (L.preset) {
                const char* d = "Duplicate and edit";
                if (pillButton("##dup", d, ImVec2(right() - pillWidth(d), by), true, idle)) res.duplicateLayout = cur;
            } else {
                const char* del = confirmDelete_ == cur ? "Delete for good" : "Delete";
                float x = right() - pillWidth(del);
                if (pillButton("##del", del, ImVec2(x, by), false, idle)) {
                    if (confirmDelete_ == cur) {
                        res.deleteLayout = cur;
                        confirmDelete_ = -1;
                    } else {
                        confirmDelete_ = cur;
                    }
                }
                x -= 8 + pillWidth("Duplicate");
                if (pillButton("##dup", "Duplicate", ImVec2(x, by), false, idle)) res.duplicateLayout = cur;
                x -= 8 + pillWidth("Edit");
                if (pillButton("##edit", "Edit", ImVec2(x, by), true, idle)) res.editLayout = cur;
            }
            rowEnd(y);
            if (in.editorOpen) note("The layout editor is open. Save or cancel there first.");
            else if (confirmDelete_ == cur) note("Click \"Delete for good\" to remove this layout. This can't be undone.");
        } else {
            note("No layouts found in assets/layouts.json.");
        }

        sub("Overlay text");
        changed |= rowToggle("coords", "Show coordinates", nullptr, s.showCoordinates);
        changed |= rowToggle("note", "Show \"no supported games detected\" when the pad is drawn instead", nullptr, s.showSourceNote);

        sub("Window");
        {
            char size[64];
            std::snprintf(size, sizeof size, "Size %d \xC3\x97 %d \xC2\xB7 %d%%", in.overlayW, in.overlayH,
                          (int)std::lround(in.overlayZoom * 100));
            if (rowButton("size", size, "Reset to 100%")) res.resetOverlaySize = true;
            note("Drag the overlay window's edge to resize it.");
        }
        changed |= rowToggle("seethru", "Transparent Window", "applies after restart", s.transparentWindow);
        changed |= rowSwatches("Background", { { "bg", "Background (hidden in OBS capture)", &s.bgColor } });
    }
    panelEnd();

    // ---- Input ----
    const std::string inputSummary = std::string(s.showGameStick ? "game value" : "pad") + (s.windowsXInputOnly ? " \xC2\xB7 XInput" : "");
    if (panelBegin("input", "Input", inputSummary, openInput_)) {
        int source = s.showGameStick ? 1 : 0;
        if (rowSegmented("src", "Stick to draw", nullptr, { "Pad", "Game value" }, source)) {
            s.showGameStick = source == 1;
            changed = true;
        }
        if (in.gameMissing) note("The game isn't running or can't be read, so the pad is drawn instead.");
        changed |= rowToggle("dpad", "D-pad moves the stick", nullptr, s.dpadMovesStick);
        changed |= rowToggle("xinput", "Read Xbox pads through XInput", "applies after restart", s.windowsXInputOnly);
        rowValue("Controller", in.controllerConnected ? in.controllerName : "None connected");
    }
    panelEnd();

    // ---- M-speed ----
    const std::string mSummary = std::string(s.showRuleIndicator ? "light on" : "light off") + " \xC2\xB7 ring " +
                                 fmt("%.3f\xE2\x80\x93%.3f", s.gameRingMin, s.gameRingMax);
    if (panelBegin("mspeed", "M-speed", mSummary, openMSpeed_)) {
        changed |= rowToggle("light", "Light up the overlay", nullptr, s.showRuleIndicator);
        note(fmt("Lights when the game value is %.3f\xE2\x80\x93%.3f from centre, after the %.3f per-axis deadzone. "
                 "Glow colors are in Display studio.", s.gameRingMin, s.gameRingMax, s.gameDeadzone));
        if (disclosure("adv", "Advanced: deadzone and ring", openAdvanced_)) {
            changed |= rowSlider("gdz", "Per-axis deadzone (measured)", nullptr, s.gameDeadzone, 0.0f, 0.5f, "%.3f");
            changed |= rowSlider("rmin", "Ring: min distance", nullptr, s.gameRingMin, 0.0f, 0.5f, "%.3f");
            changed |= rowSlider("rmax", "Ring: max distance", nullptr, s.gameRingMax, 0.0f, 0.5f, "%.3f");
            note("Measured in the game's own values, after the deadzone. Defaults: 0.265, 0.030, 0.077.");
        }
    }
    panelEnd();

    // ---- Research ----
    const GameProbeInput& g = in.probe;
    const std::string rSummary = std::string(g.attached ? "reading the game" : "no game") + (g.logging ? " \xC2\xB7 logging" : "");
    if (panelBegin("research", "Research", rSummary, openResearch_)) {
        if (g.attached) {
            rowValue("Game", fmt("%s (pid %u)", g.game.c_str(), g.pid));
            rowValue("Version", g.exactBuild ? g.version : g.version + " (build not pinned)");
            {
                ImGui::PushID("build");
                const float y = rowStart("Build");
                const char* cp = "Copy";
                const std::string v = fmt("0x%08X, 0x%08X", g.timeDateStamp, g.sizeOfImage);
                const ImVec2 vs = textSize(F->body, v.c_str());
                const float bx = right() - pillWidth(cp);
                text(F->body, ImVec2(bx - 10 - vs.x, y + (kRowH - F->body->FontSize) * 0.5f - 1), kNavy, v.c_str());
                if (pillButton("##c", cp, ImVec2(bx, y + (kRowH - 32) * 0.5f), false)) {
                    char m[128];
                    std::snprintf(m, sizeof m, "\"match\": { \"timeDateStamp\": \"0x%08X\", \"sizeOfImage\": \"0x%08X\" }",
                                  g.timeDateStamp, g.sizeOfImage);
                    ImGui::SetClipboardText(m);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Copy the build match for the game definition");
                rowEnd(y);
                ImGui::PopID();
            }
        } else {
            rowValue("Game", "Not reading a game", true);
            if (!g.message.empty()) note(g.message);
        }
        changed |= rowToggle("dz", "Draw the game value with the deadzone added back", nullptr, s.gameApplyDeadzone);
        sub("Values", "stick units, stored /128");
        rowValue("Pad X", fmt("%.3f   raw %d", g.padX, g.padRawX));
        rowValue("Pad Y", fmt("%.3f   raw %d", g.padY, g.padRawY));
        for (const auto& [name, v] : g.values) rowValue(name.c_str(), fmt("%.3f   %.6f", v, v / 128.0));
        if (rowButton("log", "Pad and game values to CSV", g.logging ? "Stop logging" : "Start logging")) res.toggleLogging = true;
        if (rowButton("reload", "Game definitions", "Reload")) res.reloadGames = true;
        if (g.logging) note(fmt("Logging %llu rows to %s", g.logRows, g.logPath.c_str()));
        else if (!g.logPath.empty()) note(fmt("Last log: %s (%llu rows)", g.logPath.c_str(), g.logRows));
    }
    panelEnd();

    ImGui::EndChild();

    // ---- footer ----
    {
        const ImVec2 o = ImGui::GetCursorScreenPos();
        const float w = ImGui::GetContentRegionAvail().x;
        dl()->AddRectFilled(o, ImVec2(o.x + w, o.y + footH), rgb(kPanel));
        dl()->AddLine(o, ImVec2(o.x + w, o.y), rgb(kLine));
        const char* save = "Save settings";
        const float bx = o.x + w - 16 - pillWidth(save);
        const std::string msg = in.status.empty() ? "Changes show on the overlay as you make them." : in.status;
        dl()->PushClipRect(ImVec2(o.x, o.y), ImVec2(bx - 12, o.y + footH), true);
        text(F->small, ImVec2(o.x + 16, o.y + (footH - F->small->FontSize) * 0.5f - 1), kMuted, msg.c_str());
        dl()->PopClipRect();
        if (pillButton("##save", save, ImVec2(bx, o.y + (footH - 32) * 0.5f), true)) res.saveSettings = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Settings also save when you quit");
    }

    if (changed) {
        idisp::clampSettings(s);
        res.settingsChanged = true;
    }

    ImGui::End();
    ImGui::Render();
    SDL_SetRenderDrawColor(renderer_, 0xE9, 0xF0, 0xF7, 255);
    SDL_RenderClear(renderer_);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
    SDL_RenderPresent(renderer_);
    return res;
}
