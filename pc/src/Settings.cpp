// ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
#include "Settings.h"
#include <QHostInfo>
#include <QRandomGenerator>
#include <QSettings>
#include <QStandardPaths>

Settings &Settings::get() { static Settings s; return s; }

QMap<QString, QKeySequence> Settings::defaultShortcuts()
{
    return {
        {"layout_vertical", QKeySequence(Qt::Key_F1)}, {"layout_side", QKeySequence(Qt::Key_F2)},
        {"layout_large_small", QKeySequence(Qt::Key_F3)}, {"layout_top", QKeySequence(Qt::Key_F4)},
        {"layout_bottom", QKeySequence(Qt::Key_F5)}, {"screenshot", QKeySequence(Qt::Key_F6)},
        {"record", QKeySequence(Qt::Key_F7)}, {"layout_custom", QKeySequence(Qt::Key_F8)},
        {"fullscreen", QKeySequence(Qt::Key_F11)}, {"hide_ui", QKeySequence(Qt::CTRL | Qt::Key_H)},
        {"edit_layout", QKeySequence(Qt::CTRL | Qt::Key_E)}, {"toggle_input", QKeySequence(Qt::CTRL | Qt::Key_I)},
    };
}

QString Settings::shortcutLabel(const QString &id)
{
    static const QMap<QString, QString> l = {
        {"layout_vertical", "Écrans superposés"}, {"layout_side", "Écrans côte à côte"},
        {"layout_large_small", "Grand écran + petit écran"}, {"layout_top", "Écran supérieur seul"},
        {"layout_bottom", "Écran inférieur seul"}, {"layout_custom", "Disposition personnalisée"},
        {"screenshot", "Capture d'écran"}, {"record", "Démarrer / arrêter l'enregistrement"},
        {"fullscreen", "Plein écran"}, {"hide_ui", "Masquer l'interface"},
        {"edit_layout", "Éditeur de disposition"}, {"toggle_input", "Contrôle à distance oui/non"},
    };
    return l.value(id, id);
}

void Settings::load()
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope, "Heivoli Network", "ScreenCTR");
    clientId = QByteArray::fromHex(s.value("clientId").toByteArray());
    if (clientId.size() != 16) {
        clientId.resize(16);
        for (int i = 0; i < 16; i++) clientId[i] = char(QRandomGenerator::system()->bounded(256));
    }
    pcName = s.value("pcName", QHostInfo::localHostName()).toString().left(31);

    consoles.clear();
    int n = s.beginReadArray("consoles");
    for (int i = 0; i < n; i++) {
        s.setArrayIndex(i);
        SavedConsole c{s.value("name").toString(), s.value("host").toString(), quint16(s.value("port", 6464).toUInt())};
        if (!c.host.isEmpty()) consoles << c;
    }
    s.endArray();
    lastConsole = s.value("lastConsole", consoles.isEmpty() ? -1 : 0).toInt();
    autoReconnect = s.value("autoReconnect", true).toBool();

    layout = sc::layoutFromId(s.value("layout", "vertical").toString().toStdString());
    custom.parse(s.value("customLayout").toString().toStdString());
    integerScale = s.value("integerScale", false).toBool();
    smooth = s.value("smooth", false).toBool();

    quality = qBound(10, s.value("quality", 70).toInt(), 95);
    fpsCap = qBound(0, s.value("fpsCap", 30).toInt(), 60);
    chroma = s.value("chroma", 0).toInt() ? 1 : 0;
    screens = qBound(1, s.value("screens", 3).toInt(), 3);

    remoteInput = s.value("remoteInput", true).toBool();
    profiles.clear();
    n = s.beginReadArray("profiles");
    for (int i = 0; i < n; i++) {
        s.setArrayIndex(i);
        sc::Profile p;
        if (p.parse(s.value("data").toString().toStdString())) profiles << p;
    }
    s.endArray();
    if (profiles.isEmpty()) {
        sc::Profile nin = sc::defaultProfile(true); nin.name = "Manette (disposition Nintendo)";
        sc::Profile xb = sc::defaultProfile(false); xb.name = "Manette (étiquettes Xbox)";
        profiles << nin << xb;
    }
    activeProfile = qBound(0, s.value("activeProfile", 0).toInt(), int(profiles.size()) - 1);

    shortcuts = defaultShortcuts();
    s.beginGroup("shortcuts");
    for (auto it = shortcuts.begin(); it != shortcuts.end(); ++it)
        if (s.contains(it.key())) it.value() = QKeySequence(s.value(it.key()).toString(), QKeySequence::PortableText);
    s.endGroup();

    QString pics = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QString vids = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    screenshotDir = s.value("screenshotDir", pics + "/ScreenCTR").toString();
    videoDir = s.value("videoDir", vids + "/ScreenCTR").toString();
    ffmpegPath = s.value("ffmpegPath").toString();
}

void Settings::save() const
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope, "Heivoli Network", "ScreenCTR");
    s.setValue("clientId", clientId.toHex());
    s.setValue("pcName", pcName);
    s.beginWriteArray("consoles", int(consoles.size()));
    for (int i = 0; i < consoles.size(); i++) {
        s.setArrayIndex(i);
        s.setValue("name", consoles[i].name); s.setValue("host", consoles[i].host); s.setValue("port", consoles[i].port);
    }
    s.endArray();
    s.setValue("lastConsole", lastConsole);
    s.setValue("autoReconnect", autoReconnect);
    s.setValue("layout", sc::layoutId(layout));
    s.setValue("customLayout", QString::fromStdString(custom.serialize()));
    s.setValue("integerScale", integerScale);
    s.setValue("smooth", smooth);
    s.setValue("quality", quality); s.setValue("fpsCap", fpsCap); s.setValue("chroma", chroma); s.setValue("screens", screens);
    s.setValue("remoteInput", remoteInput);
    s.beginWriteArray("profiles", int(profiles.size()));
    for (int i = 0; i < profiles.size(); i++) { s.setArrayIndex(i); s.setValue("data", QString::fromStdString(profiles[i].serialize())); }
    s.endArray();
    s.setValue("activeProfile", activeProfile);
    s.beginGroup("shortcuts");
    for (auto it = shortcuts.cbegin(); it != shortcuts.cend(); ++it) s.setValue(it.key(), it.value().toString(QKeySequence::PortableText));
    s.endGroup();
    s.setValue("screenshotDir", screenshotDir); s.setValue("videoDir", videoDir); s.setValue("ffmpegPath", ffmpegPath);
}

void Settings::rememberConsole(const SavedConsole &c)
{
    for (int i = 0; i < consoles.size(); i++)
        if (consoles[i].host == c.host && consoles[i].port == c.port) { consoles.removeAt(i); break; }
    consoles.prepend(c);
    while (consoles.size() > 16) consoles.removeLast();
    lastConsole = 0;
}
