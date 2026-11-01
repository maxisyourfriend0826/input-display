// The new code must make exactly the same M-speed decision as v4.43 for
// every raw stick position that can possibly hit.
#include "doctest/doctest.h"
#include "legacy_v443.hpp"

#include "idisp/rules.hpp"
#include "idisp/stick.hpp"

#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

#ifndef IDISP_ASSETS_DIR
#error "IDISP_ASSETS_DIR must be defined by the build"
#endif

static idisp::Profile loadBundledProfile() {
    std::ifstream f(std::string(IDISP_ASSETS_DIR) + "/profiles/sonic-generations-50.json");
    REQUIRE(f.good());
    std::stringstream ss;
    ss << f.rdbuf();
    return idisp::parseProfile(ss.str());
}

static bool sameBits(float a, float b) { return std::memcmp(&a, &b, sizeof a) == 0; }

TEST_CASE("normalization matches v4.43 bit for bit for every raw axis value") {
    for (int raw = -32768; raw <= 32767; ++raw) {
        const float newX = idisp::axisToUnitsLegacy(raw, false);
        const float newY = idisp::axisToUnitsLegacy(raw, true);
        const float oldX = legacy::joyX(raw) * 128;
        const float oldY = legacy::joyY(raw) * 128;
        if (!sameBits(newX, oldX) || !sameBits(newY, oldY)) {
            FAIL("raw " << raw << ": new (" << newX << ", " << newY << ") old (" << oldX << ", " << oldY << ")");
        }
    }
}

TEST_CASE("bundled M-speed profile matches v4.43 speedCheck on every reachable raw position") {
    const idisp::Profile profile = loadBundledProfile();
    REQUIRE(profile.rules.size() == 1);
    const idisp::Rule& rule = profile.rules[0];
    const legacy::Thresholds t;

    // Every region lies within |units| <= 42, i.e. |raw| <= 42 * 256. Outside
    // that band neither version can hit; we still check a margin beyond it.
    const int lo = -46 * 256, hi = 46 * 256;
#ifdef NDEBUG
    const int step = 1;   // exhaustive: ~550 million positions
#else
    const int step = 7;   // debug builds are slow; CI runs Release
#endif
    std::vector<float> ux, uy, jx, jy;
    for (int raw = lo; raw <= hi; ++raw) {
        ux.push_back(idisp::axisToUnitsLegacy(raw, false));
        uy.push_back(idisp::axisToUnitsLegacy(raw, true));
        jx.push_back(legacy::joyX(raw));
        jy.push_back(legacy::joyY(raw));
    }

    long long checked = 0, hits = 0, mismatches = 0;
    for (size_t i = 0; i < ux.size(); i += step) {
        for (size_t k = 0; k < uy.size(); ++k) {
            const bool now = rule.hits({ ux[i], uy[k] });
            const bool old = legacy::speedCheck(t, jx[i], jy[k]);
            ++checked;
            hits += old;
            if (now != old && ++mismatches <= 10) {
                MESSAGE("mismatch at raw (" << lo + (int)i << ", " << lo + (int)k << "): new " << now << " old " << old);
            }
        }
    }
    MESSAGE("checked " << checked << " positions, " << hits << " M-speed hits");
    CHECK(hits > 0);
    CHECK(mismatches == 0);
}
