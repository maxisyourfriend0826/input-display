// Separate settings window (Dear ImGui), opened with M. It is a second OS
// window, so it never appears in the OBS capture of the overlay.
#pragma once
#include "idisp/layout.hpp"
#include "idisp/rules.hpp"
#include "idisp/settings.hpp"

#include <SDL3/SDL.h>
#include <array>
#include <string>
#include <vector>

struct ImGuiContext;
#include "ui_theme.hpp"

// Game probe panel: live game-memory values next to the pad's.
struct GameProbeInput {
    bool attached = false;
    std::string message;                     // why not attached
    std::string game, process, version;
    unsigned pid = 0, timeDateStamp = 0, sizeOfImage = 0;
    bool exactBuild = false;
    std::vector<std::pair<std::string, double>> values;  // scaled, by name
    int padRawX = 0, padRawY = 0;            // SDL axis values
    float padX = 0, padY = 0;                // pad in stick units
    bool logging = false;
    std::string logPath;
    unsigned long long logRows = 0;
};

struct SettingsUiInput {
    std::vector<std::string> profiles;  // profile file names without .json
    const std::vector<std::string>* buttonSets = nullptr;
    const idisp::LayoutFile* layouts = nullptr;
    int overlayW = 0, overlayH = 0;  // overlay window size now, in pixels
    float overlayZoom = 1;           // how much the layout canvas is scaled to fit it
    bool editorOpen = false;         // the layout editor window is open
    float unitsX = 0, unitsY = 0;
    // Live state for the stick tile at the top.
    float lcdX = 0, lcdY = 0;     // drawn stick in pad space, -1..1, +y up
    double gameDistance = 0;      // stick distance in game space: what M-speed checks
    bool ruleLit = false;
    bool showingGame = false;     // drawing the game's value (F8 on and readable)
    bool gameMissing = false;     // F8 on but the game can't be read
    bool controllerConnected = false;
    std::string controllerName;
    std::string status;  // last message (errors, saves)
    GameProbeInput probe;
};

struct SettingsUiResult {
    bool settingsChanged = false;  // anything in Settings
    bool assetsChanged = false;    // button set index
    bool profileSelected = false;  // switched to another profile file
    bool saveSettings = false;
    bool toggleLogging = false;
    bool reloadGames = false;
    bool resetOverlaySize = false;  // put the overlay window back to the canvas size
    int editLayout = -1;            // open this custom layout in the editor
    int duplicateLayout = -1;       // copy this layout to a new custom one and edit it
    int deleteLayout = -1;          // delete this custom layout
};

class SettingsWindow {
public:
    bool open(const std::string& assetsDir);
    void close();
    bool isOpen() const { return window_ != nullptr; }
    // Returns true if the event belonged to this window.
    bool processEvent(const SDL_Event& e);
    SettingsUiResult frame(idisp::Settings& s, idisp::Profile& profile, const SettingsUiInput& in);

    using Fonts = ui::Fonts;

private:
    void loadThumbs(const SettingsUiInput& in);
    void freeThumbs();
    void drawTop(const idisp::Settings& s, const SettingsUiInput& in);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    ImGuiContext* ctx_ = nullptr;
    std::string assets_;
    Fonts fonts_;
    // Skin thumbnails, loaded into this window's renderer: Y, X, B, A per set.
    std::vector<std::array<SDL_Texture*, 4>> setThumbs_;
    bool practiceOpen_ = false;
    bool openDisplay_ = true, openInput_ = false, openMSpeed_ = false, openResearch_ = false;
    bool openRingSizes_ = false, openAdvanced_ = false;
    int confirmDelete_ = -1;  // layout whose Delete was clicked once
    // Stick tile: which cells are inside the M-speed ring, for the current rule.
    float lcdD_ = -1, lcdMin_ = -1, lcdMax_ = -1, lcdView_ = 1;
    std::vector<unsigned char> lcdCells_;
};
