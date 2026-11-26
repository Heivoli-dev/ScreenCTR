// ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
// Sends the composed input state to the console: every 8 ms (125 Hz) and
// immediately on every change, on its own UDP socket (independent of video).
#pragma once
#include "InputMapper.h"
#include <QHostAddress>
#include <QObject>
#include <QTimer>
#include <QUdpSocket>

class InputSender : public QObject {
    Q_OBJECT
public:
    explicit InputSender(QObject *parent = nullptr);
    sc::InputMapper &mapper() { return mapper_; }

    void startSession(const QString &host, quint16 port, quint32 token, bool allowed);
    void stopSession();
    void setEnabled(bool on);          // user toggle (Ctrl+I / settings)
    bool enabled() const { return enabled_; }
    bool active() const { return session_ && allowed_ && enabled_; }

public slots:
    void changed();                    // call after any mapper update: sends now

private slots:
    void tick();

private:
    void send(const sc_input &in);
    sc::InputMapper mapper_;
    QUdpSocket sock_;
    QTimer timer_;
    QHostAddress host_; quint16 port_ = 0;
    quint32 token_ = 0, seq_ = 0;
    bool session_ = false, allowed_ = false, enabled_ = true;
};
