// Input Display v5: entry point, input loop and frame pacing.
//
// Input is read about 1000 times a second and drawn once per display refresh.
// Every complete controller report is checked against the rules, so a hit
// that lasts a single report still shows on the next frame (see RuleMonitor).
#include "game_memory.hpp"
#include "idisp/game.hpp"
#include "idisp/layout.hpp"
#include "idisp/rules.hpp"
#include "idisp/settings.hpp"
#include "idisp/stick.hpp"
#include "image.hpp"
#include "layout_editor.hpp"
#include "overlay.hpp"
#include "settings_window.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;
using namespace idisp;

namespace {

enum ButtonIndex { kA, kB, kX, kY, kL, kR, kLT, kRT, kS, kUp, kDown, kLeft, kRight, kSelect, kL3, kR3 };

int buttonIndexFor(Uint8 b) {
    switch (b) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return kA;   // positions, not labels, as in v4.43
    case SDL_GAMEPAD_BUTTON_EAST: return kB;
    case SDL_GAMEPAD_BUTTON_WEST: return kX;
    case SDL_GAMEPAD_BUTTON_NORTH: return kY;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return kL;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return kR;
    case SDL_GAMEPAD_BUTTON_START: return kS;
    case SDL_GAMEPAD_BUTTON_BACK: return kSelect;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: return kL3;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return kR3;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return kUp;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return kDown;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return kLeft;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return kRight;
    default: return -1;
    }
}

// Profile names come from settings.ini; keep them to plain file names.
bool safeName(const std::string& n) {
    return !n.empty() && n.find_first_of("/\\:") == std::string::npos && n.find("..") == std::string::npos;
}

struct App {
    std::string base, assets, settingsPath;
    Settings settings;
    Profile profile;
    RuleMonitor monitor;
    StickAssembler stick;
    StickAssembler stickR;            // the right stick (drawn only; its own M-speed light)
    RuleMonitor monitorR;
    StickUnits unitsR;
    float smoothXR = 0, smoothYR = 0;
    Overlay overlay;
    SettingsWindow settingsWindow;
    LayoutEditor editor;
    LayoutFile presets;                 // assets/layouts.json (read-only)
    std::vector<Layout> userLayouts;    // layouts.user.json, listed after the presets
    std::string userLayoutsPath;
    int editIndex = -1;                 // layout open in the editor (index in the full list)
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Gamepad* pad = nullptr;
    SDL_JoystickID padId = 0;

    StickUnits units;                 // latest pad stick position (rules are checked on this)
    RawStick padRaw;                  // latest pad report, SDL values
    GameMemory game;

    // Probe CSV log: one row whenever the pad or the game values change.
    std::ofstream probeLog;
    std::string probeLogPath;
    uint64_t probeRows = 0, probeStartNs = 0, loggedGameChange = 0;
    bool padChanged = false;
    bool ruleLitNow = false;          // last frame's M-speed light, for the settings tile
    std::array<bool, idisp::kButtonCount> held{}, latched{};
    float smoothX = 0, smoothY = 0;
    std::string status;
    bool running = true;

    // ---- layouts ----
    const std::vector<Layout>& allLayouts() const { return overlay.layouts().layouts; }

    void publishLayouts() {
        LayoutFile f = presets;
        for (auto& l : f.layouts) l.preset = true;
        f.layouts.insert(f.layouts.end(), userLayouts.begin(), userLayouts.end());
        overlay.setLayouts(f);
        settings.buttonLayoutIndex = std::clamp(settings.buttonLayoutIndex, 0, (int)f.layouts.size() - 1);
        overlay.markDirty();
    }

    bool backupBeforeWrite = false;  // the user file had entries we couldn't read: keep a copy before overwriting it

    void loadUserLayouts() {
        userLayouts.clear();
        std::string text;
        if (!readFile(userLayoutsPath, text)) return;  // none yet
        std::vector<std::string> skipped;
        try {
            userLayouts = parseLayouts(text, true, &skipped).layouts;
            for (auto& l : userLayouts) l.preset = false;
        } catch (const std::exception& e) {
            skipped.push_back(e.what());
        }
        if (!skipped.empty()) {
            backupBeforeWrite = true;
            status = "layouts.user.json: " + std::to_string(skipped.size()) + " layout(s) couldn't be read and were left out (" +
                     skipped.front() + "). The file is backed up before it's next saved.";
            SDL_Log("%s", status.c_str());
        }
    }

    // Writes the user file (through a temp file, so a failed write leaves the
    // old one intact). Callers change their in-memory state only on success.
    bool saveUserLayouts(const std::vector<Layout>& next) {
        std::error_code ec;
        if (backupBeforeWrite) {
            fs::copy_file(userLayoutsPath, userLayoutsPath + ".bak", fs::copy_options::overwrite_existing, ec);
            if (ec) {
                status = "Not saved: couldn't back up layouts.user.json first (" + ec.message() + ")";
                SDL_Log("%s", status.c_str());
                return false;
            }
            backupBeforeWrite = false;
        }
        const std::string tmp = userLayoutsPath + ".tmp";
        if (writeFile(tmp, serializeLayouts(next))) {
            fs::rename(tmp, userLayoutsPath, ec);
            if (!ec) return true;
            fs::remove(tmp, ec);
        }
        status = "Could not save " + userLayoutsPath + "; nothing was changed";
        SDL_Log("%s", status.c_str());
        return false;
    }

    std::string imagesDir() const { return base + "images/"; }

    std::vector<std::string> layoutNames(int except = -1) const {
        std::vector<std::string> out;
        for (int i = 0; i < (int)allLayouts().size(); ++i)
            if (i != except) out.push_back(allLayouts()[i].name);
        return out;
    }

    void openEditor(int index) {
        if (index < 0 || index >= (int)allLayouts().size() || allLayouts()[index].preset) return;
        if (!editor.open(assets, allLayouts()[index])) return;
        editIndex = index;
        overlay.setEditing(&editor.working());
    }

    void closeEditor() {
        overlay.setEditing(nullptr);
        editor.close();
        editIndex = -1;
    }

    // A preset or custom layout copied into a new custom layout, then opened.
    void duplicateLayout(int index) {
        if (index < 0 || index >= (int)allLayouts().size() || editor.isOpen()) return;
        Layout copy = allLayouts()[index];
        copy.preset = false;
        const std::vector<std::string> names = layoutNames();
        std::string n = copy.name + " (copy)";
        for (int i = 2; std::find(names.begin(), names.end(), n) != names.end(); ++i)
            n = copy.name + " (copy " + std::to_string(i) + ")";
        copy.name = n;
        std::vector<Layout> next = userLayouts;
        next.push_back(copy);
        if (!saveUserLayouts(next)) return;
        userLayouts = std::move(next);
        publishLayouts();
        settings.buttonLayoutIndex = (int)allLayouts().size() - 1;
        openEditor(settings.buttonLayoutIndex);
    }

    void deleteLayout(int index) {
        const int u = index - (int)presets.layouts.size();
        if (u < 0 || u >= (int)userLayouts.size() || editor.isOpen()) return;
        std::vector<Layout> next = userLayouts;
        next.erase(next.begin() + u);
        if (!saveUserLayouts(next)) return;
        userLayouts = std::move(next);
        if (settings.buttonLayoutIndex >= index) settings.buttonLayoutIndex = std::max(0, settings.buttonLayoutIndex - 1);
        publishLayouts();
    }

    void drawEditor(const FrameInput& in) {
        LayoutEditorInput ei;
        ei.settings = &settings;
        ei.drawX = in.drawX;
        ei.drawY = in.drawY;
        ei.buttons = in.buttons;
        ei.lit = in.ruleLit && settings.showRuleIndicator;
        ei.drawXR = in.drawXR;
        ei.drawYR = in.drawYR;
        ei.litR = in.ruleLitR && settings.showRuleIndicator;
        ei.imagesDir = imagesDir();
        ei.otherNames = layoutNames(editIndex);
        if (!overlay.buttonSets().empty())
            ei.buttonSetDir = assets + "sets/" +
                              overlay.buttonSets()[std::clamp(settings.buttonSetIndex, 0, (int)overlay.buttonSets().size() - 1)] + "/";
        const LayoutEditorResult r = editor.frame(ei);
        if (r.save) {
            const int u = editIndex - (int)presets.layouts.size();
            if (u >= 0 && u < (int)userLayouts.size()) {
                std::vector<Layout> next = userLayouts;
                next[u] = editor.working();
                next[u].preset = false;
                if (!saveUserLayouts(next)) return;  // keep the editor open so nothing is lost
                userLayouts = std::move(next);
                status = "Saved layout \"" + userLayouts[u].name + "\"" + (r.renamed ? " (that name was taken, so a number was added)" : "");
            }
            closeEditor();
            publishLayouts();
        } else if (r.cancel) {
            closeEditor();
        }
    }

    std::vector<std::string> listProfiles() const {
        std::vector<std::string> out;
        std::error_code ec;
        for (const auto& e : fs::directory_iterator(assets + "profiles", ec))
            if (e.path().extension() == ".json") out.push_back(e.path().stem().string());
        std::sort(out.begin(), out.end());
        return out;
    }

    void loadProfile() {
        profile = Profile{};
        std::string text;
        const std::string path = assets + "profiles/" + settings.profile + ".json";
        if (!safeName(settings.profile) || !readFile(path, text)) {
            status = "Rule profile not found: " + settings.profile;
        } else {
            try {
                profile = parseProfile(text);
                status.clear();
            } catch (const std::exception& e) {
                status = "Rule profile " + settings.profile + " has an error: " + e.what();
            }
        }
        if (!status.empty()) SDL_Log("%s", status.c_str());
    }

    void loadGames() {
        std::vector<GameDef> defs;
        std::error_code ec;
        for (const auto& e : fs::directory_iterator(assets + "games", ec)) {
            if (e.path().extension() != ".json") continue;
            std::string text;
            if (!readFile(e.path().string(), text)) continue;
            try {
                defs.push_back(parseGameDef(text));
            } catch (const std::exception& ex) {
                status = "Game definition " + e.path().filename().string() + " has an error: " + ex.what();
                SDL_Log("%s", status.c_str());
            }
        }
        game.setDefinitions(std::move(defs));
    }

    // What the overlay draws: the game's value when asked for and readable.
    bool gameStickReadable() const { return game.value("gameStickX") && game.value("gameStickY"); }
    bool showingGame() const { return settings.showGameStick && gameStickReadable(); }
    // The game's stick as stored, in stick units (-128..128). 0,0 if unreadable.
    StickUnits gameUnits() const {
        if (!gameStickReadable()) return {};
        return { (float)*game.value("gameStickX"), (float)*game.value("gameStickY") };
    }
    StickUnits shownUnits() const {
        if (!showingGame()) return units;
        StickUnits g = gameUnits();
        if (settings.gameApplyDeadzone) {
            const double d = settings.gameDeadzone;
            g.x = (float)(padLikeFromGame(g.x / 128.0, d) * 128.0);
            g.y = (float)(padLikeFromGame(g.y / 128.0, d) * 128.0);
        }
        return g;
    }

    // M-speed: one ring on the game's stick value. The game value is checked
    // as stored (deadzone already applied by the game); the pad is sent
    // through the same per-axis deadzone first (padInGame), so both sources
    // use one rule. Two monitors so each keeps its own latch.
    StickUnits padInGame(StickUnits u) const {
        const double d = settings.gameDeadzone;
        return { (float)(gameFromPad(u.x / 128.0, d) * 128.0), (float)(gameFromPad(u.y / 128.0, d) * 128.0) };
    }
    RuleMonitor gameMonitor;
    uint64_t gameSampledChange = ~0ull;
    void rebuildGameRule() {
        Region ring;
        ring.name = "ring";
        ring.ring = true;
        // min above max (sliders or settings.ini) would never match: use the pair in order
        ring.rMin = std::min(settings.gameRingMin, settings.gameRingMax) * 128.0f;
        ring.rMax = std::max(settings.gameRingMin, settings.gameRingMax) * 128.0f;
        Profile p;
        p.name = "Game value ring";
        p.rules.push_back({ "M-speed", profile.rules.empty() ? "M" : profile.rules[0].label, { ring } });
        gameMonitor.setProfile(p);
        monitor.setProfile(p);
        monitorR.setProfile(p);
        monitor.onSample(SDL_GetTicksNS(), padInGame(units));  // re-check the current pad value
        monitorR.onSample(SDL_GetTicksNS(), padInGame(unitsR));
        gameSampledChange = ~0ull;  // re-check the current game value
    }
    void sampleGame(uint64_t nowNs) {
        if (game.changeCount() == gameSampledChange) return;
        gameSampledChange = game.changeCount();
        gameMonitor.onSample(nowNs, gameUnits());
    }

    void toggleProbeLog() {
        if (probeLog.is_open()) {
            probeLog.close();
            status = "Saved " + probeLogPath;
            return;
        }
        char stamp[32];
        const std::time_t t = std::time(nullptr);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &tm);
        probeLogPath = base + "probe-" + stamp + ".csv";
        probeLog.open(probeLogPath);
        if (!probeLog) {
            status = "Could not create " + probeLogPath;
            return;
        }
        probeRows = 0;
        probeStartNs = SDL_GetTicksNS();
        probeLog << "ms,padRawX,padRawY,padX,padY,padRingHit,gameRingHit,attached";
        // Every value from the bundled definitions, so new ones (e.g. a speed
        // value added to the JSON) are logged without code changes.
        probeColumns.clear();
        if (const GameDef* g = game.game())
            for (const auto& v : g->versions)
                for (const auto& [name, _] : v.values)
                    if (std::find(probeColumns.begin(), probeColumns.end(), name) == probeColumns.end()) probeColumns.push_back(name);
        if (probeColumns.empty()) probeColumns = { "gameStickX", "gameStickY" };
        for (const auto& c : probeColumns) probeLog << ',' << c;
        probeLog << '\n';
        padChanged = true;
    }
    std::vector<std::string> probeColumns;

    void logProbe(uint64_t nowNs) {
        if (!probeLog.is_open()) return;
        if (!padChanged && game.changeCount() == loggedGameChange) return;
        padChanged = false;
        loggedGameChange = game.changeCount();
        char buf[160];
        std::snprintf(buf, sizeof buf, "%.3f,%d,%d,%.4f,%.4f,%d,%d,%d", (nowNs - probeStartNs) / 1e6, padRaw.x, padRaw.y,
                      units.x, units.y, monitor.activeNow(0) ? 1 : 0, gameMonitor.activeNow(0) ? 1 : 0,
                      game.attached() ? 1 : 0);
        probeLog << buf;
        for (const auto& c : probeColumns) {
            probeLog << ',';
            if (auto v = game.value(c)) {
                std::snprintf(buf, sizeof buf, "%.9g", *v);
                probeLog << buf;
            }
        }
        probeLog << '\n';
        ++probeRows;
    }

    void saveSettings() {
        SDL_GetWindowSize(window, &settings.overlayWidth, &settings.overlayHeight);
        if (!writeFile(settingsPath, serializeSettings(settings))) SDL_Log("Could not save %s", settingsPath.c_str());
    }

    void openFirstPad() {
        int count = 0;
        SDL_JoystickID* ids = SDL_GetGamepads(&count);
        for (int i = 0; i < count && !pad; ++i) openPad(ids[i]);
        SDL_free(ids);
    }

    void openPad(SDL_JoystickID id) {
        if (pad) return;
        pad = SDL_OpenGamepad(id);
        if (!pad) return;
        padId = id;
        stick.reset({ SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX), SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY) });
        onReport(SDL_GetTicksNS(), stick.current());
        stickR.reset({ SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX), SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY) });
        onReportR(SDL_GetTicksNS(), stickR.current());
        SDL_Log("Controller connected: %s", SDL_GetGamepadName(pad));
    }

    void closePad() {
        if (pad) SDL_CloseGamepad(pad);
        pad = nullptr;
        padId = 0;
        held.fill(false);
        stick.reset({});
        onReport(SDL_GetTicksNS(), {});
        stickR.reset({});
        onReportR(SDL_GetTicksNS(), {});
    }

    // One complete controller report (or a d-pad change in d-pad mode).
    void onReport(uint64_t t, RawStick raw) {
        StickUnits u = toUnitsLegacy(raw);
        if (settings.dpadMovesStick) u = applyDpad(u, held[kUp], held[kDown], held[kLeft], held[kRight]);
        units = u;
        padRaw = raw;
        padChanged = true;
        monitor.onSample(t, padInGame(u));
    }

    // The right stick: the same ring rule on its pad value (the game only
    // checks the left stick; this light is for custom layouts).
    void onReportR(uint64_t t, RawStick raw) {
        unitsR = toUnitsLegacy(raw);
        monitorR.onSample(t, padInGame(unitsR));
    }

    void flushStick() {
        if (auto r = stick.flush()) onReport(r->timestampNs, r->stick);
        if (auto r = stickR.flush()) onReportR(r->timestampNs, r->stick);
    }

    void setButton(int i, bool down) {
        if (i < 0) return;
        held[i] = down;
        if (down) latched[i] = true;  // a press shorter than a frame still shows for one frame
    }

    bool hoverSettings() const {
        return window && SDL_GetMouseFocus() == window && (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS);
    }

    void drawSettingsAffordance() {
        if (!hoverSettings()) return;
        // Overlay::render uses an off-screen canvas.  Explicitly restore the
        // window target and its native coordinate system before adding chrome;
        // otherwise the control is drawn into the canvas and enlarged with it.
        SDL_SetRenderTarget(renderer, nullptr);
        SDL_SetRenderScale(renderer, 1.0f, 1.0f);
        SDL_SetRenderViewport(renderer, nullptr);
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 120);
        SDL_FRect veil{ 0, 0, (float)w, (float)h };
        SDL_RenderFillRect(renderer, &veil);
        const float size = 46, x = w - size - 14, y = 14;
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 230);
        SDL_FRect button{ x, y, size, size };
        SDL_RenderFillRect(renderer, &button);
        SDL_SetRenderDrawColor(renderer, 18, 42, 78, 255);
        const float cx = x + size * .5f, cy = y + size * .5f;
        // A compact gear: eight teeth, a ring, and its center hole.
        for (int i = 0; i < 8; ++i) {
            const float a = i * 3.14159265f / 4.0f;
            const float x1 = cx + std::cos(a) * 10, y1 = cy + std::sin(a) * 10;
            const float x2 = cx + std::cos(a) * 15, y2 = cy + std::sin(a) * 15;
            SDL_RenderLine(renderer, x1, y1, x2, y2);
        }
        for (int r = 8; r <= 10; ++r)
            for (int i = 0; i < 24; ++i) {
                const float a = i * 3.14159265f / 12.0f, b = (i + 1) * 3.14159265f / 12.0f;
                SDL_RenderLine(renderer, cx + std::cos(a) * r, cy + std::sin(a) * r,
                               cx + std::cos(b) * r, cy + std::sin(b) * r);
            }
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_FRect hole{ cx - 3, cy - 3, 6, 6 };
        SDL_RenderFillRect(renderer, &hole);
    }

    bool clickedSettingsAffordance(const SDL_Event& e) const {
        if (e.type != SDL_EVENT_MOUSE_BUTTON_DOWN || e.button.button != SDL_BUTTON_LEFT || SDL_GetWindowFromEvent(&e) != window || !hoverSettings()) return false;
        int w = 0;
        SDL_GetWindowSizeInPixels(window, &w, nullptr);
        return e.button.x >= w - 60 && e.button.x <= w - 14 && e.button.y >= 14 && e.button.y <= 60;
    }

    void onEvent(const SDL_Event& e) {
        if (editor.processEvent(e)) return;
        if (settingsWindow.processEvent(e)) return;
        switch (e.type) {
        case SDL_EVENT_QUIT:
            running = false;
            break;
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if (SDL_GetWindowFromEvent(&e) == window) running = false;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (clickedSettingsAffordance(e)) settingsWindow.open(assets);
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
            openPad(e.gdevice.which);
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            if (e.gdevice.which == padId) {
                closePad();
                openFirstPad();
            }
            break;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            if (e.gaxis.which != padId) break;
            if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX || e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) {
                if (auto r = stick.push(e.gaxis.timestamp, e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY, e.gaxis.value))
                    onReport(r->timestampNs, r->stick);
            } else if (e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTX || e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTY) {
                if (auto r = stickR.push(e.gaxis.timestamp, e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTY, e.gaxis.value))
                    onReportR(r->timestampNs, r->stick);
            } else if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) {
                setButton(kLT, e.gaxis.value >= 16384);  // v4.43: (v + 1) / 32768 > 0.5
            } else if (e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
                setButton(kRT, e.gaxis.value >= 16384);
            }
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP: {
            if (e.gbutton.which != padId) break;
            const int i = buttonIndexFor(e.gbutton.button);
            setButton(i, e.gbutton.down);
            if (settings.dpadMovesStick && i >= kUp && i <= kRight) {
                flushStick();
                onReport(e.gbutton.timestamp, stick.current());
            }
            break;
        }
        default:
            break;
        }
    }

    void drawFrame(uint64_t nowNs) {
        FrameInput in;
        const StickUnits shown = shownUnits();
        in.unitsX = shown.x;
        in.unitsY = shown.y;
        if (settings.showSourceNote && settings.showGameStick && !gameStickReadable()) in.sourceNote = "no supported games detected";
        // Both monitors are asked every frame so neither keeps a stale latch;
        // the light follows whichever stick is drawn.
        const bool padLit = monitor.activeForFrame(0, nowNs, settings.hitHoldMs);
        const bool gameLit = gameMonitor.activeForFrame(0, nowNs, settings.hitHoldMs);
        in.ruleLit = showingGame() ? gameLit : padLit;
        in.ruleLitR = monitorR.activeForFrame(0, nowNs, settings.hitHoldMs);
        ruleLitNow = in.ruleLit;
        if (!profile.rules.empty()) in.ruleLabel = profile.rules[0].label;
        const int rawX = (int)shown.x, rawY = (int)shown.y;
        if (settings.stickSmoothing) {  // v4.43 behavior: 70% of the way each frame
            smoothX += (rawX - smoothX) * 0.7f;
            smoothY += (rawY - smoothY) * 0.7f;
            in.drawX = (int)smoothX;
            in.drawY = (int)smoothY;
        } else {
            smoothX = (float)rawX;
            smoothY = (float)rawY;
            in.drawX = rawX;
            in.drawY = rawY;
        }
        if (settings.stickSmoothing) {
            smoothXR += ((int)unitsR.x - smoothXR) * 0.7f;
            smoothYR += ((int)unitsR.y - smoothYR) * 0.7f;
        } else {
            smoothXR = (float)(int)unitsR.x;
            smoothYR = (float)(int)unitsR.y;
        }
        in.drawXR = (int)smoothXR;
        in.drawYR = (int)smoothYR;
        for (size_t i = 0; i < held.size(); ++i) in.buttons[i] = held[i] || latched[i];
        latched.fill(false);

        overlay.render(settings, in);
        drawSettingsAffordance();
        SDL_RenderPresent(renderer);

        if (editor.isOpen()) drawEditor(in);
        if (settingsWindow.isOpen()) drawSettings();
    }

    void drawSettings() {
        SettingsUiInput ui;
        ui.profiles = listProfiles();
        ui.buttonSets = &overlay.buttonSets();
        ui.layouts = &overlay.layouts();
        SDL_GetWindowSize(window, &ui.overlayW, &ui.overlayH);
        {
            const Layout& cur = overlay.current(settings);
            ui.overlayZoom = std::min((float)ui.overlayW / cur.width, (float)ui.overlayH / cur.height);
        }
        ui.editorOpen = editor.isOpen();
        ui.showingGame = showingGame();
        ui.gameMissing = settings.showGameStick && !gameStickReadable();
        ui.ruleLit = ruleLitNow;
        {
            const double d = settings.gameDeadzone;
            StickUnits g = ui.showingGame ? gameUnits() : padInGame(units);
            ui.gameDistance = std::hypot(g.x / 128.0, g.y / 128.0);
            if (ui.showingGame) {
                ui.lcdX = (float)padLikeFromGame(g.x / 128.0, d);
                ui.lcdY = (float)padLikeFromGame(g.y / 128.0, d);
            } else {
                ui.lcdX = units.x / 128.0f;
                ui.lcdY = units.y / 128.0f;
            }
        }
        ui.unitsX = units.x;
        ui.unitsY = units.y;
        ui.controllerConnected = pad != nullptr;
        if (pad) ui.controllerName = SDL_GetGamepadName(pad) ? SDL_GetGamepadName(pad) : "Unknown";
        ui.status = status;
        GameProbeInput& g = ui.probe;
        g.attached = game.attached();
        g.message = game.message();
        const GameMemory::Info& gi = game.info();
        g.game = gi.gameName;
        g.process = gi.process;
        g.version = gi.versionLabel;
        g.pid = gi.pid;
        g.timeDateStamp = gi.timeDateStamp;
        g.sizeOfImage = gi.sizeOfImage;
        g.exactBuild = gi.exactBuild;
        for (const auto& [name, v] : game.values()) g.values.emplace_back(name, v);
        g.padRawX = padRaw.x;
        g.padRawY = padRaw.y;
        g.padX = units.x;
        g.padY = units.y;
        g.logging = probeLog.is_open();
        g.logPath = probeLogPath;
        g.logRows = probeRows;

        SettingsUiResult r = settingsWindow.frame(settings, profile, ui);
        if (r.settingsChanged) {
            overlay.markDirty();
            rebuildGameRule();
        }
        if (r.resetOverlaySize) {
            const Layout& cur = overlay.current(settings);
            SDL_SetWindowSize(window, cur.width, cur.height);
        }
        if (r.duplicateLayout >= 0) duplicateLayout(r.duplicateLayout);
        if (r.editLayout >= 0) openEditor(r.editLayout);
        if (r.deleteLayout >= 0) deleteLayout(r.deleteLayout);
        if (r.assetsChanged) {
            overlay.loadButtonSet(settings.buttonSetIndex);
        }
        if (r.profileSelected) {
            loadProfile();
            rebuildGameRule();
        }
        if (r.toggleLogging) toggleProbeLog();
        if (r.reloadGames) loadGames();
        if (r.saveSettings) {
            saveSettings();
            status = "Saved " + settingsPath;
        }
    }
};

// --demo: a virtual controller that walks the stick around the M-speed
// band, so the whole pipeline can be checked (and skins previewed) without a
// physical controller. Input goes through the same SDL events as a real pad.
struct Demo {
    SDL_Joystick* joy = nullptr;

    void start() {
        SDL_VirtualJoystickDesc d;
        SDL_INIT_INTERFACE(&d);
        d.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        d.naxes = SDL_GAMEPAD_AXIS_COUNT;
        d.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        d.name = "Demo controller";
        SDL_JoystickID id = SDL_AttachVirtualJoystick(&d);
        joy = id ? SDL_OpenJoystick(id) : nullptr;
        if (!joy) SDL_Log("Demo controller failed: %s", SDL_GetError());
    }

    void tick(uint64_t nowNs) {
        if (!joy) return;
        const double t = nowNs / 1e9;
        // First 3 s: hold straight up at 39 units (inside the "north" region).
        // Then circle at radius 39 units, about one turn every 8 seconds.
        const double a = t < 3 ? 1.5707963 : 1.5707963 + (t - 3) * 0.785398;
        const double radiusRaw = 39.0 * 256;
        SDL_SetJoystickVirtualAxis(joy, SDL_GAMEPAD_AXIS_LEFTX, (Sint16)(radiusRaw * std::cos(a)));
        SDL_SetJoystickVirtualAxis(joy, SDL_GAMEPAD_AXIS_LEFTY, (Sint16)(-radiusRaw * std::sin(a)));
        SDL_SetJoystickVirtualButton(joy, SDL_GAMEPAD_BUTTON_SOUTH, ((int)(t * 2) % 2) == 0);
    }

    void stop() {
        if (joy) SDL_CloseJoystick(joy);
        joy = nullptr;
    }
};

void setInputHints(const Settings& s) {
    // The game has focus while we run, so we must keep receiving input.
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
#ifdef _WIN32
    // OBS Game Capture hooks Direct3D 11 reliably (v4.43 also forced it).
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d11");
    if (s.windowsXInputOnly) {
        // Read Xbox-style pads through XInput, the API most PC games (including
        // Sonic Generations) use, so we see the same values the game sees.
        SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "0");
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_XBOX, "0");
        SDL_SetHint(SDL_HINT_JOYSTICK_WGI, "0");
#ifdef SDL_HINT_JOYSTICK_GAMEINPUT
        SDL_SetHint(SDL_HINT_JOYSTICK_GAMEINPUT, "0");
#endif
    }
#else
    (void)s;
#endif
}

uint64_t frameIntervalNs(SDL_Window* w) {
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(w));
    float hz = (mode && mode->refresh_rate > 0) ? mode->refresh_rate : 60.0f;
    return (uint64_t)(1e9 / hz);
}

} // namespace

int main(int argc, char** argv) {
    App app;
    bool demoMode = false, openSettings = false;
    int editLayoutAtStart = -1;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--demo") == 0) demoMode = true;
        if (std::strcmp(argv[i], "--settings") == 0) openSettings = true;  // open the settings window at start
        if (std::strcmp(argv[i], "--edit-layout") == 0 && i + 1 < argc) editLayoutAtStart = std::atoi(argv[++i]);  // open a custom layout in the editor
    }
    const char* basePath = SDL_GetBasePath();
    app.base = basePath ? basePath : "./";
    app.assets = app.base + "assets/";
    app.settingsPath = app.base + "settings.ini";
    app.userLayoutsPath = app.base + "layouts.user.json";

    std::string text;
    if (readFile(app.settingsPath, text)) app.settings = parseSettings(text);

    setInputHints(app.settings);
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    LayoutFile layouts;
    try {
        if (!readFile(app.assets + "layouts.json", text)) throw std::runtime_error("assets/layouts.json not found");
        layouts = parseLayouts(text);
    } catch (const std::exception& e) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Input Display", e.what(), nullptr);
        SDL_Quit();
        return 1;
    }

    // Same title as v4.43 so existing OBS sources can find the window by name.
    const SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | (app.settings.transparentWindow ? SDL_WINDOW_TRANSPARENT : 0);
    const bool savedSize = app.settings.overlayWidth > 0 && app.settings.overlayHeight > 0;
    app.window = SDL_CreateWindow("Controller Input", savedSize ? app.settings.overlayWidth : layouts.width,
                                  savedSize ? app.settings.overlayHeight : layouts.height, flags);
    if (app.window) SDL_SetWindowMinimumSize(app.window, layouts.width / 4, layouts.height / 4);
    app.renderer = app.window ? SDL_CreateRenderer(app.window, nullptr) : nullptr;
    if (!app.renderer) {
        SDL_Log("Window/renderer creation failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_Log("Renderer: %s", SDL_GetRendererName(app.renderer));

    app.overlay.init(app.renderer, app.assets);
    app.overlay.setImagesDir(app.imagesDir());
    app.presets = layouts;
    app.loadUserLayouts();
    app.publishLayouts();
    app.overlay.loadButtonSet(app.settings.buttonSetIndex);
    app.loadProfile();
    app.loadGames();
    app.rebuildGameRule();
    Demo demo;
    if (demoMode) demo.start();
    if (openSettings) app.settingsWindow.open(app.assets);
    if (editLayoutAtStart >= 0) app.openEditor(editLayoutAtStart);
    app.openFirstPad();

    const uint64_t interval = frameIntervalNs(app.window);
    uint64_t nextFrame = SDL_GetTicksNS();
    while (app.running) {
        if (demoMode) demo.tick(SDL_GetTicksNS());
        SDL_Event e;
        while (SDL_PollEvent(&e)) app.onEvent(e);  // polling also reads the controller
        app.flushStick();
        app.game.poll(SDL_GetTicksNS());
        app.sampleGame(SDL_GetTicksNS());
        app.logProbe(SDL_GetTicksNS());

        const uint64_t now = SDL_GetTicksNS();
        if (now >= nextFrame) {
            app.drawFrame(now);
            nextFrame += interval;
            if (now > nextFrame + interval) nextFrame = now + interval;  // fell behind; don't spiral
        } else {
            SDL_DelayPrecise(std::min<uint64_t>(nextFrame - now, 1'000'000));  // <= 1 ms between input reads
        }
    }

    app.saveSettings();
    app.closeEditor();
    app.settingsWindow.close();
    app.closePad();
    demo.stop();
    app.overlay.shutdown();
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    return 0;
}
