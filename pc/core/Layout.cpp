// ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
#include "Layout.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace sc {

static const double TW = 400, TH = 240, BW = 320, BH = 240;

const char *layoutName(LayoutMode m)
{
    switch (m) {
    case LayoutMode::Vertical: return "Superposés";
    case LayoutMode::SideBySide: return "Côte à côte";
    case LayoutMode::LargeSmall: return "Grand + petit";
    case LayoutMode::TopOnly: return "Écran supérieur";
    case LayoutMode::BottomOnly: return "Écran inférieur";
    case LayoutMode::Custom: return "Personnalisée";
    }
    return "";
}

const char *layoutId(LayoutMode m)
{
    switch (m) {
    case LayoutMode::Vertical: return "vertical";
    case LayoutMode::SideBySide: return "side";
    case LayoutMode::LargeSmall: return "large_small";
    case LayoutMode::TopOnly: return "top";
    case LayoutMode::BottomOnly: return "bottom";
    case LayoutMode::Custom: return "custom";
    }
    return "vertical";
}

LayoutMode layoutFromId(const std::string &id)
{
    for (int i = 0; i <= (int)LayoutMode::Custom; i++)
        if (id == layoutId((LayoutMode)i)) return (LayoutMode)i;
    return LayoutMode::Vertical;
}

std::string CustomLayout::serialize() const
{
    char b[160];
    std::snprintf(b, sizeof b, "%.4f,%.4f,%.4f,%.4f;%.4f,%.4f,%.4f,%.4f",
                  top.x, top.y, top.w, top.h, bottom.x, bottom.y, bottom.w, bottom.h);
    return b;
}

bool CustomLayout::parse(const std::string &s)
{
    Rect t, b;
    if (std::sscanf(s.c_str(), "%lf,%lf,%lf,%lf;%lf,%lf,%lf,%lf", &t.x, &t.y, &t.w, &t.h, &b.x, &b.y, &b.w, &b.h) != 8)
        return false;
    auto ok = [](const Rect &r) { return r.w > 0.01 && r.h > 0.01 && r.w <= 4 && r.h <= 4 && r.x > -4 && r.y > -4 && r.x < 4 && r.y < 4; };
    if (!ok(t) || !ok(b)) return false;
    top = t; bottom = b;
    return true;
}

namespace {
struct Native { double w, h; Rect top, bottom; };

Native native(LayoutMode m)
{
    switch (m) {
    case LayoutMode::Vertical:   return {TW, TH + BH, {0, 0, TW, TH}, {(TW - BW) / 2, TH, BW, BH}};
    case LayoutMode::SideBySide: return {TW + BW, TH, {0, 0, TW, TH}, {TW, 0, BW, BH}};
    case LayoutMode::LargeSmall: return {TW + BW / 2, TH, {0, 0, TW, TH}, {TW, TH / 2, BW / 2, BH / 2}};
    case LayoutMode::TopOnly:    return {TW, TH, {0, 0, TW, TH}, {}};
    case LayoutMode::BottomOnly: return {BW, BH, {}, {0, 0, BW, BH}};
    default:                     return {TW, TH, {0, 0, TW, TH}, {}};
    }
}

Rect place(const Rect &r, double s, double ox, double oy)
{
    if (r.empty()) return {};
    return {std::round(ox + r.x * s), std::round(oy + r.y * s), std::round(r.w * s), std::round(r.h * s)};
}
} // namespace

LayoutResult computeLayout(LayoutMode mode, double W, double H, const CustomLayout &c, bool integerScale)
{
    LayoutResult res;
    if (W <= 0 || H <= 0) return res;
    if (mode == LayoutMode::Custom) {
        res.top = {c.top.x * W, c.top.y * H, c.top.w * W, c.top.h * H};
        res.bottom = {c.bottom.x * W, c.bottom.y * H, c.bottom.w * W, c.bottom.h * H};
        res.scale = res.top.w / TW;
        return res;
    }
    Native n = native(mode);
    double s = std::min(W / n.w, H / n.h);
    if (integerScale && s >= 1) s = std::floor(s);
    double ox = (W - n.w * s) / 2, oy = (H - n.h * s) / 2;
    res.top = place(n.top, s, ox, oy);
    res.bottom = place(n.bottom, s, ox, oy);
    res.scale = s;
    return res;
}

void layoutNativeSize(LayoutMode mode, const CustomLayout &c, int &w, int &h)
{
    if (mode == LayoutMode::Custom) {
        // Pick the canvas so the top screen is at 1x
        double cw = TW / std::max(0.01, c.top.w), ch = TH / std::max(0.01, c.top.h);
        w = (int)std::lround(cw); h = (int)std::lround(ch);
    } else {
        Native n = native(mode);
        w = (int)n.w; h = (int)n.h;
    }
    w += w & 1; h += h & 1; // H.264 needs even sizes
}

bool windowToTouch(const Rect &b, double px, double py, int &tx, int &ty)
{
    if (b.empty() || !b.contains(px, py)) return false;
    windowToTouchClamped(b, px, py, tx, ty);
    return true;
}

void windowToTouchClamped(const Rect &b, double px, double py, int &tx, int &ty)
{
    if (b.empty()) { tx = ty = 0; return; }
    tx = (int)std::floor((px - b.x) * BW / b.w);
    ty = (int)std::floor((py - b.y) * BH / b.h);
    tx = std::clamp(tx, 0, (int)BW - 1);
    ty = std::clamp(ty, 0, (int)BH - 1);
}

} // namespace sc
