// Shared look for the app's ImGui windows (settings, layout editor): the
// console-system-menu palette, fonts, and the pill controls both windows use.
#pragma once
#include <SDL3/SDL.h>
#include <imgui.h>
#include <string>

namespace ui {

// Palette (see .impeccable/surfaces/app-settings-window-cpp.md).
inline constexpr unsigned kGround = 0xE9F0F7, kPanel = 0xFFFFFF, kNavy = 0x1D3557, kMuted = 0x4F6A8A;
inline constexpr unsigned kLine = 0xE3EBF4, kTrack = 0xE9F0F7, kHover = 0xDCE6F1, kNavyHover = 0x2B4A72, kNavyDown = 0x142640;
inline constexpr unsigned kOrange = 0xF08A24, kOrangeText = 0xB35500, kOnNavyMuted = 0xD3E0EF;

struct Fonts {
    ImFont *body = nullptr, *small = nullptr, *head = nullptr, *title = nullptr, *pill = nullptr;
};
extern const Fonts* F;  // the current window's fonts; set at the start of each frame

ImU32 rgb(unsigned hex, int a = 255);
ImVec4 rgbv(unsigned hex);

ImDrawList* dl();
ImVec2 textSize(ImFont* f, const char* s);
void text(ImFont* f, ImVec2 p, unsigned c, const char* s);

void handCursorIfHovered();
// Keyboard focus ring for custom-drawn controls.
void focusRing(ImVec2 a, ImVec2 b, float rounding);
// Pill switch at p (42x24). Off: outlined. On: navy filled.
bool toggleAt(ImVec2 p, bool& v, bool enabled = true);
// Pill button. Primary: navy filled. Secondary: navy outline.
bool pillButton(const char* id, const char* label, ImVec2 p, bool primary, bool enabled = true);
float pillWidth(const char* label);

// Creates an ImGui context for a window with the shared fonts and style.
// Leaves the previous context current.
ImGuiContext* createContext(SDL_Window* window, SDL_Renderer* renderer, const std::string& assetsDir, Fonts& fonts);
void destroyContext(ImGuiContext*& ctx);

} // namespace ui
