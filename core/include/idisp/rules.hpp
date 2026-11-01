// Data-driven stick rules (e.g. M-speed), loaded from profiles/*.json.
#pragma once
#include "idisp/stick.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace idisp {

// An inclusive box in stick units. With truncate=true the coordinates are
// truncated toward zero before comparing; v4.43 did this for the corner
// regions only, and the bundled profile keeps that behavior.
// With ring=true the region is instead every point whose distance from the
// centre is between rMin and rMax (the M-speed hypothesis for game values;
// see E:/.inputdisplay/mspeed-research/NOTES.md).
struct Region {
    std::string name;
    float xMin = 0, xMax = 0, yMin = 0, yMax = 0;
    bool truncate = false;
    bool ring = false;
    float rMin = 0, rMax = 0;

    bool contains(StickUnits u) const {
        if (ring) {
            const double r = std::sqrt((double)u.x * u.x + (double)u.y * u.y);
            return r >= rMin && r <= rMax;
        }
        if (truncate) {
            const int x = static_cast<int>(u.x), y = static_cast<int>(u.y);
            return x >= xMin && x <= xMax && y >= yMin && y <= yMax;
        }
        return u.x >= xMin && u.x <= xMax && u.y >= yMin && u.y <= yMax;
    }
};

struct Rule {
    std::string name;   // e.g. "M-speed"
    std::string label;  // short text drawn on the overlay, e.g. "M"
    std::vector<Region> regions;

    bool hits(StickUnits u) const {
        for (const auto& r : regions)
            if (r.contains(u)) return true;
        return false;
    }
};

struct Profile {
    std::string name;
    std::vector<Rule> rules;
};

// Throw std::runtime_error with a readable message on bad input.
Profile parseProfile(const std::string& json);
std::string serializeProfile(const Profile& p);

// Tracks rule hits between drawn frames so a hit that lasts a single
// controller report still shows on screen, without holding it any longer
// than needed: it is shown on the next frame and then dropped, unless
// holdMs > 0 asks for longer.
class RuleMonitor {
public:
    void setProfile(const Profile& p);
    const Profile& profile() const { return profile_; }

    void onSample(uint64_t timestampNs, StickUnits u);

    // Call once per drawn frame, per rule. Returns true if the rule is hit
    // now, was hit by any report since the previous frame, or was hit within
    // the last holdMs milliseconds. Clears the since-last-frame flag.
    bool activeForFrame(size_t rule, uint64_t nowNs, int holdMs);

    bool activeNow(size_t rule) const { return rule < state_.size() && state_[rule].now; }
    uint64_t hitReports(size_t rule) const { return rule < state_.size() ? state_[rule].hits : 0; }

private:
    struct State {
        bool now = false;
        bool sinceFrame = false;
        uint64_t lastHitNs = 0;
        uint64_t hits = 0;
    };
    Profile profile_;
    std::vector<State> state_;
};

} // namespace idisp
