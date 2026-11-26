// ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
#include "KeyMap.h"
#include <QMap>

// Windows: QKeyEvent::nativeScanCode() is the PS/2 "set 1" make code, with
// 0x100 added for E0-prefixed (extended) keys.
static int fromSet1(quint32 sc)
{
    static const QMap<quint32, int> m = {
        {0x1E, 4}, {0x30, 5}, {0x2E, 6}, {0x20, 7}, {0x12, 8}, {0x21, 9}, {0x22, 10}, {0x23, 11}, {0x17, 12},
        {0x24, 13}, {0x25, 14}, {0x26, 15}, {0x32, 16}, {0x31, 17}, {0x18, 18}, {0x19, 19}, {0x10, 20}, {0x13, 21},
        {0x1F, 22}, {0x14, 23}, {0x16, 24}, {0x2F, 25}, {0x11, 26}, {0x2D, 27}, {0x15, 28}, {0x2C, 29},
        {0x02, 30}, {0x03, 31}, {0x04, 32}, {0x05, 33}, {0x06, 34}, {0x07, 35}, {0x08, 36}, {0x09, 37}, {0x0A, 38}, {0x0B, 39},
        {0x1C, 40}, {0x01, 41}, {0x0E, 42}, {0x0F, 43}, {0x39, 44}, {0x0C, 45}, {0x0D, 46}, {0x1A, 47}, {0x1B, 48},
        {0x2B, 49}, {0x27, 51}, {0x28, 52}, {0x29, 53}, {0x33, 54}, {0x34, 55}, {0x35, 56},
        {0x14D, 79}, {0x14B, 80}, {0x150, 81}, {0x148, 82},
        {0x1D, 224}, {0x2A, 225}, {0x38, 226}, {0x11D, 228}, {0x36, 229}, {0x138, 230},
        {0x52, 98}, {0x4F, 89}, {0x50, 90}, {0x51, 91}, {0x4B, 92}, {0x4C, 93}, {0x4D, 94}, {0x47, 95}, {0x48, 96}, {0x49, 97},
        {0x11C, 88},
    };
    return m.value(sc, 0);
}

// Fallback (non-Windows or missing scan code): Qt::Key, layout dependent.
static int fromQtKey(int k)
{
    if (k >= Qt::Key_A && k <= Qt::Key_Z) return 4 + (k - Qt::Key_A);
    if (k >= Qt::Key_1 && k <= Qt::Key_9) return 30 + (k - Qt::Key_1);
    switch (k) {
    case Qt::Key_0: return 39; case Qt::Key_Return: return 40; case Qt::Key_Enter: return 88; case Qt::Key_Escape: return 41;
    case Qt::Key_Backspace: return 42; case Qt::Key_Tab: return 43; case Qt::Key_Space: return 44;
    case Qt::Key_Right: return 79; case Qt::Key_Left: return 80; case Qt::Key_Down: return 81; case Qt::Key_Up: return 82;
    case Qt::Key_Control: return 224; case Qt::Key_Shift: return 225; case Qt::Key_Alt: return 226;
    default: return 0;
    }
}

int hidScancodeFromEvent(const QKeyEvent *e)
{
#ifdef Q_OS_WIN
    if (int h = fromSet1(e->nativeScanCode())) return h;
#endif
    return fromQtKey(e->key());
}

QString hidScancodeName(int h)
{
    // Names of the US-QWERTY position; the UI shows them as positions.
    if (h >= 4 && h <= 29) return QString(QChar('A' + (h - 4)));
    if (h >= 30 && h <= 38) return QString::number(h - 29);
    static const QMap<int, QString> n = {
        {39, "0"}, {40, "Entrée"}, {41, "Échap"}, {42, "Retour arrière"}, {43, "Tab"}, {44, "Espace"},
        {79, "→"}, {80, "←"}, {81, "↓"}, {82, "↑"}, {224, "Ctrl"}, {225, "Maj"}, {226, "Alt"}, {228, "Ctrl droit"},
        {229, "Maj droite"}, {230, "Alt Gr"}, {88, "Entrée (pavé)"},
    };
    if (h >= 89 && h <= 98) return QString("Pavé %1").arg(h == 98 ? 0 : h - 88);
    return n.value(h, QString("Touche %1").arg(h));
}
