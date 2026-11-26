// ScreenCTR — unit tests for the PC core (layouts, touch, input mapping, stats)
// Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
#include "Layout.h"
#include "InputMapper.h"
#include "StreamStats.h"
#include <cmath>
#include <cstdio>

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
static bool near(double a, double b, double e = 1e-6) { return std::fabs(a - b) <= e; }

using namespace sc;

static void test_layouts()
{
    CustomLayout c;
    // F1 vertical in a 800x960 window: exact 2x
    auto r = computeLayout(LayoutMode::Vertical, 800, 960, c, false);
    CHECK(near(r.scale, 2) && near(r.top.x, 0) && near(r.top.w, 800) && near(r.bottom.x, 80) && near(r.bottom.y, 480) && near(r.bottom.w, 640));
    // Letterboxing: wide window, centred horizontally
    r = computeLayout(LayoutMode::Vertical, 1920, 960, c, false);
    CHECK(near(r.scale, 2) && near(r.top.x, 560));
    // Integer scaling
    r = computeLayout(LayoutMode::SideBySide, 1920, 1080, c, true);
    CHECK(near(r.scale, 2) && near(r.top.w, 800) && near(r.bottom.x, r.top.x + 800));
    r = computeLayout(LayoutMode::SideBySide, 1920, 1080, c, false);
    CHECK(near(r.scale, 1920.0 / 720.0));
    // Large + small: bottom at half scale, bottom-right aligned
    r = computeLayout(LayoutMode::LargeSmall, 560, 240, c, false);
    CHECK(near(r.bottom.w, 160) && near(r.bottom.h, 120) && near(r.bottom.x, 400) && near(r.bottom.y, 120));
    r = computeLayout(LayoutMode::TopOnly, 400, 240, c, false);
    CHECK(r.bottom.empty() && near(r.top.w, 400));
    r = computeLayout(LayoutMode::BottomOnly, 640, 480, c, false);
    CHECK(r.top.empty() && near(r.bottom.w, 640));
    // Custom
    CustomLayout cl; CHECK(cl.parse("0.1,0.1,0.5,0.5;0.6,0.6,0.3,0.3"));
    r = computeLayout(LayoutMode::Custom, 1000, 1000, cl, false);
    CHECK(near(r.top.x, 100) && near(r.bottom.w, 300));
    CustomLayout back; CHECK(back.parse(cl.serialize()) && near(back.bottom.x, 0.6, 1e-4));
    CHECK(!back.parse("garbage") && !back.parse("0,0,0,0;0,0,0,0"));
    int w, h; layoutNativeSize(LayoutMode::Vertical, c, w, h); CHECK(w == 400 && h == 480);
    layoutNativeSize(LayoutMode::LargeSmall, c, w, h); CHECK(w == 560 && h == 240);
    CHECK(layoutFromId(layoutId(LayoutMode::BottomOnly)) == LayoutMode::BottomOnly && layoutFromId("??") == LayoutMode::Vertical);
}

static void test_touch()
{
    Rect b{80, 480, 640, 480}; // bottom screen at 2x
    int x, y;
    CHECK(windowToTouch(b, 80, 480, x, y) && x == 0 && y == 0);
    CHECK(windowToTouch(b, 719.9, 959.9, x, y) && x == 319 && y == 239);
    CHECK(windowToTouch(b, 400, 720, x, y) && x == 160 && y == 120);
    CHECK(!windowToTouch(b, 79, 600, x, y) && !windowToTouch(b, 720, 600, x, y));
    windowToTouchClamped(b, -50, 2000, x, y); CHECK(x == 0 && y == 239);
    // Non-integer scale (1.37x)
    Rect c{10, 20, 320 * 1.37, 240 * 1.37};
    CHECK(windowToTouch(c, 10 + 100 * 1.37 + 0.5, 20 + 50 * 1.37 + 0.5, x, y) && x == 100 && y == 50);
}

static void test_input()
{
    InputMapper m;
    sc_input in = m.compose();
    CHECK(in.buttons == 0 && in.cpad_x == 0 && !m.anyActive());
    // Nintendo positional: east = A
    m.setPadButton(PadButton::East, true); m.setPadButton(PadButton::DpadUp, true);
    in = m.compose(); CHECK(in.buttons == (SC_BTN_A | SC_BTN_UP));
    m.setPadButton(PadButton::East, false); m.setPadButton(PadButton::DpadUp, false);
    // Triggers -> ZL/ZR with threshold
    m.setPadAxis(PadAxis::LeftTrigger, 0.2); CHECK(!(m.compose().buttons & SC_BTN_ZL));
    m.setPadAxis(PadAxis::LeftTrigger, 0.6); CHECK(m.compose().buttons & SC_BTN_ZL);
    m.setPadAxis(PadAxis::LeftTrigger, 0);
    // Deadzone + Y inversion (SDL +y down -> 3DS +y up)
    m.setPadAxis(PadAxis::LeftX, 0.1); m.setPadAxis(PadAxis::LeftY, 0.05);
    in = m.compose(); CHECK(in.cpad_x == 0 && in.cpad_y == 0);
    m.setPadAxis(PadAxis::LeftX, 0); m.setPadAxis(PadAxis::LeftY, -1.0);
    in = m.compose(); CHECK(in.cpad_x == 0 && in.cpad_y == 32767);
    m.setPadAxis(PadAxis::LeftY, 0);
    // Keyboard: W+D (scancodes 26, 7) -> up-right diagonal, normalised
    m.setKey(26, true); m.setKey(7, true);
    in = m.compose(); CHECK(in.cpad_x == in.cpad_y && std::abs(in.cpad_x - 23170) <= 1);
    m.setKey(40, true); CHECK(m.compose().buttons & SC_BTN_START);
    // Touch
    m.setTouch(true, 400, -5); in = m.compose();
    CHECK((in.flags & SC_INPUT_FLAG_TOUCH) && in.touch_x == 319 && in.touch_y == 0);
    // Release everything (disconnect / focus loss)
    m.clearAll(); CHECK(!m.anyActive());
    // Xbox-label layout: south = A
    m.setProfile(defaultProfile(false)); m.setPadButton(PadButton::South, true);
    CHECK(m.compose().buttons == SC_BTN_A);

    double x = 0.5, y = 0; applyDeadzone(x, y, 0.2); CHECK(near(x, 0.375) && near(y, 0));
    x = 1; y = 1; applyDeadzone(x, y, 0.1); CHECK(near(std::sqrt(x * x + y * y), 1.0));

    // Profile round trip and validation
    Profile p = defaultProfile(); p.name = "Ma manette"; p.deadzoneLeft = 0.25; p.bindings[Target::Home] = {"pad:misc1"};
    Profile q; CHECK(q.parse(p.serialize()));
    CHECK(q.name == "Ma manette" && near(q.deadzoneLeft, 0.25) && q.bindings[Target::Home].size() == 1 && q.bindings[Target::A].size() == 2);
    Profile bad; CHECK(!bad.parse("bind.a=pad:nonexistent\n") && !bad.parse("deadzone_left=3\n") && !bad.parse("bind.zz=key:4\n"));
}

static void test_stats()
{
    FpsCounter f; for (int i = 0; i < 30; i++) f.frame(i * 33); CHECK(near(f.fps(990), 30));
    CHECK(near(f.fps(2000), 0));
    RttEstimator r; r.sample(20); CHECK(near(r.srtt(), 20)); for (int i = 0; i < 50; i++) r.sample(40); CHECK(r.srtt() > 39 && r.srtt() < 40.1);
    LossTracker l; for (uint32_t i = 1; i <= 10; i++) if (i != 4 && i != 5) l.completed(0, i);
    l.completed(0, 3); // late duplicate ignored
    CHECK(near(l.takeLossRatio(), 0.2)); CHECK(near(l.takeLossRatio(), 0));
    l.completed(1, 0xFFFFFFFF); l.completed(1, 0); CHECK(near(l.takeLossRatio(), 0)); // wrap
    CHECK(qualityFromScore(qualityScore(0, 5, 1)) == LinkQuality::Excellent);
    CHECK(qualityFromScore(qualityScore(0.15, 80, 20)) == LinkQuality::Poor);
}

int main()
{
    test_layouts(); test_touch(); test_input(); test_stats();
    std::printf("%d/%d checks passed\n", checks - fails, checks);
    return fails ? 1 : 0;
}
