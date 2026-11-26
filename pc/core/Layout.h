// ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
// Screen layouts and mouse -> touch conversion. No Qt dependency (unit tested).
#pragma once
#include <string>

namespace sc {

struct Rect {
    double x = 0, y = 0, w = 0, h = 0;
    bool contains(double px, double py) const { return px >= x && py >= y && px < x + w && py < y + h; }
    bool empty() const { return w <= 0 || h <= 0; }
};

enum class LayoutMode { Vertical, SideBySide, LargeSmall, TopOnly, BottomOnly, Custom };

const char *layoutName(LayoutMode m);         // French UI label
const char *layoutId(LayoutMode m);           // stable id for settings
LayoutMode layoutFromId(const std::string &id);

// Custom layout: rectangles in normalised window coordinates (0..1).
struct CustomLayout {
    Rect top{0.0, 0.0, 0.625, 0.625};
    Rect bottom{0.625, 0.5, 0.375, 0.375};
    std::string serialize() const;
    bool parse(const std::string &s);
};

struct LayoutResult {
    Rect top, bottom;           // in window pixels; empty() when hidden
    double scale = 1;           // pixels per 3DS pixel (top screen)
};

// integerScale: snap the scale to whole multiples for pixel-perfect output
// (only when the window is at least 1x).
LayoutResult computeLayout(LayoutMode mode, double winW, double winH,
                           const CustomLayout &custom, bool integerScale);

// Size of the layout at scale 1 (used for screenshots and recording).
void layoutNativeSize(LayoutMode mode, const CustomLayout &custom, int &w, int &h);

// Window point -> 3DS touch coordinate (0..319, 0..239). False if outside.
bool windowToTouch(const Rect &bottom, double px, double py, int &tx, int &ty);
// Like windowToTouch but clamps to the edge (used while a drag continues
// outside the screen so the stylus does not jump).
void windowToTouchClamped(const Rect &bottom, double px, double py, int &tx, int &ty);

} // namespace sc
