// ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
#include "InputMapper.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace sc {

static const char *kTargetIds[] = {
    "a", "b", "x", "y", "l", "r", "zl", "zr", "start", "select", "up", "down", "left", "right", "home",
    "cpad_up", "cpad_down", "cpad_left", "cpad_right", "cstick_up", "cstick_down", "cstick_left", "cstick_right" };
static const char *kTargetLabels[] = {
    "A", "B", "X", "Y", "L", "R", "ZL", "ZR", "START", "SELECT", "Croix ↑", "Croix ↓", "Croix ←", "Croix →", "HOME",
    "Circle Pad ↑", "Circle Pad ↓", "Circle Pad ←", "Circle Pad →", "C-Stick ↑", "C-Stick ↓", "C-Stick ←", "C-Stick →" };
static const char *kPadIds[] = { "south", "east", "west", "north", "back", "guide", "start", "lstick", "rstick",
    "lshoulder", "rshoulder", "dpup", "dpdown", "dpleft", "dpright", "misc1" };
static const char *kAxisIds[] = { "leftx", "lefty", "rightx", "righty", "lefttrigger", "righttrigger" };

const char *targetId(Target t) { return kTargetIds[(int)t]; }
const char *targetLabel(Target t) { return kTargetLabels[(int)t]; }
const char *padButtonId(PadButton b) { return kPadIds[(int)b]; }
const char *padAxisId(PadAxis a) { return kAxisIds[(int)a]; }

// USB HID scancodes (same values as SDL_Scancode / Qt nativeScanCode mapping
// done in the Qt layer): layout-independent, so ZQSD on AZERTY == WASD.
enum { SC_A = 4, SC_D = 7, SC_H = 11, SC_I = 12, SC_J = 13, SC_K = 14, SC_L = 15, SC_O = 18, SC_Q = 20,
       SC_S = 22, SC_U = 24, SC_W = 26, SC_ENTER = 40, SC_BKSP = 42, SC_7 = 36, SC_9 = 38,
       SC_RIGHT = 79, SC_LEFT = 80, SC_DOWN = 81, SC_UP = 82 };

static std::string key(int sc) { return "key:" + std::to_string(sc); }

Profile defaultProfile(bool nintendo)
{
    Profile p;
    auto &b = p.bindings;
    // Nintendo layout = position based (A is the right face button).
    b[Target::A] = { nintendo ? "pad:east" : "pad:south", key(SC_L) };
    b[Target::B] = { nintendo ? "pad:south" : "pad:east", key(SC_K) };
    b[Target::X] = { nintendo ? "pad:north" : "pad:west", key(SC_I) };
    b[Target::Y] = { nintendo ? "pad:west" : "pad:north", key(SC_J) };
    b[Target::L] = { "pad:lshoulder", key(SC_U) };
    b[Target::R] = { "pad:rshoulder", key(SC_O) };
    b[Target::ZL] = { "axis:lefttrigger+", key(SC_7) };
    b[Target::ZR] = { "axis:righttrigger+", key(SC_9) };
    b[Target::Start] = { "pad:start", key(SC_ENTER) };
    b[Target::Select] = { "pad:back", key(SC_BKSP) };
    b[Target::Home] = { "pad:guide", key(SC_H) };
    b[Target::Up] = { "pad:dpup", key(SC_UP) };
    b[Target::Down] = { "pad:dpdown", key(SC_DOWN) };
    b[Target::Left] = { "pad:dpleft", key(SC_LEFT) };
    b[Target::Right] = { "pad:dpright", key(SC_RIGHT) };
    b[Target::CpadUp] = { key(SC_W) };
    b[Target::CpadDown] = { key(SC_S) };
    b[Target::CpadLeft] = { key(SC_A) };
    b[Target::CpadRight] = { key(SC_D) };
    (void)SC_Q;
    return p;
}

std::string Profile::serialize() const
{
    std::ostringstream o;
    o << "name=" << name << "\n";
    o << "deadzone_left=" << deadzoneLeft << "\ndeadzone_right=" << deadzoneRight << "\n";
    o << "trigger_threshold=" << triggerThreshold << "\n";
    o << "left_stick_cpad=" << (leftStickToCpad ? 1 : 0) << "\nright_stick_cstick=" << (rightStickToCstick ? 1 : 0) << "\n";
    for (int t = 0; t < (int)Target::Count; t++) {
        auto it = bindings.find((Target)t);
        o << "bind." << kTargetIds[t] << "=";
        if (it != bindings.end())
            for (size_t i = 0; i < it->second.size(); i++) o << (i ? "," : "") << it->second[i];
        o << "\n";
    }
    return o.str();
}

static bool validSource(const std::string &s)
{
    if (s.rfind("key:", 0) == 0) { char *e; long v = std::strtol(s.c_str() + 4, &e, 10); return *e == 0 && v > 0 && v < 512; }
    if (s.rfind("pad:", 0) == 0) { for (auto id : kPadIds) if (s.substr(4) == id) return true; return false; }
    if (s.rfind("axis:", 0) == 0 && s.size() > 6 && (s.back() == '+' || s.back() == '-')) {
        std::string a = s.substr(5, s.size() - 6);
        for (auto id : kAxisIds) if (a == id) return true;
    }
    return false;
}

bool Profile::parse(const std::string &text)
{
    Profile p; p.bindings.clear();
    std::istringstream in(text); std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        auto num = [&](double lo, double hi, double &dst) { char *e; double d = std::strtod(v.c_str(), &e); if (*e || d < lo || d > hi) return false; dst = d; return true; };
        if (k == "name") p.name = v;
        else if (k == "deadzone_left") { if (!num(0, 0.9, p.deadzoneLeft)) return false; }
        else if (k == "deadzone_right") { if (!num(0, 0.9, p.deadzoneRight)) return false; }
        else if (k == "trigger_threshold") { if (!num(0.05, 0.95, p.triggerThreshold)) return false; }
        else if (k == "left_stick_cpad") p.leftStickToCpad = v == "1";
        else if (k == "right_stick_cstick") p.rightStickToCstick = v == "1";
        else if (k.rfind("bind.", 0) == 0) {
            std::string tid = k.substr(5); int t = -1;
            for (int i = 0; i < (int)Target::Count; i++) if (tid == kTargetIds[i]) t = i;
            if (t < 0) return false;
            std::vector<std::string> srcs; std::istringstream vs(v); std::string s;
            while (std::getline(vs, s, ',')) { if (s.empty()) continue; if (!validSource(s)) return false; srcs.push_back(s); }
            p.bindings[(Target)t] = srcs;
        }
    }
    *this = p;
    return true;
}

void applyDeadzone(double &x, double &y, double dz)
{
    double m = std::sqrt(x * x + y * y);
    if (m <= dz || m == 0) { x = y = 0; return; }
    double nm = std::min(1.0, (m - dz) / (1.0 - dz));
    x = x / m * nm; y = y / m * nm;
}

void InputMapper::clearAll()
{
    pad_.fill(false); axes_.fill(0.0); keys_.clear(); touch_ = false;
}

double InputMapper::axisValue(const std::string &s) const
{
    std::string a = s.substr(5, s.size() - 6);
    for (int i = 0; i < (int)PadAxis::Count; i++)
        if (a == kAxisIds[i]) { double v = axes_[i]; return s.back() == '+' ? std::max(0.0, v) : std::max(0.0, -v); }
    return 0;
}

bool InputMapper::sourceActive(const std::string &s) const
{
    if (s.rfind("key:", 0) == 0) return keys_.count(std::atoi(s.c_str() + 4)) > 0;
    if (s.rfind("pad:", 0) == 0) {
        for (int i = 0; i < (int)PadButton::Count; i++) if (s.compare(4, std::string::npos, kPadIds[i]) == 0) return pad_[i];
        return false;
    }
    if (s.rfind("axis:", 0) == 0) return axisValue(s) >= profile_.triggerThreshold;
    return false;
}

static int16_t toS16(double v) { return (int16_t)std::lround(std::clamp(v, -1.0, 1.0) * 32767.0); }

sc_input InputMapper::compose() const
{
    sc_input in; sc_input_neutral(&in);
    static const uint32_t bits[] = { SC_BTN_A, SC_BTN_B, SC_BTN_X, SC_BTN_Y, SC_BTN_L, SC_BTN_R, SC_BTN_ZL, SC_BTN_ZR,
        SC_BTN_START, SC_BTN_SELECT, SC_BTN_UP, SC_BTN_DOWN, SC_BTN_LEFT, SC_BTN_RIGHT, SC_BTN_HOME };
    auto active = [&](Target t) {
        auto it = profile_.bindings.find(t);
        if (it == profile_.bindings.end()) return false;
        for (auto &s : it->second) if (sourceActive(s)) return true;
        return false;
    };
    for (int t = 0; t <= (int)Target::Home; t++) if (active((Target)t)) in.buttons |= bits[t];

    // Analog sticks (SDL: +y is down; 3DS protocol: +y is up)
    double lx = 0, ly = 0, rx = 0, ry = 0;
    if (profile_.leftStickToCpad) { lx = axes_[(int)PadAxis::LeftX]; ly = -axes_[(int)PadAxis::LeftY]; applyDeadzone(lx, ly, profile_.deadzoneLeft); }
    if (profile_.rightStickToCstick) { rx = axes_[(int)PadAxis::RightX]; ry = -axes_[(int)PadAxis::RightY]; applyDeadzone(rx, ry, profile_.deadzoneRight); }
    // Digital bindings to stick directions override analog on that axis
    auto digital = [&](Target neg, Target pos, double &v) {
        bool n = active(neg), p = active(pos);
        if (n != p) v = p ? 1.0 : -1.0;   // both held: cancel out to the analog value
    };
    digital(Target::CpadLeft, Target::CpadRight, lx);
    digital(Target::CpadDown, Target::CpadUp, ly);
    digital(Target::CstickLeft, Target::CstickRight, rx);
    digital(Target::CstickDown, Target::CstickUp, ry);
    // Keyboard diagonals: keep the Circle Pad inside its circular range
    double m = std::sqrt(lx * lx + ly * ly); if (m > 1) { lx /= m; ly /= m; }
    m = std::sqrt(rx * rx + ry * ry); if (m > 1) { rx /= m; ry /= m; }
    in.cpad_x = toS16(lx); in.cpad_y = toS16(ly); in.cstick_x = toS16(rx); in.cstick_y = toS16(ry);

    if (touch_) {
        in.flags |= SC_INPUT_FLAG_TOUCH;
        in.touch_x = (uint16_t)std::clamp(tx_, 0, SC_BOTTOM_W - 1);
        in.touch_y = (uint16_t)std::clamp(ty_, 0, SC_BOTTOM_H - 1);
    }
    return in;
}

bool InputMapper::anyActive() const
{
    sc_input in = compose();
    return in.buttons || in.cpad_x || in.cpad_y || in.cstick_x || in.cstick_y || (in.flags & SC_INPUT_FLAG_TOUCH);
}

} // namespace sc
