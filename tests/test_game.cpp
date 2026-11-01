// Game definitions and pointer paths, against a fake process memory.
#include "doctest/doctest.h"

#include "idisp/game.hpp"

#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>

using namespace idisp;

namespace {

// Sparse fake memory: address -> bytes.
struct FakeMemory {
    std::map<uint64_t, unsigned char> bytes;
    template <class T> void put(uint64_t a, T v) {
        unsigned char b[sizeof(T)];
        std::memcpy(b, &v, sizeof(T));
        for (size_t i = 0; i < sizeof(T); ++i) bytes[a + i] = b[i];
    }
    ReadMemory reader() {
        return [this](uint64_t a, void* out, size_t n) {
            for (size_t i = 0; i < n; ++i) {
                auto it = bytes.find(a + i);
                if (it == bytes.end()) return false;
                static_cast<unsigned char*>(out)[i] = it->second;
            }
            return true;
        };
    }
};

ModuleBase modules(uint64_t exeBase) {
    return [exeBase](const std::string& m) -> std::optional<uint64_t> {
        if (m == "game.exe") return exeBase;
        return std::nullopt;
    };
}

} // namespace

TEST_CASE("pointer paths parse in Cheat Engine notation") {
    PointerPath p = parsePointerPath({ "SONIC_GENERATIONS.exe+1E8C190" });
    CHECK(p.module == "SONIC_GENERATIONS.exe");
    CHECK(p.moduleOffset == 0x1E8C190);
    CHECK(p.offsets.empty());

    p = parsePointerPath({ "game.exe + 0x10", "0x20", "-8", "1C" });
    CHECK(p.moduleOffset == 0x10);
    REQUIRE(p.offsets.size() == 3);
    CHECK(p.offsets[0] == 0x20);
    CHECK(p.offsets[1] == -8);
    CHECK(p.offsets[2] == 0x1C);

    CHECK_THROWS(parsePointerPath({}));
    CHECK_THROWS(parsePointerPath({ "0x1234" }));
    CHECK_THROWS(parsePointerPath({ "game.exe+zz" }));
}

TEST_CASE("static address and multi-level pointer resolve like Cheat Engine") {
    FakeMemory mem;
    const uint64_t base = 0x140000000;
    mem.put<float>(base + 0x1E8C190, 0.25f);

    GameValue v;
    v.path = parsePointerPath({ "game.exe+1E8C190" });
    v.scale = 128;
    auto r = readGameValue(v, modules(base), mem.reader());
    REQUIRE(r);
    CHECK(*r == doctest::Approx(32.0));

    // [[base+0x100]+0x20]+0x8 -> int32
    mem.put<uint64_t>(base + 0x100, 0x20000000);
    mem.put<uint64_t>(0x20000020, 0x30000000);
    mem.put<int32_t>(0x30000008, -77);
    v.path = parsePointerPath({ "game.exe+100", "20", "8" });
    v.type = ValueType::I32;
    v.scale = 1;
    r = readGameValue(v, modules(base), mem.reader());
    REQUIRE(r);
    CHECK(*r == -77);

    // A null pointer along the way (e.g. in a menu) gives no value, not garbage.
    mem.put<uint64_t>(0x20000020, 0);
    CHECK_FALSE(readGameValue(v, modules(base), mem.reader()));
    // Unknown module.
    v.path.module = "other.dll";
    CHECK_FALSE(readGameValue(v, modules(base), mem.reader()));
}

TEST_CASE("versions match by exe build, with unverified fallback") {
    const char* text = R"({
      "id": "g", "name": "G", "process": "game.exe",
      "versions": [
        { "label": "v1", "match": { "timeDateStamp": "0x11111111", "sizeOfImage": "0x2000" },
          "values": { "gameStickX": { "path": ["game.exe+10"], "type": "float", "scale": 128 } } },
        { "label": "any", "values": { "gameStickX": { "path": ["game.exe+20"] } } }
      ] })";
    GameDef d = parseGameDef(text);
    CHECK(d.versions.size() == 2);
    auto m = matchVersion(d, 0x11111111, 0x2000);
    REQUIRE(m.version);
    CHECK(m.version->label == "v1");
    CHECK(m.exact);
    m = matchVersion(d, 0x22222222, 0x2000);
    REQUIRE(m.version);
    CHECK(m.version->label == "any");
    CHECK_FALSE(m.exact);

    d.versions.pop_back();
    CHECK_FALSE(matchVersion(d, 0x22222222, 0x2000).version);
}

TEST_CASE("bad game definitions give readable errors") {
    CHECK_THROWS_WITH(parseGameDef("{"), doctest::Contains("not valid JSON"));
    CHECK_THROWS_WITH(parseGameDef(R"({"id":"g","name":"G","versions":[]})"), doctest::Contains("process"));
    CHECK_THROWS_WITH(parseGameDef(R"({"id":"g","name":"G","process":"g.exe","versions":[{"values":{"x":{"path":["g.exe+1"],"type":"quad"}}}]})"),
                      doctest::Contains("unknown value type"));
}

TEST_CASE("bundled game definitions parse") {
    std::ifstream f(std::string(IDISP_ASSETS_DIR) + "/games/sonic-x-shadow-generations.json");
    REQUIRE(f);
    std::stringstream ss;
    ss << f.rdbuf();
    GameDef d = parseGameDef(ss.str());
    CHECK(d.process == "SONIC_GENERATIONS.exe");
    REQUIRE(!d.versions.empty());
    CHECK(d.versions[0].values.count("gameStickX"));
    CHECK(d.versions[0].values.count("gameStickY"));
}

#include "idisp/rules.hpp"

TEST_CASE("adding the deadzone back undoes the game's per-axis deadzone") {
    const double d = 0.265;
    for (double p : { 0.27, 0.3, 0.5, 0.9, 1.0, -0.3, -1.0 }) {
        const double g = (p < 0 ? -1 : 1) * ((p < 0 ? -p : p) - d) / (1 - d);  // what the game stores
        CHECK(padLikeFromGame(g, d) == doctest::Approx(p));
    }
    CHECK(padLikeFromGame(0.0, d) == 0.0);  // inside the deadzone: no position kept
}

TEST_CASE("the pad value goes through the game's per-axis deadzone") {
    const double d = 0.265;
    CHECK(gameFromPad(0.2, d) == 0.0);                       // inside the deadzone
    CHECK(gameFromPad(-0.265, d) == 0.0);
    CHECK(gameFromPad(1.0, d) == doctest::Approx(1.0));
    CHECK(gameFromPad(-1.0, d) == doctest::Approx(-1.0));
    for (double g : { 0.03, 0.077, 0.5, -0.4 })               // inverse of padLikeFromGame
        CHECK(gameFromPad(padLikeFromGame(g, d), d) == doctest::Approx(g));
}

TEST_CASE("M-speed ring on the pad, through the deadzone, matches the v4.43 band") {
    // Ring 0.03..0.077 in game values, deadzone 0.265 (fit to probe logs
    // 2026-10-31). Pad points in stick units (-128..128).
    auto inRing = [](double x, double y) {
        const double gx = gameFromPad(x / 128, 0.265), gy = gameFromPad(y / 128, 0.265);
        const double r = std::sqrt(gx * gx + gy * gy);
        return r >= 0.03 && r <= 0.077;
    };
    CHECK(inRing(0, 37));    // v4.43 north band, 37..41.17
    CHECK(inRing(0, 41));
    CHECK(!inRing(0, 36));
    CHECK(!inRing(0, 42));
    CHECK(inRing(-38, 0));   // west band
    CHECK(inRing(36, 36));   // v4.43 corner box, 36..37.99
    CHECK(inRing(-37, -37));
    CHECK(!inRing(34, 34));  // diagonal, still in the deadzone
    CHECK(!inRing(39.5, 39.5));
}

TEST_CASE("ring regions hit by distance from centre, and round-trip through JSON") {
    Profile p = parseProfile(R"({"name":"t","rules":[{"name":"M-speed","regions":[{"name":"ring","r":[3.84,9.856]}]}]})");
    const Region& g = p.rules[0].regions[0];
    CHECK(g.ring);
    CHECK(g.contains({ 0, 5 }));
    CHECK(g.contains({ 4, 4 }));          // diagonal, distance 5.66
    CHECK_FALSE(g.contains({ 0, 3 }));    // too close
    CHECK_FALSE(g.contains({ 7.5, 7.5 })); // distance 10.6, too far
    Profile back = parseProfile(serializeProfile(p));
    CHECK(back.rules[0].regions[0].ring);
    CHECK(back.rules[0].regions[0].rMax == doctest::Approx(9.856f));
}
