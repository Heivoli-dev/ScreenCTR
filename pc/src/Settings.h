// ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
#pragma once
#include "InputMapper.h"
#include "Layout.h"
#include <QByteArray>
#include <QKeySequence>
#include <QList>
#include <QMap>
#include <QString>

struct SavedConsole {
    QString name, host;
    quint16 port = 6464;
    QString label() const { return name.isEmpty() ? QString("%1:%2").arg(host).arg(port) : QString("%1 — %2:%3").arg(name, host).arg(port); }
};

// All persistent settings (QSettings INI in %APPDATA%/Heivoli Network/ScreenCTR.ini)
class Settings {
public:
    static Settings &get();
    void load();
    void save() const;

    QByteArray clientId;                 // 16 random bytes, generated once
    QString pcName;
    QList<SavedConsole> consoles;
    int lastConsole = -1;
    bool autoReconnect = true;

    sc::LayoutMode layout = sc::LayoutMode::Vertical;
    sc::CustomLayout custom;
    bool integerScale = false, smooth = false;

    int quality = 70, fpsCap = 30, chroma = 0, screens = 3;

    bool remoteInput = true;
    QList<sc::Profile> profiles;
    int activeProfile = 0;

    QMap<QString, QKeySequence> shortcuts;     // action id -> sequence
    static QMap<QString, QKeySequence> defaultShortcuts();
    static QString shortcutLabel(const QString &id);

    QString screenshotDir, videoDir, ffmpegPath;

    void rememberConsole(const SavedConsole &c);   // move to front, dedupe by host:port
private:
    Settings() = default;
};
