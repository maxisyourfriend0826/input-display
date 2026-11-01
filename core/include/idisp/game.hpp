// Game definitions (assets/games/*.json): where a game keeps its input values
// in memory, per game build. No OS calls here; the app supplies a memory
// reader, so pointer-path logic can be unit-tested.
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace idisp {

// Cheat Engine notation: "Module.exe+0x1E8C190", then offsets to follow.
// Every element except the last is dereferenced (read a pointer, add the next
// offset), exactly like a CE pointer entry. One element = a static address.
struct PointerPath {
    std::string module;            // as written; matched case-insensitively
    uint64_t moduleOffset = 0;
    std::vector<int64_t> offsets;  // offsets after the first element
};

enum class ValueType { F32, F64, I8, U8, I16, U16, I32, U32 };

struct GameValue {
    PointerPath path;
    ValueType type = ValueType::F32;
    double scale = 1.0;            // multiply the stored value by this
};

struct GameVersion {
    std::string label;
    // Exe header fields that identify the exact build. A version without them
    // matches any build, and is reported as unverified.
    std::optional<uint32_t> timeDateStamp, sizeOfImage;
    std::map<std::string, GameValue> values;  // e.g. gameStickX, gameStickY, frame
};

struct GameDef {
    std::string id, name, process, profile;
    std::vector<GameVersion> versions;
};

// Throws std::runtime_error with a readable message on bad input.
GameDef parseGameDef(const std::string& json);
PointerPath parsePointerPath(const std::vector<std::string>& parts);

struct VersionMatch {
    const GameVersion* version = nullptr;
    bool exact = false;  // false = matched a version with no build fields
};
// Exact build match first, then the first version without build fields.
VersionMatch matchVersion(const GameDef& def, uint32_t timeDateStamp, uint32_t sizeOfImage);

// Reads n bytes at a target-process address; false on failure.
using ReadMemory = std::function<bool(uint64_t address, void* out, size_t n)>;
// Base address of a module in the target process, or nullopt.
using ModuleBase = std::function<std::optional<uint64_t>(const std::string& module)>;

std::optional<uint64_t> resolveAddress(const PointerPath& p, const ModuleBase& base, const ReadMemory& read,
                                       bool pointers64 = true);
// Undo a per-axis deadzone for drawing, so the game's stick looks like the
// pad's: the game stores g = (|p| - d) / (1 - d) (SXSG fit, d about 0.265),
// so p = |g| (1 - d) + d. A 0 stays 0: inside the deadzone the game keeps no
// position. g and the result are -1..1.
inline double padLikeFromGame(double g, double d) {
    if (g == 0) return 0;
    const double p = (g < 0 ? -g : g) * (1 - d) + d;
    return g < 0 ? -p : p;
}

// What the game stores for a pad axis value p (-1..1): g = (|p| - d) / (1 - d),
// 0 inside the deadzone, capped at 1. Fits SXSG probe logs to about 0.001
// at every stick sensitivity (d = 0.265; mspeed-research/NOTES.md).
inline double gameFromPad(double p, double d) {
    const double a = p < 0 ? -p : p;
    if (a <= d) return 0;
    const double g = a >= 1 ? 1 : (a - d) / (1 - d);
    return p < 0 ? -g : g;
}

std::optional<double> readGameValue(const GameValue& v, const ModuleBase& base, const ReadMemory& read,
                                    bool pointers64 = true);

} // namespace idisp
