// ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
// Controller / keyboard / mouse -> 3DS input state. No Qt / SDL dependency:
// the Qt layer feeds raw events, this class owns profiles and mapping logic.
#pragma once
#include "screenctr_protocol.h"
#include <array>
#include <map>
#include <string>
#include <vector>

namespace sc {

// 3DS-side targets a source can be bound to
enum class Target : int {
    A, B, X, Y, L, R, ZL, ZR, Start, Select, Up, Down, Left, Right, Home,
    CpadUp, CpadDown, CpadLeft, CpadRight, CstickUp, CstickDown, CstickLeft, CstickRight,
    Count
};
const char *targetId(Target t);
const char *targetLabel(Target t);

// Generic gamepad sources (SDL3 "gamepad" layout: positional, Xbox-like).
enum class PadButton : int {
    South, East, West, North, Back, Guide, Start, LeftStick, RightStick,
    LeftShoulder, RightShoulder, DpadUp, DpadDown, DpadLeft, DpadRight, Misc1, Count
};
enum class PadAxis : int { LeftX, LeftY, RightX, RightY, LeftTrigger, RightTrigger, Count };

// A source is "pad:<button>", "axis:<axis>+"/"axis:<axis>-" or "key:<scancode>".
struct Profile {
    std::string name = "Par défaut";
    std::map<Target, std::vector<std::string>> bindings;
    double deadzoneLeft = 0.15, deadzoneRight = 0.15;   // radial, 0..0.9
    double triggerThreshold = 0.35;                      // ZL/ZR from analog triggers
    bool leftStickToCpad = true, rightStickToCstick = true;
    std::string serialize() const;
    bool parse(const std::string &text);
};
Profile defaultProfile(bool nintendoLayout = true);
const char *padButtonId(PadButton b);
const char *padAxisId(PadAxis a);

// Radial dead zone with rescaling, result in [-1, 1]
void applyDeadzone(double &x, double &y, double dz);

class InputMapper {
public:
    void setProfile(const Profile &p) { profile_ = p; }
    const Profile &profile() const { return profile_; }

    // Raw state setters (axes in [-1,1], triggers in [0,1], +y = DOWN as in SDL)
    void setPadButton(PadButton b, bool down) { pad_[(int)b] = down; }
    void setPadAxis(PadAxis a, double v) { axes_[(int)a] = v; }
    void setKey(int scancode, bool down) { if (down) keys_[scancode] = true; else keys_.erase(scancode); }
    void setTouch(bool down, int x, int y) { touch_ = down; tx_ = x; ty_ = y; }
    void clearAll();                     // focus loss, controller unplugged

    // Compose the full ScreenCTR input state (session/seq left to the caller).
    sc_input compose() const;
    // True when anything at all is pressed / deflected / touched.
    bool anyActive() const;

private:
    bool sourceActive(const std::string &src) const;
    double axisValue(const std::string &src) const; // 0..1 deflection for axis sources
    Profile profile_ = defaultProfile();
    std::array<bool, (int)PadButton::Count> pad_{};
    std::array<double, (int)PadAxis::Count> axes_{};
    std::map<int, bool> keys_;
    bool touch_ = false; int tx_ = 0, ty_ = 0;
};

} // namespace sc
