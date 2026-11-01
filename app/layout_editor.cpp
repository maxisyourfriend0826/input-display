// Layout editor window. Look: the settings window's sibling (ui_theme), with
// the canvas as the one dark object. See .impeccable/surfaces/app-layout-editor-cpp.md.
#include "layout_editor.hpp"
#include "image.hpp"

#include <imgui.h>
#include <imgui_internal.h>  // SetKeyOwner: arrows nudge, Tab still moves focus
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <optional>
#include <mutex>

namespace fs = std::filesystem;

using idisp::Layout;
using idisp::Rect;
using namespace ui;

namespace {

const unsigned kStage = 0xFFFFFF, kDots = 0xCFDBE8, kCanvasEdge = 0x9FB2C8;
const unsigned kGhost = 0x8AA0CC, kGhostText = 0xC9D6F0, kGuide = 0x7FC4FF;
const float kTopH = 60, kRailW = 250, kGap = 20, kRowH = 26;

struct Elem {
    std::string id;
    std::string label;  // in the list and on the card
    std::string chip;   // short text in the list's icon chip ("" = drawn)
};
struct Group {
    std::string name;
    std::vector<Elem> elems;
};
const std::vector<Group>& groups() {
    static const std::vector<Group> g = {
        { "Sticks", { { idisp::kStick, "Left stick", "" }, { idisp::kStickR, "Right stick", "" } } },
        // Face buttons go by position: sets print different letters on them.
        { "Face buttons", { { "A", "Bottom", "A" }, { "B", "Right", "B" }, { "X", "Left", "X" }, { "Y", "Top", "Y" } } },
        { "Shoulders and triggers",
          { { "L", "Left shoulder", "L" }, { "R", "Right shoulder", "R" }, { "LT", "Left trigger", "LT" }, { "RT", "Right trigger", "RT" } } },
        { "D-pad", { { "Up", "Up", "" }, { "Down", "Down", "" }, { "Left", "Left", "" }, { "Right", "Right", "" } } },
        { "Extras",
          { { "S", "Start", "St" }, { "Select", "Select", "Se" }, { "L3", "Left stick click", "L3" }, { "R3", "Right stick click", "R3" },
            { idisp::kCoords, "Coordinates", "xy" } } },
    };
    return g;
}
const Elem* findElem(const std::string& id) {
    for (const auto& g : groups())
        for (const auto& e : g.elems)
            if (id == e.id) return &e;
    return nullptr;
}
// An image's name in the list: its file name without the extension.
std::string imageLabel(const idisp::LayoutImage& im) {
    const size_t dot = im.file.rfind('.');
    return dot == std::string::npos ? im.file : im.file.substr(0, dot);
}
// The fixed groups plus the layout's images (only when it has some).
std::vector<Group> groupsFor(const Layout& L) {
    std::vector<Group> g = groups();
    if (!L.images.empty()) {
        Group im{ "Images", {} };
        for (const auto& i : L.images) im.elems.push_back({ i.id, imageLabel(i), "Im" });
        g.push_back(im);
    }
    return g;
}
std::vector<std::string> allIds(const Layout& L) {
    std::vector<std::string> out;
    for (const auto& g : groupsFor(L))
        for (const auto& e : g.elems) out.push_back(e.id);
    return out;
}

int buttonIndex(const std::string& id) {
    for (size_t i = 0; i < idisp::kButtonCount; ++i)
        if (id == idisp::kButtonNames[i]) return (int)i;
    return -1;
}
bool isButton(const std::string& id) { return buttonIndex(id) >= 0; }
bool isStick(const std::string& id) { return id == idisp::kStick || id == idisp::kStickR; }

// An element exists when it has a box: the sticks and text always do;
// a button only when the layout places it; an image while it's in the layout.
bool exists(const Layout& L, const std::string& id) {
    if (isButton(id)) return L.buttons.count(id) > 0;
    if (idisp::isImageId(id)) return L.image(id) != nullptr;
    return true;
}
bool drawn(const Layout& L, const std::string& id) { return exists(L, id) && L.shown(id); }

Rect rectOf(const Layout& L, const std::string& id, int R) {
    if (id == idisp::kStick) {
        const int d = (int)std::lround(2 * R * L.stickScale);
        return Rect{ L.ringCenterX - d / 2, L.ringCenterY - d / 2, d, d };
    }
    if (id == idisp::kStickR) {
        const int d = (int)std::lround(2 * R * L.rightScale);
        return Rect{ L.rightX - d / 2, L.rightY - d / 2, d, d };
    }
    if (id == idisp::kCoords) return L.coordsRect();
    if (const idisp::LayoutImage* im = L.image(id)) return im->rect;
    auto it = L.buttons.find(id);
    return it != L.buttons.end() ? it->second : Rect{ L.width / 2 - 16, L.height / 2 - 16, 32, 32 };
}

// Width/height ratio an element keeps when resized (0 = free). An image keeps
// the proportions it has.
float lockedRatio(const Layout& L, const std::string& id) {
    if (isStick(id)) return 1.0f;
    if (id == idisp::kCoords) return (float)idisp::kCoordsW / idisp::kCoordsH;
    if (const idisp::LayoutImage* im = L.image(id)) return im->rect.h > 0 ? (float)im->rect.w / im->rect.h : 0;
    return 0;
}

void setRect(Layout& L, const std::string& id, Rect r, int R) {
    r.w = std::max(r.w, 4);
    r.h = std::max(r.h, 4);
    if (isStick(id)) {
        const float scale = std::clamp(r.w / (2.0f * R), 0.1f, 8.0f);
        const int cx = r.x + r.w / 2, cy = r.y + r.w / 2;
        if (id == idisp::kStick) L.stickScale = scale, L.ringCenterX = cx, L.ringCenterY = cy;
        else L.rightScale = scale, L.rightX = cx, L.rightY = cy;
    } else if (id == idisp::kCoords) {
        r.h = std::max(1, (int)std::lround(r.w * (float)idisp::kCoordsH / idisp::kCoordsW));
        L.coords = r;
    } else if (idisp::isImageId(id)) {
        for (auto& im : L.images)
            if (im.id == id) im.rect = r;
    } else {
        L.buttons[id] = r;
    }
}

// The element's layer among the drawn ones (0 = bottom) and how many are drawn.
std::pair<int, int> layerOf(const Layout& L, const std::string& id) {
    int at = -1, n = 0;
    for (const auto& o : L.drawOrder()) {
        if (!drawn(L, o) && o != id) continue;
        if (o == id) at = n;
        ++n;
    }
    return { at, n };
}

// Moves an element past the next drawn one above (up) or below (!up), or to
// the very top or bottom. Hidden and unplaced elements don't count as steps.
void moveLayer(Layout& L, const std::string& id, bool up, bool all) {
    std::vector<std::string> o = L.drawOrder();
    auto it = std::find(o.begin(), o.end(), id);
    if (it == o.end()) return;
    int from = (int)(it - o.begin());
    o.erase(it);
    int to;
    if (all) {
        to = up ? (int)o.size() : 0;
    } else if (up) {
        int i = from;  // the element that was above us is now at `from`
        while (i < (int)o.size() && !drawn(L, o[i])) ++i;
        if (i >= (int)o.size()) return;
        to = i + 1;
    } else {
        int i = from - 1;
        while (i >= 0 && !drawn(L, o[i])) --i;
        if (i < 0) return;
        to = i;
    }
    o.insert(o.begin() + to, id);
    L.order = o;
}

void dashedLine(ImVec2 a, ImVec2 b, unsigned c, float thick = 1.5f, float dash = 6) {
    const float dx = b.x - a.x, dy = b.y - a.y, len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0) return;
    for (float t = 0; t < len; t += dash * 2) {
        const float e = std::min(len, t + dash);
        dl()->AddLine(ImVec2(a.x + dx * t / len, a.y + dy * t / len), ImVec2(a.x + dx * e / len, a.y + dy * e / len), rgb(c), thick);
    }
}
void dashedRect(ImVec2 a, ImVec2 b, unsigned c) {
    dashedLine(a, ImVec2(b.x, a.y), c);
    dashedLine(ImVec2(b.x, a.y), b, c);
    dashedLine(b, ImVec2(a.x, b.y), c);
    dashedLine(ImVec2(a.x, b.y), a, c);
}

// D-pad arrow pointing in the element's direction.
void arrow(ImVec2 c, float s, const std::string& id, ImU32 col) {
    ImVec2 p[3];
    if (id == "Up") p[0] = { c.x, c.y - s }, p[1] = { c.x + s, c.y + s * 0.6f }, p[2] = { c.x - s, c.y + s * 0.6f };
    else if (id == "Down") p[0] = { c.x, c.y + s }, p[1] = { c.x - s, c.y - s * 0.6f }, p[2] = { c.x + s, c.y - s * 0.6f };
    else if (id == "Left") p[0] = { c.x - s, c.y }, p[1] = { c.x + s * 0.6f, c.y - s }, p[2] = { c.x + s * 0.6f, c.y + s };
    else p[0] = { c.x + s, c.y }, p[1] = { c.x - s * 0.6f, c.y + s }, p[2] = { c.x - s * 0.6f, c.y - s };
    dl()->AddTriangleFilled(p[0], p[1], p[2], col);
}

ImU32 rgba(const idisp::Rgba& c, int a = -1) { return IM_COL32(c.r, c.g, c.b, a < 0 ? c.a : a); }

// Small pill switch for list rows (34x20). `inverse` draws it for a navy row.
bool smallToggle(ImVec2 p, bool& v, bool inverse) {
    const ImVec2 sz(30, 18), b(p.x + sz.x, p.y + sz.y);
    ImGui::SetCursorScreenPos(p);
    const bool pressed = ImGui::InvisibleButton("##t", sz);
    if (pressed) v = !v;
    handCursorIfHovered();
    const bool hov = ImGui::IsItemHovered();
    const unsigned on = inverse ? kPanel : (hov ? kNavyHover : kNavy), knobOn = inverse ? kNavy : kPanel;
    if (v) dl()->AddRectFilled(p, b, rgb(on), sz.y * 0.5f);
    else {
        dl()->AddRectFilled(p, b, rgb(inverse ? kNavy : hov ? kHover : kTrack), sz.y * 0.5f);
        dl()->AddRect(p, b, rgb(inverse ? kOnNavyMuted : kMuted), sz.y * 0.5f, 0, 1.5f);
    }
    const float r = sz.y * 0.5f - 4;
    dl()->AddCircleFilled(ImVec2(v ? b.x - r - 4 : p.x + r + 4, p.y + sz.y * 0.5f), r,
                          rgb(v ? knobOn : (inverse ? kOnNavyMuted : kMuted)));
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) ImGui::SetTooltip(v ? "Shown on the overlay" : "Hidden on the overlay");
    return pressed;
}

} // namespace

// ---- window lifetime -----------------------------------------------------------

bool LayoutEditor::open(const std::string& assetsDir, const Layout& start) {
    if (window_) {
        SDL_RaiseWindow(window_);
        return true;
    }
    if (!SDL_CreateWindowAndRenderer("Layout editor", 1180, 760, SDL_WINDOW_RESIZABLE, &window_, &renderer_)) {
        SDL_Log("Could not open the layout editor: %s", SDL_GetError());
        return false;
    }
    SDL_SetWindowMinimumSize(window_, 900, 600);
    ctx_ = ui::createContext(window_, renderer_, assetsDir, fonts_);
    work_ = start;
    work_.preset = false;
    // The text follows the ring only while it has no box of its own.
    work_.coords = work_.coordsRect();
    saved_ = work_;
    undo_.clear();
    redo_.clear();
    selected_.clear();
    notice_.clear();
    {
        // A dialog answer that arrived after the last session closed isn't for this one.
        std::lock_guard<std::mutex> lock(dialogMutex_);
        if (dialogDone_) dialogDone_ = dialogOpen_ = false, dialogPath_.clear();
    }
    std::snprintf(name_, sizeof name_, "%s", work_.name.c_str());
    closeRequested_ = false;
    drag_ = Drag::None;
    return true;
}

void LayoutEditor::freeArt() {
    for (auto& t : art_) {
        if (t) SDL_DestroyTexture(t);
        t = nullptr;
    }
    artDir_.clear();
}

void LayoutEditor::loadArt(const std::string& dir) {
    if (dir == artDir_) return;
    freeArt();
    artDir_ = dir;
    if (dir.empty()) return;
    for (size_t i = 0; i < idisp::kButtonCount; ++i) {
        const std::string path = dir + "butt" + idisp::kButtonNames[i] + ".png";
        if (!SDL_GetPathInfo(path.c_str(), nullptr)) continue;
        art_[i] = loadTexture(renderer_, path);
        if (art_[i]) SDL_SetTextureScaleMode(art_[i], SDL_SCALEMODE_LINEAR);
    }
}

void LayoutEditor::close() {
    if (!window_) return;
    freeArt();
    freeImages();
    ui::destroyContext(ctx_);
    fonts_ = {};
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
    renderer_ = nullptr;
    window_ = nullptr;
}

bool LayoutEditor::processEvent(const SDL_Event& e) {
    if (!window_ || SDL_GetWindowFromEvent(&e) != window_) return false;
    if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        closeRequested_ = true;  // same as Cancel
        return true;
    }
    ImGui::SetCurrentContext(ctx_);
    ImGui_ImplSDL3_ProcessEvent(&e);
    return true;
}

// ---- undo ------------------------------------------------------------------------

void LayoutEditor::snapshot() {
    undo_.push_back(work_);
    if (undo_.size() > 200) undo_.erase(undo_.begin());
    redo_.clear();
}
void LayoutEditor::undo() {
    if (undo_.empty()) return;
    redo_.push_back(work_);
    work_ = undo_.back();
    undo_.pop_back();
    std::snprintf(name_, sizeof name_, "%s", work_.name.c_str());
}
void LayoutEditor::redo() {
    if (redo_.empty()) return;
    undo_.push_back(work_);
    work_ = redo_.back();
    redo_.pop_back();
    std::snprintf(name_, sizeof name_, "%s", work_.name.c_str());
}

// ---- top bar ---------------------------------------------------------------------

void LayoutEditor::drawTopBar(const LayoutEditorInput& in, LayoutEditorResult& res) {
    const ImVec2 o = ImGui::GetWindowPos();
    const float W = ImGui::GetWindowWidth();
    const float midY = o.y + kTopH * 0.5f;
    text(F->title, ImVec2(o.x + kGap, midY - F->title->FontSize * 0.5f - 1), kNavy, "Layout editor");

    // Name field
    const float nameX = o.x + kGap + textSize(F->title, "Layout editor").x + 16;
    ImGui::SetCursorScreenPos(ImVec2(nameX, midY - ImGui::GetFrameHeight() * 0.5f));
    ImGui::SetNextItemWidth(220);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, rgbv(kPanel));
    ImGui::PushStyleColor(ImGuiCol_Border, rgbv(nameHot_ ? kNavy : 0xC9D7E6));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8);
    if (ImGui::InputText("##name", name_, sizeof name_)) work_.name = name_;
    nameHot_ = ImGui::IsItemHovered() || ImGui::IsItemActive();
    if (ImGui::IsItemActivated()) snapshot();
    if (ImGui::IsItemDeactivated() && !undo_.empty() && undo_.back() == work_) undo_.pop_back();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) ImGui::SetTooltip("Layout name");
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    // Pills, right to left.
    const float py = midY - 16;
    float x = o.x + W - kGap;
    auto place = [&](const char* label) { x -= pillWidth(label); const float at = x; x -= 8; return at; };
    if (pillButton("##save", "Save layout", ImVec2(place("Save layout"), py), true)) res.save = true;
    if (pillButton("##cancel", "Cancel", ImVec2(place("Cancel"), py), false)) res.cancel = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Close without saving; the overlay goes back to the saved layout");

    char canvasLabel[48];
    std::snprintf(canvasLabel, sizeof canvasLabel, "Canvas %d \xC3\x97 %d", work_.width, work_.height);
    if (pillButton("##canvas", canvasLabel, ImVec2(place(canvasLabel), py), false)) ImGui::OpenPopup("canvasPop");
    char snapLabel[32];
    if (snapGrid_) std::snprintf(snapLabel, sizeof snapLabel, "Snap %d px", grid_);
    else std::snprintf(snapLabel, sizeof snapLabel, "%s", snapAlign_ ? "Align only" : "Snap off");
    const bool snapOn = snapGrid_ || snapAlign_;
    if (pillButton("##snap", snapLabel, ImVec2(place(snapLabel), py), snapOn)) ImGui::OpenPopup("snapPop");
    if (pillButton("##redo", "Redo", ImVec2(place("Redo"), py), false, !redo_.empty())) redo();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Ctrl+Y");
    if (pillButton("##undo", "Undo", ImVec2(place("Undo"), py), false, !undo_.empty())) undo();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Ctrl+Z");
    x -= 8;  // a little more room between editing and adding
    if (pillButton("##addimg", "Add image", ImVec2(place("Add image"), py), false, !dialogOpen_)) openImageDialog();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip("A PNG to place on the overlay: a background, a ring design, a frame.\nNew images go to the bottom layer.");

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 10));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8);
    // Popup rows: label on the left, control on the right (the settings window's grammar).
    const float popW = 300, popRowH = 38;
    auto popTitle = [&](const char* t) {
        ImGui::PushFont(F->pill);
        ImGui::TextUnformatted(t);
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(popW, 2));
    };
    auto popLabel = [&](ImVec2 p, const char* t) { text(F->body, ImVec2(p.x, p.y + (popRowH - F->body->FontSize) * 0.5f - 1), kNavy, t); };
    auto popNext = [&](ImVec2 p) {
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + popRowH));
        ImGui::Dummy(ImVec2(0, 0));
    };
    if (ImGui::BeginPopup("snapPop")) {
        popTitle("Snapping");
        ImVec2 p = ImGui::GetCursorScreenPos();
        popLabel(p, "Snap to the grid");
        ImGui::PushID("grid");
        toggleAt(ImVec2(p.x + popW - 42, p.y + (popRowH - 24) * 0.5f), snapGrid_);
        ImGui::PopID();
        popNext(p);
        p = ImGui::GetCursorScreenPos();
        popLabel(p, "Grid spacing");
        float px = p.x + popW;
        for (int g : { 16, 8, 4 }) {
            char l[16];
            std::snprintf(l, sizeof l, "%d px", g);
            px -= pillWidth(l);
            ImGui::PushID(g);
            if (pillButton("##g", l, ImVec2(px, p.y + (popRowH - 32) * 0.5f), grid_ == g, snapGrid_)) grid_ = g;
            ImGui::PopID();
            px -= 6;
        }
        popNext(p);
        p = ImGui::GetCursorScreenPos();
        popLabel(p, "Align to other elements");
        ImGui::PushID("align");
        toggleAt(ImVec2(p.x + popW - 42, p.y + (popRowH - 24) * 0.5f), snapAlign_);
        ImGui::PopID();
        popNext(p);
        ImGui::TextColored(rgbv(kMuted), "Hold Alt while dragging to place freely.");
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("canvasPop")) {
        popTitle("Canvas size");
        int v[2] = { work_.width, work_.height };
        const char* names[2] = { "Width", "Height" };
        bool changed = false;
        ImGui::PushStyleColor(ImGuiCol_FrameBg, rgbv(kTrack));
        for (int i = 0; i < 2; ++i) {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            popLabel(p, names[i]);
            ImGui::SetCursorScreenPos(ImVec2(p.x + popW - 96, p.y + (popRowH - ImGui::GetFrameHeight()) * 0.5f));
            ImGui::SetNextItemWidth(96);
            ImGui::PushID(i);
            if (ImGui::InputInt("##v", &v[i], 0, 0)) changed = true;
            if (ImGui::IsItemActivated()) snapshot();
            if (ImGui::IsItemDeactivated() && !undo_.empty() && undo_.back() == work_) undo_.pop_back();
            ImGui::PopID();
            popNext(p);
        }
        ImGui::PopStyleColor();
        if (changed) {
            work_.width = std::clamp(v[0], 64, 2048);
            work_.height = std::clamp(v[1], 64, 2048);
        }
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + popW);
        ImGui::TextColored(rgbv(kMuted), "In pixels. The overlay window scales this canvas to fit, keeping its shape.");
        ImGui::PopTextWrapPos();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(3);
    (void)in;
}

// ---- element list ------------------------------------------------------------------

void LayoutEditor::drawRail(const LayoutEditorInput& in) {
    const ImVec2 o = ImGui::GetWindowPos();
    const ImVec2 a(o.x + kGap, o.y + kTopH), b(a.x + kRailW, o.y + ImGui::GetWindowHeight() - kGap);
    dl()->AddRectFilled(a, b, rgb(kPanel), 12);
    ImGui::SetCursorScreenPos(ImVec2(a.x, a.y + 8));
    ImGui::BeginChild("##rail", ImVec2(kRailW, b.y - a.y - 16), ImGuiChildFlags_None);
    const float x0 = ImGui::GetCursorScreenPos().x, w = kRailW;
    for (const auto& g : groupsFor(work_)) {
        ImVec2 p = ImGui::GetCursorScreenPos();
        text(F->pill, ImVec2(x0 + 16, p.y + 5), kNavy, g.name.c_str());
        ImGui::SetCursorScreenPos(ImVec2(x0, p.y + 24));
        for (const auto& e : g.elems) {
            ImGui::PushID(e.id.c_str());
            p = ImGui::GetCursorScreenPos();
            const bool sel = selected_ == e.id;
            bool shown = drawn(work_, e.id);
            const ImVec2 ra(x0 + 6, p.y), rb(x0 + w - 6, p.y + kRowH);
            ImGui::SetCursorScreenPos(ra);
            if (ImGui::InvisibleButton("##row", ImVec2(rb.x - ra.x - 50, kRowH))) selected_ = e.id;
            handCursorIfHovered();
            const bool hov = ImGui::IsItemHovered();
            if (sel) dl()->AddRectFilled(ra, rb, rgb(kNavy), 8);
            else if (hov) dl()->AddRectFilled(ra, rb, rgb(kHover), 8);
            const unsigned ink = sel ? kPanel : kNavy;
            // Icon chip
            const ImVec2 c(x0 + 28, p.y + kRowH * 0.5f);
            dl()->AddCircleFilled(c, 10, rgb(sel ? kPanel : 0xE3EBF4));
            const unsigned chipInk = sel ? kNavy : kNavy;
            const int artIdx = isButton(e.id) ? buttonIndex(e.id) : -1;
            const idisp::LayoutImage* im = work_.image(e.id);
            SDL_Texture* imTex = im ? imageTex(im->file) : nullptr;
            if (artIdx >= 0 && art_[artIdx]) {
                dl()->AddImage((ImTextureID)(intptr_t)art_[artIdx], ImVec2(c.x - 10, c.y - 10), ImVec2(c.x + 10, c.y + 10));
            } else if (imTex) {
                dl()->AddImage((ImTextureID)(intptr_t)imTex, ImVec2(c.x - 8, c.y - 8), ImVec2(c.x + 8, c.y + 8));
            } else if (e.id == idisp::kStick || e.id == idisp::kStickR) {
                dl()->AddCircle(c, 6.5f, rgb(chipInk), 0, 2);
                dl()->AddCircleFilled(ImVec2(c.x + 2.5f, c.y - 2.5f), 2.2f, rgb(chipInk));
            } else if (e.chip.empty()) {
                arrow(c, 5, e.id, rgb(chipInk));
            } else {
                const ImVec2 ts = textSize(F->pill, e.chip.c_str());
                const float sc = ts.x > 16 ? 16 / ts.x : 1;
                dl()->AddText(F->pill, F->pill->FontSize * sc, ImVec2(c.x - ts.x * sc * 0.5f, c.y - ts.y * sc * 0.5f), rgb(chipInk), e.chip.c_str());
            }
            // Name (struck when hidden)
            const ImVec2 ns = textSize(F->body, e.label.c_str());
            const float ty = p.y + (kRowH - F->body->FontSize) * 0.5f - 1;
            text(F->body, ImVec2(x0 + 48, ty), shown ? ink : (sel ? kOnNavyMuted : kMuted), e.label.c_str());
            if (!shown) dl()->AddLine(ImVec2(x0 + 48, ty + ns.y * 0.55f), ImVec2(x0 + 48 + ns.x, ty + ns.y * 0.55f),
                                      rgb(sel ? kOnNavyMuted : kMuted), 1.5f);
            // Live press
            const int bi = isButton(e.id) ? buttonIndex(e.id) : -1;
            if (bi >= 0 && in.buttons[bi]) dl()->AddCircleFilled(ImVec2(x0 + 56 + ns.x, p.y + kRowH * 0.5f), 3.5f, rgb(ink));
            // Show / hide
            if (smallToggle(ImVec2(rb.x - 40, p.y + (kRowH - 18) * 0.5f), shown, sel)) {
                snapshot();
                if (shown) {
                    work_.hidden.erase(e.id);
                    if (isButton(e.id) && !work_.buttons.count(e.id)) work_.buttons[e.id] = rectOf(work_, e.id, ringRadius_);
                } else {
                    work_.hidden.insert(e.id);
                }
                selected_ = e.id;
            }
            ImGui::SetCursorScreenPos(ImVec2(x0, p.y + kRowH));
            ImGui::Dummy(ImVec2(0, 0));
            ImGui::PopID();
        }
        ImGui::SetCursorScreenPos(ImVec2(x0, ImGui::GetCursorScreenPos().y + 4));
        ImGui::Dummy(ImVec2(0, 0));
    }
    ImGui::EndChild();
}

// ---- canvas ----------------------------------------------------------------------

void LayoutEditor::drawStage(const LayoutEditorInput& in) {
    const idisp::Settings& s = *in.settings;
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 o = ImGui::GetWindowPos();
    const ImVec2 sa(o.x + kGap + kRailW + kGap, o.y + kTopH), sb(o.x + ImGui::GetWindowWidth() - kGap, o.y + ImGui::GetWindowHeight() - kGap);
    dl()->AddRectFilled(sa, sb, rgb(kStage), 12);
    dl()->PushClipRect(ImVec2(sa.x + 6, sa.y + 6), ImVec2(sb.x - 6, sb.y - 6), true);
    for (float y = sa.y + 12; y < sb.y; y += 12)
        for (float x = sa.x + 12; x < sb.x; x += 12) dl()->AddRectFilled(ImVec2(x, y), ImVec2(x + 1.5f, y + 1.5f), rgb(kDots));
    dl()->PopClipRect();

    // Zoom: the largest step that fits, leaving room for the label and the hints.
    const float availW = sb.x - sa.x - 80, availH = sb.y - sa.y - 110;
    static const float steps[] = { 3, 2, 1.5f, 1, 0.75f, 0.5f, 0.25f };
    zoom_ = 0.25f;
    for (float z : steps)
        if (work_.width * z <= availW && work_.height * z <= availH) {
            zoom_ = z;
            break;
        }
    const float cw = work_.width * zoom_, chh = work_.height * zoom_;
    originX_ = std::floor(sa.x + (sb.x - sa.x - cw) * 0.5f);
    originY_ = std::floor(sa.y + 44 + (sb.y - sa.y - 96 - chh) * 0.5f);
    auto S = [&](float x, float y) { return ImVec2(originX_ + x * zoom_, originY_ + y * zoom_); };
    const ImVec2 ca = S(0, 0), cb = S((float)work_.width, (float)work_.height);

    char lab[96];
    std::snprintf(lab, sizeof lab, "Canvas \xC2\xB7 %d%%", (int)std::lround(zoom_ * 100));
    text(F->small, ImVec2(ca.x, ca.y - 22), kMuted, lab);
    const char* hints = "Drag to move \xC2\xB7 drag a corner to resize \xC2\xB7 Alt places freely \xC2\xB7 arrows nudge 1 px, Shift+arrows a grid step";
    dl()->PushClipRect(ImVec2(sa.x, sb.y - 36), ImVec2(sb.x - 8, sb.y), true);
    if (!notice_.empty()) text(F->pill, ImVec2(sa.x + 20, sb.y - 30), kOrangeText, notice_.c_str());
    else text(F->small, ImVec2(sa.x + 20, sb.y - 30), kMuted, hints);
    dl()->PopClipRect();
    if (!(work_ == saved_)) {
        const char* u = "Unsaved changes";
        text(F->pill, ImVec2(cb.x - textSize(F->pill, u).x, ca.y - 22), kNavy, u);
    }

    // Input: the whole stage is one button; clicks pick, drag moves or resizes.
    ImGui::SetCursorScreenPos(sa);
    ImGui::InvisibleButton("##stage", ImVec2(sb.x - sa.x, sb.y - sa.y));
    const bool stageHovered = ImGui::IsItemHovered();
    const float mx = (io.MousePos.x - originX_) / zoom_, my = (io.MousePos.y - originY_) / zoom_;
    const int R = ringRadius_;

    auto hit = [&](float x, float y) {
        std::string best;
        long bestArea = 0;
        for (const std::string& id : allIds(work_)) {
            if (!drawn(work_, id) && selected_ != id) continue;
            const Rect r = rectOf(work_, id, R);
            if (x < r.x || y < r.y || x > r.x + r.w || y > r.y + r.h) continue;
            const long area = (long)r.w * r.h;  // the smallest box under the mouse wins
            if (best.empty() || area < bestArea) best = id, bestArea = area;
        }
        return best;
    };
    std::string hovered = stageHovered && drag_ == Drag::None ? hit(mx, my) : std::string();

    // Snap one value (an edge or a center) to other elements, the canvas, or the grid.
    auto snapAxis = [&](int& v, int len, bool isX) {
        if (io.KeyAlt) return;
        const float thr = 6 / zoom_;
        if (snapAlign_) {
            std::vector<int> targets = { 0, (isX ? work_.width : work_.height) / 2, isX ? work_.width : work_.height };
            for (const std::string& id : allIds(work_)) {
                if (id == selected_ || !drawn(work_, id)) continue;
                const Rect r = rectOf(work_, id, R);
                const int p = isX ? r.x : r.y, l = isX ? r.w : r.h;
                targets.insert(targets.end(), { p, p + l / 2, p + l });
            }
            float bestD = thr + 1;
            int bestT = 0, bestShift = 0;
            const int offsets[3] = { 0, len / 2, len };
            for (int off : offsets)
                for (int t : targets) {
                    const float d = (float)std::abs(t - (v + off));
                    if (d <= thr && d < bestD) bestD = d, bestT = t, bestShift = t - (v + off);
                }
            if (bestD <= thr) {
                v += bestShift;
                if (isX) guides_.push_back({ (float)bestT, 0, (float)bestT, (float)work_.height });
                else guides_.push_back({ 0, (float)bestT, (float)work_.width, (float)bestT });
                return;
            }
        }
        if (snapGrid_ && grid_ > 1) v = (int)std::lround((float)v / grid_) * grid_;
    };

    if (ImGui::IsItemActivated()) {
        drag_ = Drag::None;
        bool onHandle = false;
        if (!selected_.empty()) {
            const Rect r = rectOf(work_, selected_, R);
            const float hx[2] = { (float)r.x, (float)(r.x + r.w) }, hy[2] = { (float)r.y, (float)(r.y + r.h) };
            for (int i = 0; i < 2 && !onHandle; ++i)
                for (int j = 0; j < 2 && !onHandle; ++j) {
                    const ImVec2 h = S(hx[i], hy[j]);
                    if (std::fabs(io.MousePos.x - h.x) <= 8 && std::fabs(io.MousePos.y - h.y) <= 8) {
                        onHandle = true;
                        anchorX_ = (int)hx[1 - i];
                        anchorY_ = (int)hy[1 - j];
                    }
                }
        }
        if (onHandle) {
            drag_ = Drag::Resize;
            dragRatio_ = lockedRatio(work_, selected_);
            snapshot();
        } else {
            const std::string picked = hit(mx, my);
            selected_ = picked;
            if (!picked.empty()) {
                const Rect r = rectOf(work_, picked, R);
                grabX_ = mx - r.x;
                grabY_ = my - r.y;
                drag_ = Drag::Move;
                snapshot();
            }
        }
    }
    guides_.clear();
    if (ImGui::IsItemActive() && drag_ != Drag::None && !selected_.empty() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1)) {
        Rect r = rectOf(work_, selected_, R);
        if (drag_ == Drag::Move) {
            int nx = (int)std::lround(mx - grabX_), ny = (int)std::lround(my - grabY_);
            snapAxis(nx, r.w, true);
            snapAxis(ny, r.h, false);
            r.x = nx;
            r.y = ny;
        } else {
            int px = (int)std::lround(mx), py = (int)std::lround(my);
            snapAxis(px, 0, true);
            snapAxis(py, 0, false);
            int w = std::max(4, std::abs(px - anchorX_)), h = std::max(4, std::abs(py - anchorY_));
            const float ratio = dragRatio_;
            if (ratio > 0) {
                if (w / ratio >= h) h = std::max(1, (int)std::lround(w / ratio));
                else w = std::max(4, (int)std::lround(h * ratio));
            }
            r.x = px < anchorX_ ? anchorX_ - w : anchorX_;
            r.y = py < anchorY_ ? anchorY_ - h : anchorY_;
            r.w = w;
            r.h = h;
        }
        setRect(work_, selected_, r, R);
    }
    if (ImGui::IsItemDeactivated()) {
        if (drag_ != Drag::None && !undo_.empty() && undo_.back() == work_) undo_.pop_back();  // a click, not a change
        drag_ = Drag::None;
    }

    // ---- draw the canvas: layers bottom first, as the overlay does ----
    dl()->AddRectFilled(ca, cb, IM_COL32(s.bgColor.r, s.bgColor.g, s.bgColor.b, 255));
    dl()->PushClipRect(ca, cb, true);
    struct StickView {
        float cx, cy, scale;
        int dx, dy;
        bool lit;
        int shape;
    };
    auto stickView = [&](const std::string& id) {
        const int i = id == idisp::kStick ? 0 : 1;
        const idisp::StickOptions& o = work_.sticks[i];
        const int sh = o.shape >= 0 ? o.shape : s.shapeIndex;
        if (i == 0)
            return StickView{ (float)work_.ringCenterX, (float)work_.ringCenterY, work_.stickScale, in.drawX, in.drawY, in.lit && o.light, sh };
        return StickView{ (float)work_.rightX, (float)work_.rightY, work_.rightScale, in.drawXR, in.drawYR, in.litR && o.light, sh };
    };
    auto stickDot = [&](const StickView& v) {
        const float k = v.scale * zoom_;
        const ImVec2 c = S(v.cx, v.cy);
        const ImVec2 dot(c.x + v.dx * k, c.y - v.dy * k);
        dl()->AddLine(c, dot, rgba(s.stickColor, 255), 3 * k);
        dl()->AddCircleFilled(dot, s.circleSize * k, v.lit ? rgb(kOrange) : rgba(s.joystickCircleColor, 255));
        dl()->AddCircle(dot, s.circleSize * k, IM_COL32(0, 0, 0, 255), 0, std::max(1.0f, k));
    };
    for (const std::string& id : work_.drawOrder()) {
        if (!drawn(work_, id)) continue;
        if (isStick(id)) {
            // A simplified stick: fill and rings (the octagon as 8 sides), or nothing for "dot only".
            const StickView v = stickView(id);
            const float k = v.scale * zoom_;
            const ImVec2 c = S(v.cx, v.cy);
            if (v.shape == idisp::kShapeTarget) {
                // Same proportions as the overlay's Target (see overlay.cpp).
                const float Rk = s.ringOuterRadius * k;
                const ImU32 blue = v.lit ? rgb(kOrange) : IM_COL32(74, 140, 247, 255), dark = IM_COL32(27, 74, 160, 255);
                const ImU32 black = IM_COL32(0, 0, 0, 255), red = IM_COL32(240, 64, 32, 255);
                const float pi = 3.14159265f;
                dl()->AddCircleFilled(c, Rk * 0.95f, rgba(s.fill), 64);
                auto arc = [&](float rIn, float rOut, float a0, float a1, ImU32 col) {
                    dl()->PathArcTo(c, (rIn + rOut) * 0.5f, a0, a1, std::max(12, (int)((a1 - a0) * 20)));
                    dl()->PathStroke(col, 0, rOut - rIn);
                };
                arc(Rk * 0.972f, Rk, 0, 2 * pi, blue);
                arc(Rk * 0.948f, Rk * 0.972f, 0, 2 * pi, black);
                arc(Rk * 0.432f, Rk * 0.670f, -0.75f * pi, -0.25f * pi, blue);   // ImGui angles: y down
                arc(Rk * 0.432f, Rk * 0.670f, 0.25f * pi, 0.75f * pi, blue);
                arc(Rk * 0.278f, Rk * 0.481f, -0.25f * pi, 0.25f * pi, dark);
                arc(Rk * 0.278f, Rk * 0.481f, 0.75f * pi, 1.25f * pi, dark);
                const float e = Rk * 0.948f, ct = std::max(1.0f, Rk * 0.016f), pl = Rk * 0.122f, pt = std::max(2.0f, Rk * 0.038f);
                dl()->AddLine(ImVec2(c.x - e, c.y), ImVec2(c.x + e, c.y), black, ct);
                dl()->AddLine(ImVec2(c.x, c.y - e), ImVec2(c.x, c.y + e), black, ct);
                dl()->AddLine(ImVec2(c.x - pl, c.y), ImVec2(c.x + pl, c.y), red, pt);
                dl()->AddLine(ImVec2(c.x, c.y - pl), ImVec2(c.x, c.y + pl), red, pt);
            } else if (v.shape != idisp::kShapeDotOnly) {
                const int seg = v.shape == 2 ? 8 : 0;
                const float t1 = (float)s.ringThickness, t2 = (float)s.secondRingThickness;
                if (v.shape != 2) dl()->AddCircleFilled(c, s.ringOuterRadius * k, rgba(s.fill), 64);
                if (v.shape != 3) {
                    dl()->AddCircle(c, (s.ringOuterRadius - t1 * 0.5f) * k, v.lit ? rgb(kOrange) : rgba(s.ringColor, 255), seg, t1 * k);
                    dl()->AddCircle(c, (s.secondRingOuterRadius - t2 * 0.5f) * k, v.lit ? rgb(kOrange) : rgba(s.secondRingColor, 255), seg, t2 * k);
                } else {
                    dl()->AddCircle(c, s.ringOuterRadius * k, rgba(s.ringOutlineColor, 255), 0, std::max(1.0f, k));
                }
            }
            if (!s.stickDotOnTop) stickDot(v);
        } else if (id == idisp::kCoords) {
            const Rect r = work_.coordsRect();
            const float k = (float)r.w / idisp::kCoordsW * zoom_, fs = 17 * k;
            char x[32], y[32];
            std::snprintf(x, sizeof x, "X:  %.2f", (float)in.drawX);
            std::snprintf(y, sizeof y, "Y:   %.2f", (float)in.drawY);
            const ImVec2 p = S((float)r.x, (float)r.y);
            dl()->AddText(F->body, fs, ImVec2(p.x, p.y + 2 * k), IM_COL32_WHITE, x);
            dl()->AddText(F->body, fs, ImVec2(p.x + 116 * k, p.y + 2 * k), IM_COL32_WHITE, y);
            dl()->AddText(F->head, 20 * k, ImVec2(p.x + 205 * k, p.y), in.lit ? rgb(kOrange) : IM_COL32_WHITE, "M");
        } else if (const idisp::LayoutImage* im = work_.image(id)) {
            const ImVec2 a = S((float)im->rect.x, (float)im->rect.y), b = S((float)(im->rect.x + im->rect.w), (float)(im->rect.y + im->rect.h));
            if (SDL_Texture* t = imageTex(im->file)) {
                dl()->AddImage((ImTextureID)(intptr_t)t, a, b);
            } else {
                const char* m = "Image file missing";
                const ImVec2 ts = textSize(F->small, m);
                text(F->small, ImVec2((a.x + b.x - ts.x) * 0.5f, (a.y + b.y - ts.y) * 0.5f), kGhostText, m);
            }
        } else {
            const int i = buttonIndex(id);
            const Rect r = work_.buttons.at(id);
            const ImVec2 a = S((float)r.x, (float)r.y), b = S((float)(r.x + r.w), (float)(r.y + r.h));
            const float round = std::min(b.x - a.x, b.y - a.y) * (r.w == r.h && r.w >= 48 ? 0.5f : 0.22f);
            const bool down = in.buttons[i];
            if (art_[i]) {
                // The set's own art, faint at rest and full when pressed (like the overlay's ghost buttons).
                dl()->AddImage((ImTextureID)(intptr_t)art_[i], a, b, ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, down ? 255 : 120));
                continue;
            }
            if (down) dl()->AddRectFilled(a, b, rgb(kGhostText), round);
            else dl()->AddRect(a, b, rgb(kGhost), round, 0, std::max(1.5f, 2 * zoom_));
            const unsigned ink = down ? 0x0A205B : kGhostText;
            const ImVec2 c((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
            const Elem* e = findElem(id);
            if (e && e->chip.empty()) {
                arrow(c, std::min(b.x - a.x, b.y - a.y) * 0.22f, id, rgb(ink));
            } else {
                const char* t = e ? e->chip.c_str() : id.c_str();
                const float fs = std::min((b.y - a.y) * 0.55f, 34.0f);
                const ImVec2 ts = F->head->CalcTextSizeA(fs, FLT_MAX, 0, t);
                dl()->AddText(F->head, fs, ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), rgb(ink), t);
            }
        }
    }
    if (s.stickDotOnTop)
        for (const char* id : { idisp::kStick, idisp::kStickR })
            if (drawn(work_, id)) stickDot(stickView(id));

    // Every element's box, above the art, so each one reads as placeable.
    for (const std::string& id : allIds(work_)) {
        if (!drawn(work_, id)) continue;
        const Rect r = rectOf(work_, id, R);
        const ImVec2 a = S((float)r.x, (float)r.y), b = S((float)(r.x + r.w), (float)(r.y + r.h));
        if (id == idisp::kCoords || idisp::isImageId(id)) dashedRect(a, b, kGhost);
        else if (isStick(id) || art_[buttonIndex(id)]) dl()->AddRect(a, b, rgb(kGhost, 110), 0, 0, 1);
    }
    for (const auto& g : guides_) dashedLine(S(g[0], g[1]), S(g[2], g[3]), kGuide, 1.5f, 5);
    dl()->PopClipRect();
    dl()->AddRect(ImVec2(ca.x - 1, ca.y - 1), ImVec2(cb.x + 1, cb.y + 1), rgb(kCanvasEdge));

    // Hover and selection marks (may sit outside the canvas).
    if (!hovered.empty() && hovered != selected_) {
        const Rect r = rectOf(work_, hovered, R);
        dl()->AddRect(S(r.x - 2.0f / zoom_, r.y - 2.0f / zoom_), S(r.x + r.w + 2.0f / zoom_, r.y + r.h + 2.0f / zoom_), rgb(kGhostText, 160), 4, 0, 1.5f);
    }
    cardVisible_ = false;
    if (!selected_.empty()) {
        const Rect r = rectOf(work_, selected_, R);
        const ImVec2 a = S((float)r.x, (float)r.y), b = S((float)(r.x + r.w), (float)(r.y + r.h));
        const ImVec2 oa(a.x - 4, a.y - 4), ob(b.x + 4, b.y + 4);
        if (drawn(work_, selected_)) dl()->AddRect(oa, ob, IM_COL32_WHITE, 4, 0, 2);
        else dashedRect(oa, ob, kPanel);
        for (ImVec2 h : { a, ImVec2(b.x, a.y), ImVec2(a.x, b.y), b }) {
            dl()->AddRectFilled(ImVec2(h.x - 5, h.y - 5), ImVec2(h.x + 5, h.y + 5), rgb(kPanel));
            dl()->AddRect(ImVec2(h.x - 5, h.y - 5), ImVec2(h.x + 5, h.y + 5), rgb(kNavy), 0, 0, 2);
        }
        // The number card sits on the free side of the selection.
        // Try right, left, below and above; keep the spot that covers the
        // least of the other elements (and stays inside the stage).
        const float cardW = cardW_, cardH = cardH_;
        // Next to the selection first, then in the stage margins outside the canvas.
        const ImVec2 spots[8] = { { ob.x + 18, oa.y }, { oa.x - 18 - cardW, oa.y }, { oa.x, ob.y + 18 }, { oa.x, oa.y - 18 - cardH },
                                  { cb.x + 12, oa.y }, { ca.x - 12 - cardW, oa.y }, { oa.x, cb.y + 12 }, { oa.x, ca.y - 12 - cardH } };
        float bestCost = FLT_MAX;
        for (const ImVec2& sp : spots) {
            const float x = std::clamp(sp.x, sa.x + 8, sb.x - 8 - cardW), y = std::clamp(sp.y, sa.y + 8, sb.y - 8 - cardH);
            auto over = [&](ImVec2 qa, ImVec2 qb) {
                return std::max(0.0f, std::min(x + cardW, qb.x) - std::max(x, qa.x)) * std::max(0.0f, std::min(y + cardH, qb.y) - std::max(y, qa.y));
            };
            float cost = (std::fabs(x - sp.x) + std::fabs(y - sp.y)) * 5;  // clamped into the stage
            cost += over(oa, ob) > 0 ? 100000 : 0;                           // never on the selection
            // Each covered element costs a lot (small ones are what you align
            // against); the ring's open middle costs little.
            for (const std::string& id : allIds(work_)) {
                    if (id == selected_ || !drawn(work_, id)) continue;
                    const Rect q = rectOf(work_, id, R);
                    const float o2 = over(S((float)q.x, (float)q.y), S((float)(q.x + q.w), (float)(q.y + q.h)));
                    if (o2 <= 0) continue;
                    if (id == idisp::kStick || id == idisp::kStickR || idisp::isImageId(id)) cost += o2 * 0.05f;
                    else cost += 5000 + o2;
                }
            if (cost < bestCost) bestCost = cost, cardX_ = x, cardY_ = y;
        }
        cardVisible_ = true;
        // Soft shadow under the card (drawn here so it sits behind it).
        for (int i = 3; i >= 1; --i)
            dl()->AddRectFilled(ImVec2(cardX_ - i * 2.0f, cardY_ + 6 - i), ImVec2(cardX_ + cardW + i * 2.0f, cardY_ + cardH + 2 + i * 2.0f),
                                IM_COL32(13, 30, 60, 14), 12 + i * 2.0f);
    }

    // Keyboard: nudge with arrows (when no text field or popup has focus).
    if (!selected_.empty() && !io.WantTextInput && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) {
        // Claim only the arrow keys, so keyboard navigation (Tab, focus rings) keeps working.
        const ImGuiID nudgeOwner = ImGui::GetID("##nudge");
        for (ImGuiKey k : { ImGuiKey_LeftArrow, ImGuiKey_RightArrow, ImGuiKey_UpArrow, ImGuiKey_DownArrow })
            ImGui::SetKeyOwner(k, nudgeOwner, ImGuiInputFlags_LockThisFrame);
        const int step = io.KeyShift ? std::max(1, grid_) : 1;
        int dx = 0, dy = 0;
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, ImGuiInputFlags_Repeat, nudgeOwner)) dx = -step;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, ImGuiInputFlags_Repeat, nudgeOwner)) dx = step;
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, ImGuiInputFlags_Repeat, nudgeOwner)) dy = -step;
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, ImGuiInputFlags_Repeat, nudgeOwner)) dy = step;
        if (dx || dy) {
            snapshot();
            Rect r = rectOf(work_, selected_, R);
            r.x += dx;
            r.y += dy;
            setRect(work_, selected_, r, R);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) selected_.clear();
        if (ImGui::IsKeyPressed(ImGuiKey_Delete) && idisp::isImageId(selected_)) deleteImage(selected_);
    }
}

// ---- number card -----------------------------------------------------------------

void LayoutEditor::drawCard() {
    if (!cardVisible_ || selected_.empty()) return;
    const Elem* e = findElem(selected_);
    const idisp::LayoutImage* im = work_.image(selected_);
    ImGui::SetNextWindowPos(ImVec2(cardX_, cardY_));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, rgbv(kPanel));
    ImGui::Begin("##card", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);
    // The group tells the face buttons' Left/Right from the d-pad's.
    std::string title = e ? e->label : im ? imageLabel(*im) : selected_;
    for (const auto& g : groups())
        for (const auto& el : g.elems)
            if (selected_ == el.id) {
                if (g.name == "Face buttons") title += " face button";
                else if (g.name == "D-pad") title = "D-pad " + title;
            }
    ImGui::PushFont(F->head);
    ImGui::TextUnformatted(title.c_str());
    ImGui::PopFont();

    const int R = ringRadius_;
    Rect r = rectOf(work_, selected_, R);
    int v[4] = { r.x, r.y, r.w, r.h };
    static const char* names[4] = { "X", "Y", "W", "H" };
    bool changed = false;
    int which = -1;
    for (int i = 0; i < 4; ++i) {
        ImGui::PushID(i);
        if (i % 2) ImGui::SameLine(0, 14);
        ImGui::AlignTextToFramePadding();
        ImGui::PushFont(F->pill);
        ImGui::TextColored(rgbv(kMuted), "%s", names[i]);
        ImGui::PopFont();
        ImGui::SameLine(0, 6);
        ImGui::SetNextItemWidth(72);
        if (ImGui::InputInt("##v", &v[i], 0, 0)) changed = true, which = i;
        if (ImGui::IsItemActivated()) snapshot();
        if (ImGui::IsItemDeactivated() && !undo_.empty() && undo_.back() == work_) undo_.pop_back();
        ImGui::PopID();
    }
    if (changed) {
        const float ratio = lockedRatio(work_, selected_);
        if (ratio > 0 && which == 3) v[2] = (int)std::lround(v[3] * ratio);
        if (ratio > 0 && which == 2) v[3] = (int)std::lround(v[2] / ratio);
        setRect(work_, selected_, Rect{ v[0], v[1], v[2], v[3] }, R);
    }
    if (lockedRatio(work_, selected_) > 0) ImGui::TextColored(rgbv(kMuted), "Keeps its proportions");

    // A label on the left, a switch on the right (the settings window's grammar).
    const float rowW = 204;
    auto toggleRow = [&](const char* id, const char* label, bool v) {
        ImGui::Dummy(ImVec2(0, 2));
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::PushID(id);
        const bool hit = toggleAt(ImVec2(p.x + rowW - 42, p.y), v);
        ImGui::PopID();
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + 28));
        ImGui::Dummy(ImVec2(0, 0));
        return hit ? std::optional<bool>(v) : std::nullopt;
    };

    if (auto shown = toggleRow("show", "Show on the overlay", drawn(work_, selected_))) {
        snapshot();
        if (*shown) {
            work_.hidden.erase(selected_);
            if (isButton(selected_) && !work_.buttons.count(selected_)) work_.buttons[selected_] = r;
        } else {
            work_.hidden.insert(selected_);
        }
    }

    // Sticks: the M-speed light and a shape of their own.
    if (isStick(selected_)) {
        idisp::StickOptions& o = work_.sticks[selected_ == idisp::kStick ? 0 : 1];
        if (auto light = toggleRow("light", "Show M-speed", o.light)) {
            snapshot();
            o.light = *light;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip(selected_ == idisp::kStick ? "Light this ring when the M-speed rule is hit"
                                                         : "Light this ring when the right stick's own position hits the M-speed ring");
        static const char* shapes[] = { "Match settings", "Rays", "Rays + cross", "Octagon", "Plain", "Ring", "Clock", "Dot only", "Target" };
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Shape");
        ImGui::SameLine(0, 0);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, rowW - 150 - textSize(F->body, "Shape").x));
        ImGui::SetNextItemWidth(150);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, rgbv(kTrack));
        int pick = o.shape + 1;
        if (ImGui::Combo("##shape", &pick, shapes, IM_ARRAYSIZE(shapes))) {
            snapshot();
            o.shape = pick - 1;
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("\"Match settings\" follows the Shape in Display studio. \"Dot only\" draws just the dot and its line, for your own ring image.");
    }

    // Layer: where it sits in the stack of drawn elements.
    if (drawn(work_, selected_)) {
        const auto [at, n] = layerOf(work_, selected_);
        ImGui::Dummy(ImVec2(0, 2));
        ImGui::PushFont(F->pill);
        ImGui::TextColored(rgbv(kMuted), "Layer %d of %d", at + 1, n);
        ImGui::PopFont();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        float x = p.x;
        struct Step {
            const char* id;
            const char* label;
            const char* tip;
            bool up, all;
        };
        static const Step steps[] = { { "##bottom", "Bottom", "Send to the bottom layer", false, true },
                                      { "##down", "Down", "One layer down", false, false },
                                      { "##up", "Up", "One layer up", true, false },
                                      { "##top", "Top", "Bring to the top layer", true, true } };
        for (const Step& st : steps) {
            const bool can = st.up ? at < n - 1 : at > 0;
            if (pillButton(st.id, st.label, ImVec2(x, p.y), false, can)) {
                snapshot();
                moveLayer(work_, selected_, st.up, st.all);
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_DelayNormal)) ImGui::SetTooltip("%s", st.tip);
            x += pillWidth(st.label) + 6;
        }
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + 38));
        ImGui::Dummy(ImVec2(0, 0));
        if (isStick(selected_) && settings_ && settings_->stickDotOnTop)
            ImGui::TextColored(rgbv(kMuted), "The dot and line stay above every layer.");
    }

    // Images: remove from this layout (the file stays in the images folder).
    if (im) {
        ImGui::Dummy(ImVec2(0, 2));
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const std::string id = selected_;
        if (pillButton("##delimg", "Remove image", p, false)) deleteImage(id);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) ImGui::SetTooltip("Delete key. The PNG stays in the images folder.");
        ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + 38));
        ImGui::Dummy(ImVec2(0, 0));
    }
    cardW_ = ImGui::GetWindowWidth();
    cardH_ = ImGui::GetWindowHeight();
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);
}

// ---- the frame -------------------------------------------------------------------

LayoutEditorResult LayoutEditor::frame(const LayoutEditorInput& in) {
    LayoutEditorResult res;
    if (!window_ || !in.settings) return res;
    if (closeRequested_) {
        res.cancel = true;
        return res;
    }
    ringRadius_ = std::max(1, in.settings->ringOuterRadius);
    settings_ = in.settings;
    loadArt(in.buttonSetDir);
    imagesDir_ = in.imagesDir;
    {
        std::string picked;
        {
            std::lock_guard<std::mutex> lock(dialogMutex_);
            if (dialogDone_) {
                picked = dialogPath_;
                dialogDone_ = dialogOpen_ = false;
            }
        }
        if (!picked.empty()) addImage(picked);
    }
    ImGui::SetCurrentContext(ctx_);
    ui::F = &fonts_;
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("##editor", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
    drawTopBar(in, res);
    drawStage(in);
    drawRail(in);

    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput) {
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z)) undo();
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y) || ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z)) redo();
    }
    ImGui::End();
    drawCard();
    lastWantText_ = ImGui::GetIO().WantTextInput;

    if (res.save) {
        // A name is required and must not repeat another layout's.
        std::string n = work_.name;
        while (!n.empty() && n.back() == ' ') n.pop_back();
        while (!n.empty() && n.front() == ' ') n.erase(n.begin());
        if (n.empty()) n = "Custom layout";
        const std::string base = n;
        for (int i = 2; std::find(in.otherNames.begin(), in.otherNames.end(), n) != in.otherNames.end(); ++i)
            n = base + " " + std::to_string(i);
        res.renamed = n != base;
        work_.name = n;
        std::snprintf(name_, sizeof name_, "%s", n.c_str());
    }

    ImGui::Render();
    SDL_SetRenderDrawColor(renderer_, 0xE9, 0xF0, 0xF7, 255);
    SDL_RenderClear(renderer_);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
    SDL_RenderPresent(renderer_);
    return res;
}

// ---- images ----------------------------------------------------------------------

SDL_Texture* LayoutEditor::imageTex(const std::string& file) {
    auto it = images_.find(file);
    if (it != images_.end()) return it->second;
    SDL_Texture* t = nullptr;
    if (idisp::safeImageFile(file) && !imagesDir_.empty()) {
        const std::string path = imagesDir_ + file;
        if (SDL_GetPathInfo(path.c_str(), nullptr)) t = loadTexture(renderer_, path);
        if (t) SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
    }
    images_[file] = t;
    return t;
}

void LayoutEditor::freeImages() {
    for (auto& [_, t] : images_)
        if (t) SDL_DestroyTexture(t);
    images_.clear();
}

void LayoutEditor::openImageDialog() {
    {
        std::lock_guard<std::mutex> lock(dialogMutex_);
        if (dialogOpen_) return;
        dialogOpen_ = true;
        dialogDone_ = false;
        dialogPath_.clear();
    }
    static const SDL_DialogFileFilter filters[] = { { "PNG images", "png" } };
    // SDL may call back on another thread, so the result goes through the mutex
    // and frame() picks it up.
    SDL_ShowOpenFileDialog(
        [](void* self, const char* const* files, int) {
            auto* ed = static_cast<LayoutEditor*>(self);
            std::lock_guard<std::mutex> lock(ed->dialogMutex_);
            ed->dialogPath_ = files && files[0] ? files[0] : "";
            ed->dialogDone_ = true;
        },
        this, window_, filters, 1, nullptr, false);
}

void LayoutEditor::addImage(const std::string& source) {
    notice_.clear();
    std::error_code ec;
    // Paths are UTF-8; on Windows a plain std::string path would be read as the ANSI code page.
    auto P = [](const std::string& u) { return fs::path(std::u8string(u.begin(), u.end())); };
    const fs::path src = P(source);
    // A plain ASCII file name, so the layout file and every platform agree on it.
    std::string stem;
    for (char8_t c : src.stem().u8string()) {
        const bool keep = c < 128 && (std::isalnum((int)c) || c == '-' || c == '_');
        if (keep) stem += (char)c;
        else if ((c < 0x80 || c >= 0xC0) && !stem.empty() && stem.back() != '_') stem += '_';  // one '_' per run (and per UTF-8 character)
    }
    while (!stem.empty() && stem.back() == '_') stem.pop_back();
    if (stem.size() > 60) stem.resize(60);
    if (stem.empty()) stem = "image";
    fs::create_directories(P(imagesDir_), ec);
    std::string name = stem + ".png";
    for (int i = 2; fs::exists(P(imagesDir_ + name), ec); ++i) name = stem + "-" + std::to_string(i) + ".png";
    const std::string dest = imagesDir_ + name;
    if (imagesDir_.empty() || !fs::copy_file(src, P(dest), ec) || ec) {
        notice_ = "Couldn't copy the image into the images folder" + (ec ? " (" + ec.message() + ")" : std::string()) + ".";
        return;
    }
    SDL_Texture* t = imageTex(name);
    float w = 0, h = 0;
    if (t) SDL_GetTextureSize(t, &w, &h);
    if (!t || w < 1 || h < 1) {
        notice_ = "That file isn't a PNG this app can read.";
        images_.erase(name);
        fs::remove(P(dest), ec);
        return;
    }
    // Its own size, made smaller if it doesn't fit the canvas, centered.
    const float k = std::min({ 1.0f, work_.width / w, work_.height / h });
    Rect r;
    r.w = std::max(4, (int)std::lround(w * k));
    r.h = std::max(4, (int)std::lround(h * k));
    r.x = (work_.width - r.w) / 2;
    r.y = (work_.height - r.h) / 2;
    int n = 1;
    while (work_.image("img" + std::to_string(n))) ++n;
    snapshot();
    work_.images.push_back({ "img" + std::to_string(n), name, r });
    // drawOrder() puts an image missing from the order at the bottom.
    selected_ = work_.images.back().id;
}

void LayoutEditor::deleteImage(const std::string& id) {
    if (!work_.image(id)) return;
    snapshot();
    std::erase_if(work_.images, [&](const idisp::LayoutImage& im) { return im.id == id; });
    std::erase(work_.order, id);
    work_.hidden.erase(id);
    if (selected_ == id) selected_.clear();
}
