// ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
// SDL3 gamepads: Xbox, DualShock 4, DualSense, Switch Pro and every device in
// SDL's controller database (extra mappings: gamecontrollerdb.txt next to the exe).
#pragma once
#include "InputMapper.h"
#include <QObject>
#include <QStringList>
#include <QTimer>

struct SDL_Gamepad;

class GamepadManager : public QObject {
    Q_OBJECT
public:
    explicit GamepadManager(QObject *parent = nullptr);
    ~GamepadManager() override;
    bool available() const { return ok_; }
    QStringList deviceNames() const;
    QString activeName() const;

signals:
    // Full state of the active pad, polled every 4 ms
    void stateChanged(const QVector<bool> &buttons, const QVector<double> &axes);
    void devicesChanged();
    // For "press a button to bind" in the settings dialog
    void sourceActivated(const QString &source);

private slots:
    void poll();

private:
    void openAll();
    bool ok_ = false;
    QTimer timer_;
    QList<SDL_Gamepad *> pads_;
    SDL_Gamepad *active_ = nullptr;
    QVector<bool> lastButtons_;
    QVector<double> lastAxes_;
};
