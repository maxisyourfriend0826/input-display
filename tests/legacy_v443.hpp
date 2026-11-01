// Verbatim copy of the v4.43 input math (Generations-Input-Display,
// 2.x/SonicInputDisplay/src/Main.cpp, pollAndUpdateGameController and
// speedCheck), used only as the reference the new code must match.
// Thresholds are parsed with std::stof from the shipped 4.43 settings.ini values,
// exactly as loadSettings() did.
#pragma once
#include <string>

namespace legacy {

struct Thresholds {
    float NorthYFloor = std::stof("37"), NorthYCeil = std::stof("41.17");
    float NorthXFloor = std::stof("-37"), NorthXCeil = std::stof("39.9");
    float WestYCeil = std::stof("38.81"), WestYFloor = std::stof("-36");
    float WestXFloor = std::stof("-41"), WestXCeil = std::stof("-37");
    float EastYCeil = std::stof("38.32"), EastYFloor = std::stof("-36");
    float EastXFloor = std::stof("37"), EastXCeil = std::stof("41");
    float SouthYCeil = std::stof("-38"), SouthYFloor = std::stof("-41");
    float SouthXFloor = std::stof("-37"), SouthXCeil = std::stof("37");
    int CornerMin = 36, CornerMax = 37;
    int CornerMinN = -36, CornerMaxN = -37;  // = CornerMin * -1, CornerMax * -1 (Main.cpp:1832)
};

// Main.cpp:1334-1350: raw axis -> joyX / joyY
inline float joyX(int leftX) {
    if (leftX > 0) leftX++;
    return leftX / 32768.0f;
}
inline float joyY(int leftY) {
    if (leftY > 0) leftY++;
    return -leftY / 32768.0f;
}

// Main.cpp:1278-1299
inline bool speedCheck(const Thresholds& t, float cordX, float cordY) {
    bool mSpeed = false;
    float crdX = cordX * 128;
    float crdY = cordY * 128;
    int crdXrounded = (int)crdX;
    int crdYrounded = (int)crdY;

    if (crdY >= t.NorthYFloor && crdY <= t.NorthYCeil && crdX >= t.NorthXFloor && crdX <= t.NorthXCeil) { mSpeed = true; }
    if (crdY <= t.SouthYCeil && crdY >= t.SouthYFloor && crdX >= t.SouthXFloor && crdX <= t.SouthXCeil) { mSpeed = true; }
    if (crdX >= t.EastXFloor && crdX <= t.EastXCeil && crdY >= t.EastYFloor && crdY <= t.EastYCeil) { mSpeed = true; }
    if (crdX <= t.WestXCeil && crdX >= t.WestXFloor && crdY >= t.WestYFloor && crdY <= t.WestYCeil) { mSpeed = true; }
    if ((crdXrounded >= t.CornerMin && crdXrounded <= t.CornerMax) && (crdYrounded >= t.CornerMin && crdYrounded <= t.CornerMax)) { mSpeed = true; }
    if ((crdXrounded <= t.CornerMinN && crdXrounded >= t.CornerMaxN) && (crdYrounded >= t.CornerMin && crdYrounded <= t.CornerMax)) { mSpeed = true; }
    if ((crdXrounded >= t.CornerMin && crdXrounded <= t.CornerMax) && (crdYrounded <= t.CornerMinN && crdYrounded >= t.CornerMaxN)) { mSpeed = true; }
    if ((crdXrounded <= t.CornerMinN && crdXrounded >= t.CornerMaxN) && (crdYrounded <= t.CornerMinN && crdYrounded >= t.CornerMaxN)) { mSpeed = true; }
    return mSpeed;
}

} // namespace legacy
