// Reads a running game's input values from its memory, read-only, the same
// way LiveSplit autosplitters do. Windows only for now; elsewhere it reports
// that it is unavailable.
#pragma once
#include "idisp/game.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class GameMemory {
public:
    GameMemory();
    ~GameMemory();
    GameMemory(const GameMemory&) = delete;
    GameMemory& operator=(const GameMemory&) = delete;

    // Replaces the known games and detaches.
    void setDefinitions(std::vector<idisp::GameDef> defs);

    // Call often (every input loop). Looks for a known game about once a
    // second while detached; while attached, reads every value.
    void poll(uint64_t nowNs);

    bool attached() const;
    // Latest values by name (e.g. "gameStickX"), already scaled. A value
    // missing from the map could not be read this poll.
    const std::map<std::string, double>& values() const { return values_; }
    std::optional<double> value(const std::string& name) const;
    // Increments whenever any value changes.
    uint64_t changeCount() const { return changes_; }

    struct Info {
        std::string gameName, process, versionLabel;
        uint32_t pid = 0, timeDateStamp = 0, sizeOfImage = 0;
        bool exactBuild = false;
    };
    const Info& info() const { return info_; }
    const std::string& message() const { return message_; }  // why we're not reading, or ""
    const idisp::GameDef* game() const;

private:
    void detach(const std::string& why);
    void tryAttach();

    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::vector<idisp::GameDef> defs_;
    std::map<std::string, double> values_;
    Info info_;
    std::string message_;
    uint64_t nextAttachNs_ = 0, nextAliveCheckNs_ = 0, changes_ = 0;
};
