// Stick sampling and normalization. No SDL here, so it can be unit-tested.
#pragma once
#include <cstdint>
#include <optional>

namespace idisp {

// Stick position in "stick units": -128..128, Y up is positive.
// The M-speed thresholds are expressed in these units.
struct StickUnits {
    float x = 0.0f;
    float y = 0.0f;
};

// Raw SDL axis values: -32768..32767, Y down is positive.
struct RawStick {
    int16_t x = 0;
    int16_t y = 0;
};

// Same float arithmetic as v4.43 (Main.cpp pollAndUpdateGameController +
// speedCheck), so results match bit for bit. tests/test_equivalence.cpp
// checks this against a verbatim copy of the old code for every raw value.
inline float axisToUnitsLegacy(int raw, bool invert) {
    if (raw > 0) raw++;
    float j = (invert ? -raw : raw) / 32768.0f;
    return j * 128;
}

inline StickUnits toUnitsLegacy(RawStick s) {
    return { axisToUnitsLegacy(s.x, false), axisToUnitsLegacy(s.y, true) };
}

// D-pad mode: the d-pad pushes the stick fully in that direction.
// Right wins over left and up over down, as in v4.43.
inline StickUnits applyDpad(StickUnits u, bool up, bool down, bool left, bool right) {
    if (right) u.x = 128.0f;
    else if (left) u.x = -128.0f;
    if (up) u.y = 128.0f;
    else if (down) u.y = -128.0f;
    return u;
}

// SDL sends X and Y as separate events. Evaluating after each event would
// briefly combine the new X with the old Y, a position the stick never had,
// and that could light M-speed falsely. SDL stamps every axis from one
// controller report with the same timestamp, so a report is complete when an
// event with a different timestamp arrives, or when the queue is drained.
class StickAssembler {
public:
    struct Report {
        uint64_t timestampNs;
        RawStick stick;
    };

    std::optional<Report> push(uint64_t timestampNs, bool isY, int16_t value) {
        std::optional<Report> done;
        if (pending_ && timestampNs != timestamp_) done = Report{ timestamp_, current_ };
        (isY ? current_.y : current_.x) = value;
        timestamp_ = timestampNs;
        pending_ = true;
        return done;
    }

    std::optional<Report> flush() {
        if (!pending_) return std::nullopt;
        pending_ = false;
        return Report{ timestamp_, current_ };
    }

    // Seed with the current state, e.g. when a controller connects.
    void reset(RawStick s) { current_ = s; pending_ = false; }
    RawStick current() const { return current_; }

private:
    RawStick current_{};
    uint64_t timestamp_ = 0;
    bool pending_ = false;
};

} // namespace idisp
