// ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
#include "GamepadManager.h"
#include <QCoreApplication>
#include <QFile>
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>

using sc::PadAxis;
using sc::PadButton;

static const SDL_GamepadButton kButtonMap[] = {
    SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH,
    SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_GUIDE, SDL_GAMEPAD_BUTTON_START,
    SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK,
    SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
    SDL_GAMEPAD_BUTTON_MISC1,
};
static_assert(sizeof(kButtonMap) / sizeof(kButtonMap[0]) == size_t(PadButton::Count), "button map");
static const SDL_GamepadAxis kAxisMap[] = {
    SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY, SDL_GAMEPAD_AXIS_RIGHTX, SDL_GAMEPAD_AXIS_RIGHTY,
    SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,
};

GamepadManager::GamepadManager(QObject *parent) : QObject(parent)
{
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1"); // keep playing when OBS has focus
    ok_ = SDL_Init(SDL_INIT_GAMEPAD);
    if (!ok_) { qWarning("SDL_Init failed: %s", SDL_GetError()); return; }
    QString db = QCoreApplication::applicationDirPath() + "/gamecontrollerdb.txt";
    if (QFile::exists(db)) SDL_AddGamepadMappingsFromFile(db.toUtf8().constData());
    openAll();
    lastButtons_.fill(false, int(PadButton::Count));
    lastAxes_.fill(0.0, int(PadAxis::Count));
    timer_.setTimerType(Qt::PreciseTimer);
    connect(&timer_, &QTimer::timeout, this, &GamepadManager::poll);
    timer_.start(4);
}

GamepadManager::~GamepadManager()
{
    for (auto *p : pads_) SDL_CloseGamepad(p);
    if (ok_) SDL_Quit();
}

void GamepadManager::openAll()
{
    int n = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&n);
    for (int i = 0; i < n; i++) {
        bool known = false;
        for (auto *p : pads_) if (SDL_GetGamepadID(p) == ids[i]) known = true;
        if (known) continue;
        if (SDL_Gamepad *g = SDL_OpenGamepad(ids[i])) { pads_ << g; if (!active_) active_ = g; }
    }
    SDL_free(ids);
}

QStringList GamepadManager::deviceNames() const
{
    QStringList l;
    for (auto *p : pads_) l << QString::fromUtf8(SDL_GetGamepadName(p)) + (p == active_ ? QObject::tr(" (active)") : QString());
    return l;
}

QString GamepadManager::activeName() const { return active_ ? QString::fromUtf8(SDL_GetGamepadName(active_)) : QString(); }

void GamepadManager::poll()
{
    SDL_Event e;
    bool changed = false;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_GAMEPAD_ADDED) { openAll(); changed = true; }
        else if (e.type == SDL_EVENT_GAMEPAD_REMOVED) {
            for (int i = 0; i < pads_.size(); i++)
                if (SDL_GetGamepadID(pads_[i]) == e.gdevice.which) {
                    if (pads_[i] == active_) active_ = nullptr;
                    SDL_CloseGamepad(pads_[i]); pads_.removeAt(i); break;
                }
            if (!active_ && !pads_.isEmpty()) active_ = pads_.first();
            changed = true;
        } else if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            // The pad the player last touched becomes the active one
            for (auto *p : pads_) if (SDL_GetGamepadID(p) == e.gbutton.which && p != active_) { active_ = p; changed = true; }
        }
    }
    if (changed) emit devicesChanged();

    QVector<bool> b(int(PadButton::Count), false);
    QVector<double> a(int(PadAxis::Count), 0.0);
    if (active_) {
        for (int i = 0; i < int(PadButton::Count); i++) b[i] = SDL_GetGamepadButton(active_, kButtonMap[i]);
        for (int i = 0; i < int(PadAxis::Count); i++) {
            double v = SDL_GetGamepadAxis(active_, kAxisMap[i]) / 32767.0;
            a[i] = std::clamp(v, -1.0, 1.0);
        }
    }
    // Binding capture: report new presses / large deflections
    for (int i = 0; i < b.size(); i++)
        if (b[i] && !lastButtons_[i]) emit sourceActivated(QString("pad:") + sc::padButtonId(PadButton(i)));
    for (int i = 0; i < a.size(); i++) {
        if (std::fabs(a[i]) > 0.6 && std::fabs(lastAxes_[i]) <= 0.6)
            emit sourceActivated(QString("axis:") + sc::padAxisId(PadAxis(i)) + (a[i] > 0 ? "+" : "-"));
    }
    if (b != lastButtons_ || a != lastAxes_) emit stateChanged(b, a);
    lastButtons_ = b; lastAxes_ = a;
}
