#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "idisp/layout.hpp"
#include "idisp/rules.hpp"
#include "idisp/settings.hpp"
#include "idisp/stick.hpp"

using namespace idisp;

// ---- StickAssembler -------------------------------------------------------

TEST_CASE("X and Y from one report are combined before evaluation") {
    StickAssembler a;
    a.reset({ 0, 0 });
    CHECK_FALSE(a.push(100, false, 9500).has_value());  // X of report 1
    CHECK_FALSE(a.push(100, true, -9500).has_value());  // Y of report 1: same timestamp
    auto r1 = a.push(200, false, 0);                    // report 2 starts -> report 1 complete
    REQUIRE(r1.has_value());
    CHECK(r1->timestampNs == 100);
    CHECK(r1->stick.x == 9500);
    CHECK(r1->stick.y == -9500);
    auto r2 = a.flush();
    REQUIRE(r2.has_value());
    CHECK(r2->stick.x == 0);
    CHECK(r2->stick.y == -9500);  // Y unchanged in report 2 keeps its value
    CHECK_FALSE(a.flush().has_value());
}

TEST_CASE("no phantom position: new X with old Y is never emitted") {
    // Moving from (far right, centre) to (centre, top) in one report must not
    // produce (centre, centre) or (far right, top).
    StickAssembler a;
    a.reset({ 32767, 0 });
    a.push(50, false, 0);
    a.push(50, true, -32768);
    auto r = a.flush();
    REQUIRE(r.has_value());
    CHECK(r->stick.x == 0);
    CHECK(r->stick.y == -32768);
}

// ---- RuleMonitor ----------------------------------------------------------

static Profile boxProfile() {
    Profile p;
    Rule r;
    r.name = "Box";
    r.label = "B";
    r.regions.push_back(Region{ "box", 10, 20, 10, 20, false });
    p.rules.push_back(r);
    return p;
}

TEST_CASE("a single-report hit between frames shows on exactly one frame") {
    RuleMonitor m;
    m.setProfile(boxProfile());
    const uint64_t ms = 1'000'000;
    m.onSample(1 * ms, { 15, 15 });  // hit
    m.onSample(2 * ms, { 0, 0 });    // gone before the frame is drawn
    CHECK(m.activeForFrame(0, 16 * ms, 0));        // frame 1: shown
    CHECK_FALSE(m.activeForFrame(0, 33 * ms, 0));  // frame 2: not held
}

TEST_CASE("holdMs keeps a hit lit for that long, and no longer") {
    RuleMonitor m;
    m.setProfile(boxProfile());
    const uint64_t ms = 1'000'000;
    m.onSample(1 * ms, { 15, 15 });
    m.onSample(2 * ms, { 0, 0 });
    CHECK(m.activeForFrame(0, 3 * ms, 20));
    CHECK(m.activeForFrame(0, 20 * ms, 20));
    CHECK_FALSE(m.activeForFrame(0, 22 * ms, 20));
}

TEST_CASE("a held position stays lit every frame") {
    RuleMonitor m;
    m.setProfile(boxProfile());
    m.onSample(1, { 12, 18 });
    for (uint64_t f = 1; f < 5; ++f) CHECK(m.activeForFrame(0, f * 16'000'000, 0));
}

TEST_CASE("truncated regions compare whole units like v4.43 corners") {
    Region g{ "corner", 36, 37, 36, 37, true };
    CHECK(g.contains({ 36.0f, 37.99f }));   // 37.99 truncates to 37
    CHECK_FALSE(g.contains({ 35.99f, 36 })); // 35.99 truncates to 35
    Region n{ "corner-neg", -37, -36, -37, -36, true };
    CHECK(n.contains({ -37.99f, -36.5f }));
    CHECK_FALSE(n.contains({ -35.99f, -36 }));
}

// ---- Profile JSON ---------------------------------------------------------

TEST_CASE("profile round-trips through JSON") {
    Profile p = boxProfile();
    p.name = "Test";
    p.rules[0].regions.push_back(Region{ "c", -37, -36, 36, 37, true });
    Profile q = parseProfile(serializeProfile(p));
    REQUIRE(q.rules.size() == 1);
    REQUIRE(q.rules[0].regions.size() == 2);
    CHECK(q.name == "Test");
    CHECK(q.rules[0].regions[1].truncate);
    CHECK(q.rules[0].regions[1].xMin == -37);
}

TEST_CASE("bad profiles give readable errors") {
    CHECK_THROWS_WITH_AS(parseProfile("{"), doctest::Contains("invalid JSON"), std::runtime_error);
    CHECK_THROWS_WITH_AS(parseProfile(R"({"rules":[{"name":"M","regions":[{"x":[5,1],"y":[0,1]}]}]})"),
                         doctest::Contains("min is greater than max"), std::runtime_error);
}

// ---- Settings -------------------------------------------------------------

TEST_CASE("a v4.43 settings.ini loads with the same values") {
    const char* ini =
        "showCoordinatesSwitch=1\nshowMSpeedSwitch=0\ntoonSwitch=1\ncircleSize=4\n"
        "pulseRate=0.004\njoystickGlowColorR=45\njoystickGlowColorG=169\njoystickGlowColorB=255\n"
        "bgR=10\nbgG=32\nbgB=91\nfirstCrossColorA=255\nfillR=163\nfillA=50\nshapeIndex=4\n"
        "ButtonSetIndex=3\nToonIndex=2\nNorthYCeil=41.17\nCornerMin=36\n";
    Settings s = parseSettings(ini);
    CHECK(s.showCoordinates);
    CHECK_FALSE(s.showRuleIndicator);
    CHECK(s.pulseRate == doctest::Approx(0.004f));
    CHECK(s.joystickGlowColor.g == 169);
    CHECK(s.bgColor.b == 91);
    CHECK(s.fill.a == 50);
    CHECK(s.shapeIndex == 4);
    CHECK(s.buttonSetIndex == 3);
    CHECK(s.toonIndex == 2);
    // Old threshold keys are kept, not lost, when the file is saved again.
    CHECK(s.unknown.at("NorthYCeil") == "41.17");
    Settings again = parseSettings(serializeSettings(s));
    CHECK(again.unknown.at("CornerMin") == "36");
    CHECK(again.bgColor.r == 10);
    CHECK(again.fill.a == 50);
}

TEST_CASE("out-of-range values are clamped") {
    Settings s = parseSettings("shapeIndex=99\nbgR=400\ncircleSize=-3\n");
    CHECK(s.shapeIndex == 7);  // 8 shapes: the last is "target"
    CHECK(s.bgColor.r == 255);
    CHECK(s.circleSize == 1);
}

// ---- Layouts --------------------------------------------------------------

TEST_CASE("layouts parse, and malformed buttons are rejected") {
    LayoutFile f = parseLayouts(R"({"layouts":[{"name":"L","ringCenter":[10,20],"buttons":{"A":[1,2,3,4]}}]})");
    REQUIRE(f.layouts.size() == 1);
    CHECK(f.layouts[0].ringCenterX == 10);
    CHECK(f.layouts[0].buttons.at("A").w == 3);
    CHECK_THROWS_AS(parseLayouts(R"({"layouts":[{"buttons":{"A":[1,2]}}]})"), std::runtime_error);
}

TEST_CASE("a layout without the new fields keeps the v4.43 placement") {
    LayoutFile f = parseLayouts(R"({"width":420,"height":300,"layouts":[{"name":"P","ringCenter":[288,144],"buttons":{}}]})");
    const Layout& l = f.layouts[0];
    CHECK(l.width == 420);
    CHECK(l.height == 300);
    CHECK(l.stickScale == 1.0f);
    CHECK(l.toonRect() == Rect{ 152, 8, 272, 272 });     // ring center - 136
    CHECK(l.coordsRect() == Rect{ 184, 278, kCoordsW, kCoordsH });  // x: ring center - 104
    CHECK(l.shown(kStick));
}

TEST_CASE("user layouts round-trip, and an empty user file is allowed") {
    Layout l;
    l.name = "Mine";
    l.width = 500;
    l.height = 260;
    l.ringCenterX = 300;
    l.ringCenterY = 130;
    l.stickScale = 0.75f;
    l.toon = Rect{ 1, 2, 100, 100 };
    l.coords = Rect{ 5, 230, 115, 11 };
    l.buttons["A"] = Rect{ 10, 20, 30, 40 };
    l.buttons["Select"] = Rect{ 50, 60, 16, 16 };
    l.hidden = { "LT", kToon };
    LayoutFile back = parseLayouts(serializeLayouts({ l }), true);
    REQUIRE(back.layouts.size() == 1);
    CHECK(back.layouts[0] == l);
    CHECK(parseLayouts(serializeLayouts({}), true).layouts.empty());
    CHECK_THROWS_AS(parseLayouts(R"({"layouts":[]})"), std::runtime_error);
}

TEST_CASE("right stick, stick options, images and layers round-trip") {
    Layout l;
    l.name = "Layers";
    l.rightX = 60;
    l.rightY = 70;
    l.rightScale = 0.25f;
    l.sticks[0] = { 2, false };
    l.sticks[1] = { kShapeDotOnly, true };
    l.images.push_back({ "img1", "ring.png", Rect{ 0, 0, 200, 200 } });
    l.images.push_back({ "img2", "frame-2.png", Rect{ 5, 5, 50, 40 } });
    l.order = { "img2", kToon, kStick };
    LayoutFile back = parseLayouts(serializeLayouts({ l }), true);
    REQUIRE(back.layouts.size() == 1);
    CHECK(back.layouts[0] == l);
}

TEST_CASE("layer order: missing images go to the bottom, other elements on top") {
    Layout l;
    l.images.push_back({ "img1", "a.png", Rect{ 0, 0, 4, 4 } });
    l.images.push_back({ "img2", "b.png", Rect{ 0, 0, 4, 4 } });
    std::vector<std::string> d = l.drawOrder();  // default order
    REQUIRE(d.size() == 2 + kButtonCount + 4);
    CHECK(d[0] == "img1");
    CHECK(d[1] == "img2");
    CHECK(d[d.size() - 4] == kStickR);
    CHECK(d[d.size() - 3] == kStick);
    CHECK(d.back() == kToon);
    l.order = { kToon, "img1", "nope", kToon };  // unknown and repeated ids are dropped
    d = l.drawOrder();
    CHECK(d[0] == "img2");  // not in the order: bottom
    CHECK(d[1] == kToon);
    CHECK(d[2] == "img1");
    CHECK(d.size() == 2 + kButtonCount + 4);
}

TEST_CASE("old layouts hide the right stick; image files must be plain names") {
    LayoutFile f = parseLayouts(R"({"layouts":[{"name":"Old","buttons":{}}]})");
    CHECK_FALSE(f.layouts[0].shown(kStickR));
    CHECK(f.layouts[0].shown(kStick));
    for (const char* bad : { "../x.png", "a/b.png", "a\\b.png", "C:x.png", "" }) {
        CAPTURE(bad);
        CHECK_FALSE(safeImageFile(bad));
        const std::string text = std::string(R"({"layouts":[{"name":"I","images":[{"id":"img1","file":")") +
                                 (std::string(bad) == "a\\b.png" ? "a\\\\b.png" : bad) + R"(","rect":[0,0,4,4]}]}]})";
        CHECK_THROWS_AS(parseLayouts(text), std::runtime_error);
    }
    CHECK(safeImageFile("my ring-2.png"));
    CHECK_THROWS_AS(parseLayouts(R"({"layouts":[{"name":"I","images":[{"id":"x1","file":"a.png","rect":[0,0,4,4]}]}]})"),
                    std::runtime_error);  // ids must look like img<n>
}

TEST_CASE("a malformed user layout is skipped, the others still load") {
    const std::string text = R"({"layouts":[{"name":"Good","buttons":{"A":[1,2,3,4]}},
                                             {"name":"Bad","buttons":{"A":[1,2]}},
                                             {"name":"Also bad","ringCenter":"x"}]})";
    std::vector<std::string> skipped;
    LayoutFile f = parseLayouts(text, true, &skipped);
    REQUIRE(f.layouts.size() == 1);
    CHECK(f.layouts[0].name == "Good");
    CHECK(skipped.size() == 2);
    CHECK_THROWS_AS(parseLayouts(text, true), std::runtime_error);  // without the list, the file fails
}
