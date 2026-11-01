#include "ui_theme.hpp"

#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <cfloat>

namespace ui {

const Fonts* F = nullptr;

ImU32 rgb(unsigned hex, int a) { return IM_COL32((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF, a); }
ImVec4 rgbv(unsigned hex) { return ImGui::ColorConvertU32ToFloat4(rgb(hex)); }

ImDrawList* dl() { return ImGui::GetWindowDrawList(); }
ImVec2 textSize(ImFont* f, const char* s) { return f->CalcTextSizeA(f->FontSize, FLT_MAX, 0, s); }
void text(ImFont* f, ImVec2 p, unsigned c, const char* s) { dl()->AddText(f, f->FontSize, p, rgb(c), s); }

void handCursorIfHovered() {
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
}

void focusRing(ImVec2 a, ImVec2 b, float rounding) {
    if (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible)
        dl()->AddRect(ImVec2(a.x - 3, a.y - 3), ImVec2(b.x + 3, b.y + 3), rgb(kNavy), rounding + 3, 0, 2);
}

bool toggleAt(ImVec2 p, bool& v, bool enabled) {
    const ImVec2 sz(42, 24);
    ImGui::SetCursorScreenPos(p);
    ImGui::BeginDisabled(!enabled);
    const bool pressed = ImGui::InvisibleButton("##t", sz);
    ImGui::EndDisabled();
    if (pressed) v = !v;
    handCursorIfHovered();
    const ImVec2 b(p.x + sz.x, p.y + sz.y);
    const bool hov = ImGui::IsItemHovered();
    if (v) dl()->AddRectFilled(p, b, rgb(hov ? kNavyHover : kNavy), sz.y * 0.5f);
    else {
        dl()->AddRectFilled(p, b, rgb(hov ? kHover : kTrack), sz.y * 0.5f);
        dl()->AddRect(p, b, rgb(kMuted), sz.y * 0.5f, 0, 1.5f);
    }
    const float r = sz.y * 0.5f - 4;
    const ImVec2 knob(v ? b.x - r - 4 : p.x + r + 4, p.y + sz.y * 0.5f);
    dl()->AddCircleFilled(knob, r, rgb(v ? kPanel : kMuted));
    focusRing(p, b, sz.y * 0.5f);
    return pressed;
}

bool pillButton(const char* id, const char* label, ImVec2 p, bool primary, bool enabled) {
    const ImVec2 ts = textSize(F->pill, label), sz(ts.x + 30, 32), b(p.x + sz.x, p.y + sz.y);
    ImGui::SetCursorScreenPos(p);
    ImGui::BeginDisabled(!enabled);
    const bool pressed = ImGui::InvisibleButton(id, sz);
    ImGui::EndDisabled();
    if (enabled) handCursorIfHovered();
    const bool hov = enabled && ImGui::IsItemHovered(), down = ImGui::IsItemActive();
    if (!enabled) dl()->AddRect(p, b, rgb(kLine), sz.y * 0.5f, 0, 1.5f);
    else if (primary) dl()->AddRectFilled(p, b, rgb(down ? kNavyDown : hov ? kNavyHover : kNavy), sz.y * 0.5f);
    else {
        dl()->AddRectFilled(p, b, rgb(down ? kHover : hov ? kTrack : kPanel), sz.y * 0.5f);
        dl()->AddRect(p, b, rgb(kNavy), sz.y * 0.5f, 0, 1.5f);
    }
    text(F->pill, ImVec2(p.x + 15, p.y + (sz.y - ts.y) * 0.5f), !enabled ? kMuted : primary ? kPanel : kNavy, label);
    focusRing(p, b, sz.y * 0.5f);
    return pressed;
}
float pillWidth(const char* label) { return textSize(F->pill, label).x + 30; }

ImGuiContext* createContext(SDL_Window* window, SDL_Renderer* renderer, const std::string& assetsDir, Fonts& fonts) {
    ImGuiContext* previous = ImGui::GetCurrentContext();
    ImGuiContext* ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // don't write imgui.ini into the working folder
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // M PLUS Rounded 1c (OFL, assets/fonts). Latin plus a few punctuation marks.
    static const ImWchar ranges[] = { 0x0020, 0x00FF, 0x2013, 0x2014, 0x2026, 0x2026, 0 };
    const std::string medium = assetsDir + "fonts/MPLUSRounded1c-Medium.ttf";
    const std::string bold = assetsDir + "fonts/MPLUSRounded1c-ExtraBold.ttf";
    auto font = [&](const std::string& path, float size) -> ImFont* {
        if (!SDL_GetPathInfo(path.c_str(), nullptr)) return nullptr;
        return io.Fonts->AddFontFromFileTTF(path.c_str(), size, nullptr, ranges);
    };
    fonts.body = font(medium, 15);
    fonts.small = font(medium, 13);
    fonts.head = font(bold, 16);
    fonts.title = font(bold, 22);
    fonts.pill = font(bold, 13);
    if (!fonts.body || !fonts.small || !fonts.head || !fonts.title || !fonts.pill) {
        SDL_Log("UI fonts missing in %sfonts; using the built-in font", assetsDir.c_str());
        io.Fonts->Clear();
        ImFont* f = io.Fonts->AddFontDefault();
        fonts = { f, f, f, f, f };
    }
    io.FontDefault = fonts.body;

    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowPadding = ImVec2(0, 0);
    st.WindowBorderSize = 0;
    st.FrameRounding = 14;
    st.GrabRounding = 10;
    st.PopupRounding = 12;
    st.PopupBorderSize = 1;
    st.ScrollbarRounding = 8;
    st.ScrollbarSize = 12;
    st.FramePadding = ImVec2(12, 5);
    st.GrabMinSize = 18;
    st.ItemSpacing = ImVec2(8, 0);
    st.DisabledAlpha = 0.45f;
    ImVec4* c = st.Colors;
    c[ImGuiCol_Text] = rgbv(kNavy);
    c[ImGuiCol_TextDisabled] = rgbv(kMuted);
    c[ImGuiCol_WindowBg] = rgbv(kGround);
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = rgbv(kPanel);
    c[ImGuiCol_Border] = rgbv(0xC9D7E6);
    c[ImGuiCol_FrameBg] = rgbv(kTrack);
    c[ImGuiCol_FrameBgHovered] = rgbv(kHover);
    c[ImGuiCol_FrameBgActive] = rgbv(kHover);
    c[ImGuiCol_SliderGrab] = rgbv(kNavy);
    c[ImGuiCol_SliderGrabActive] = rgbv(kNavyHover);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = rgbv(0xB8C8DA);
    c[ImGuiCol_ScrollbarGrabHovered] = rgbv(0x9DB2C9);
    c[ImGuiCol_ScrollbarGrabActive] = rgbv(kMuted);
    c[ImGuiCol_CheckMark] = rgbv(kNavy);
    c[ImGuiCol_NavCursor] = rgbv(kNavy);
    c[ImGuiCol_TextSelectedBg] = ImGui::ColorConvertU32ToFloat4(rgb(kNavy, 70));

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);
    ImGui::SetCurrentContext(previous ? previous : ctx);
    return ctx;
}

void destroyContext(ImGuiContext*& ctx) {
    if (!ctx) return;
    ImGuiContext* previous = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(ctx);
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext(ctx);
    ImGui::SetCurrentContext(previous != ctx ? previous : nullptr);
    ctx = nullptr;
}

} // namespace ui
