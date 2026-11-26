// ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
#include "InputSender.h"
#include <QHostInfo>

InputSender::InputSender(QObject *parent) : QObject(parent)
{
    timer_.setTimerType(Qt::PreciseTimer);
    timer_.setInterval(8);
    connect(&timer_, &QTimer::timeout, this, &InputSender::tick);
}

void InputSender::startSession(const QString &host, quint16 port, quint32 token, bool allowed)
{
    QHostAddress a(host);
    if (a.isNull()) {
        QHostInfo hi = QHostInfo::fromName(host);
        for (const auto &x : hi.addresses()) if (x.protocol() == QAbstractSocket::IPv4Protocol) { a = x; break; }
    }
    host_ = a; port_ = port; token_ = token; seq_ = 0;
    session_ = !a.isNull(); allowed_ = allowed;
    if (active()) timer_.start();
}

void InputSender::stopSession()
{
    if (active()) { sc_input n; sc_input_neutral(&n); send(n); } // console also releases on its own
    session_ = false;
    timer_.stop();
}

void InputSender::setEnabled(bool on)
{
    if (enabled_ == on) return;
    if (!on && active()) { sc_input n; sc_input_neutral(&n); send(n); }
    enabled_ = on;
    if (active()) timer_.start(); else timer_.stop();
}

void InputSender::changed() { if (active()) send(mapper_.compose()); }
void InputSender::tick() { if (active()) send(mapper_.compose()); }

void InputSender::send(const sc_input &state)
{
    sc_input in = state;
    in.session_token = token_;
    in.seq = ++seq_;
    uint8_t buf[SC_INPUT_SIZE];
    sc_input_write(buf, &in);
    sock_.writeDatagram(reinterpret_cast<const char *>(buf), SC_INPUT_SIZE, host_, port_);
}
