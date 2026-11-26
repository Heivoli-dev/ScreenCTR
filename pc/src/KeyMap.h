// ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
// Qt key events -> USB HID scancodes (keyboard-layout independent bindings:
// the key at the "W" position is the same on QWERTY and AZERTY).
#pragma once
#include <QKeyEvent>
#include <QString>

int hidScancodeFromEvent(const QKeyEvent *e);   // 0 if unknown
QString hidScancodeName(int hid);               // "Z", "Entrée", "↑"…
