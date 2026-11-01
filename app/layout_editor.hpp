// The layout editor window (see .impeccable/surfaces/app-layout-editor-cpp.md):
// a list of the overlay's elements, the canvas, and the selected element's
// numbers beside it. It edits a working copy; the overlay draws that copy
// live, and Save or Cancel ends the session.
#pragma once
#include "idisp/layout.hpp"
#include "idisp/settings.hpp"
#include "ui_theme.hpp"

#include <SDL3/SDL.h>
#include <array>
#include <map>
#include <mutex>
#include <string>
#include <vector>

struct ImGuiContext;

struct LayoutEditorInput {
    const idisp::Settings* settings = nullptr;
    int drawX = 0, drawY = 0;  // stick dot offset in canvas px at scale 1 (+y up)
    std::array<bool, idisp::kButtonCount> buttons{};
    bool lit = false;          // M-speed light is on
    int drawXR = 0, drawYR = 0;  // right stick
    bool litR = false;           // the right stick's own ring hit
    std::string imagesDir;       // user images folder (ends with a slash)
    std::vector<std::string> otherNames;  // names already used by other layouts
    std::string buttonSetDir;             // assets/sets/<set>/ : the button art drawn on the canvas
    std::string toonPath;                 // the active toon's first frame ("" = toon off)
};

struct LayoutEditorResult {
    bool save = false;    // keep the working copy
    bool cancel = false;  // throw it away
    bool renamed = false; // the name was taken, so Save added a number
};

class LayoutEditor {
public:
    bool open(const std::string& assetsDir, const idisp::Layout& start);
    void close();
    bool isOpen() const { return window_ != nullptr; }
    // Returns true if the event belonged to this window.
    bool processEvent(const SDL_Event& e);
    LayoutEditorResult frame(const LayoutEditorInput& in);
    const idisp::Layout& working() const { return work_; }

private:
    void snapshot();
    void undo();
    void redo();
    void drawTopBar(const LayoutEditorInput& in, LayoutEditorResult& res);
    void drawRail(const LayoutEditorInput& in);
    void drawStage(const LayoutEditorInput& in);
    void drawCard();
    void loadArt(const std::string& dir);
    void freeArt();
    SDL_Texture* imageTex(const std::string& file);
    void freeImages();
    void openImageDialog();
    void addImage(const std::string& source);
    void deleteImage(const std::string& id);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    ImGuiContext* ctx_ = nullptr;
    ui::Fonts fonts_;

    idisp::Layout work_, saved_;
    std::vector<idisp::Layout> undo_, redo_;
    std::string selected_;      // element id, or empty
    char name_[64] = {};
    bool closeRequested_ = false;
    int ringRadius_ = 128;      // the settings' outer ring radius, for the stick's box

    // Snapping
    bool snapGrid_ = true, snapAlign_ = true;
    int grid_ = 8;

    // Canvas view and drag state
    float zoom_ = 1.5f;
    float originX_ = 0, originY_ = 0;
    enum class Drag { None, Move, Resize } drag_ = Drag::None;
    float grabX_ = 0, grabY_ = 0;     // move: mouse offset from the rect's corner (canvas px)
    int anchorX_ = 0, anchorY_ = 0;   // resize: the fixed corner
    float dragRatio_ = 0;             // resize: the width/height ratio kept (0 = free)
    std::vector<std::array<float, 4>> guides_;  // x1, y1, x2, y2 in canvas px
    float cardX_ = 0, cardY_ = 0, cardW_ = 236, cardH_ = 160;
    std::string artDir_;
    std::array<SDL_Texture*, idisp::kButtonCount> art_{};
    std::string toonPath_;
    SDL_Texture* toonTex_ = nullptr;
    bool lastWantText_ = false;  // a text field had focus last frame
    bool nameHot_ = false;       // name field hovered or active last frame
    bool cardVisible_ = false;
    const idisp::Settings* settings_ = nullptr;

    // Images
    std::string imagesDir_;
    std::map<std::string, SDL_Texture*> images_;  // by file name; nullptr = couldn't load
    std::string notice_;                          // shown under the canvas (e.g. a failed import)
    std::mutex dialogMutex_;                      // the file dialog may answer on another thread
    bool dialogOpen_ = false, dialogDone_ = false;
    std::string dialogPath_;
};
