// ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
#include "StreamClient.h"
#include <QNetworkDatagram>
#include <cstring>

StreamClient::StreamClient(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<LinkStats>();
    qRegisterMetaType<StreamClient::State>();
    for (int i = 0; i < 2; i++) {
        buffers_[i].reset(new uint8_t[SC_MAX_FRAME_BYTES]);
        sc_reasm_init(&reasm_[i], buffers_[i].get());
    }
    clock_.start();
}

StreamClient::~StreamClient()
{
    if (tcp_ && tcp_->state() == QAbstractSocket::ConnectedState) { sendCtrl(SC_MSG_BYE); tcp_->flush(); }
}

void StreamClient::setState(State s, const QString &msg)
{
    state_ = s;
    emit stateChanged(s, msg);
}

void StreamClient::connectTo(const QString &host, quint16 port, const QByteArray &clientId, const QString &pcName, bool wantInput)
{
    // Objects are created lazily so they belong to the worker thread
    if (!tcp_) {
        tcp_ = new QTcpSocket(this);
        connect(tcp_, &QTcpSocket::connected, this, &StreamClient::onTcpConnected);
        connect(tcp_, &QTcpSocket::readyRead, this, &StreamClient::onTcpReadyRead);
        connect(tcp_, &QTcpSocket::disconnected, this, &StreamClient::onTcpDisconnected);
        connect(tcp_, &QTcpSocket::errorOccurred, this, &StreamClient::onTcpError);
        udp_ = new QUdpSocket(this);
        connect(udp_, &QUdpSocket::readyRead, this, &StreamClient::onUdpReadyRead);
        tick_ = new QTimer(this); tick_->setInterval(1000);
        connect(tick_, &QTimer::timeout, this, &StreamClient::onTick);
        retry_ = new QTimer(this); retry_->setSingleShot(true);
        connect(retry_, &QTimer::timeout, this, &StreamClient::retry);
    }
    host_ = host; port_ = port; clientId_ = clientId; pcName_ = pcName; wantInput_ = wantInput;
    userDisconnect_ = false; retryDelayMs_ = 1000;
    retry_->stop();
    tcp_->abort();
    if (udp_->state() != QAbstractSocket::BoundState) {
        // Default video port; fall back to any free port (sent in HELLO)
        if (!udp_->bind(QHostAddress::AnyIPv4, SC_PORT_VIDEO) && !udp_->bind(QHostAddress::AnyIPv4, 0)) {
            setState(State::Disconnected, tr("Impossible d'ouvrir le port vidéo UDP : %1").arg(udp_->errorString()));
            return;
        }
        udp_->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 1 << 20);
    }
    rx_.clear();
    setState(State::Connecting, tr("Connexion à %1:%2…").arg(host).arg(port));
    tcp_->connectToHost(host, port);
}

void StreamClient::retry()
{
    if (userDisconnect_ || host_.isEmpty()) return;
    tcp_->abort();
    rx_.clear();
    setState(State::Reconnecting, tr("Reconnexion à %1:%2…").arg(host_).arg(port_));
    tcp_->connectToHost(host_, port_);
}

void StreamClient::disconnectFromConsole()
{
    userDisconnect_ = true;
    if (retry_) retry_->stop();
    if (tcp_ && tcp_->state() == QAbstractSocket::ConnectedState) { sendCtrl(SC_MSG_BYE); tcp_->flush(); }
    endSession(tr("Déconnecté"), false);
    if (tcp_) tcp_->abort();
}

void StreamClient::endSession(const QString &why, bool allowReconnect)
{
    bool wasActive = state_ == State::Streaming;
    if (tick_) tick_->stop();
    token_ = 0;
    if (wasActive) emit sessionEnded();
    if (allowReconnect && autoReconnect_ && !userDisconnect_ && retry_) {
        setState(State::Reconnecting, why + tr(" — nouvelle tentative dans %1 s").arg(retryDelayMs_ / 1000.0, 0, 'f', 0));
        retry_->start(retryDelayMs_);
        retryDelayMs_ = qMin(retryDelayMs_ * 2, 8000);
    } else {
        if (retry_) retry_->stop();
        setState(State::Disconnected, why);
    }
}

void StreamClient::sendCtrl(uint8_t type, const uint8_t *p, uint16_t n)
{
    uint8_t buf[SC_MAX_CTRL_PAYLOAD + 3];
    size_t m = sc_write_ctrl(buf, sizeof buf, type, p, n);
    if (m && tcp_) tcp_->write(reinterpret_cast<const char *>(buf), qint64(m));
}

void StreamClient::onTcpConnected()
{
    tcp_->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    sc_hello h{};
    h.magic = SC_HELLO_MAGIC; h.version = SC_PROTO_VERSION;
    h.flags = wantInput_ ? SC_HELLO_WANTS_INPUT : 0;
    h.video_port = udp_->localPort();
    QByteArray name = pcName_.toUtf8().left(SC_NAME_LEN - 1);
    std::memcpy(h.name, name.constData(), size_t(name.size()));
    std::memcpy(h.client_id, clientId_.constData(), 16);
    uint8_t p[SC_HELLO_SIZE];
    sc_hello_write(p, &h);
    sendCtrl(SC_MSG_HELLO, p, SC_HELLO_SIZE);
}

void StreamClient::onTcpReadyRead()
{
    rx_.append(tcp_->readAll());
    for (;;) {
        uint8_t t; const uint8_t *p; uint16_t n;
        int used = sc_parse_ctrl(reinterpret_cast<const uint8_t *>(rx_.constData()), size_t(rx_.size()), &t, &p, &n);
        if (used < 0) { tcp_->abort(); endSession(tr("Réponse invalide de la console"), true); return; }
        if (used == 0) break;
        QByteArray payload(reinterpret_cast<const char *>(p), n);  // copy: rx_ is modified below
        rx_.remove(0, used);
        handleMessage(t, reinterpret_cast<const uint8_t *>(payload.constData()), n);
        if (!tcp_ || tcp_->state() != QAbstractSocket::ConnectedState) return;
    }
}

void StreamClient::handleMessage(uint8_t type, const uint8_t *p, uint16_t n)
{
    switch (type) {
    case SC_MSG_AUTH_PENDING:
        setState(State::WaitingApproval, tr("Acceptez la connexion sur la console (A ou Y)"));
        break;
    case SC_MSG_WELCOME: {
        sc_welcome w;
        if (sc_welcome_read(p, n, &w) != 0) return;
        token_ = w.session_token;
        retryDelayMs_ = 1000;
        for (auto &r : reasm_) { uint8_t *b = r.buf; sc_reasm_init(&r, b); }
        loss_.takeLossRatio();
        setState(State::Streaming, w.input_allowed ? tr("Diffusion en cours — contrôle à distance autorisé")
                                                   : tr("Diffusion en cours — contrôle à distance non autorisé"));
        uint8_t q[SC_QUALITY_SIZE]; sc_quality_write(q, &q_);
        sendCtrl(SC_MSG_SET_QUALITY, q, sizeof q);
        uint8_t e = inputEnabled_ ? 1 : 0; sendCtrl(SC_MSG_SET_INPUT, &e, 1);
        emit sessionStarted(host_, w.input_port, w.session_token, w.input_allowed != 0);
        tick_->start();
        onTick();
        break;
    }
    case SC_MSG_REJECT: {
        int r = n ? p[0] : 0;
        QString why = r == SC_REJECT_DENIED ? tr("Connexion refusée sur la console")
                    : r == SC_REJECT_BUSY ? tr("La console diffuse déjà vers un autre PC")
                    : r == SC_REJECT_VERSION ? tr("Versions de ScreenCTR incompatibles (console / PC)")
                    : r == SC_REJECT_TIMEOUT ? tr("Pas de réponse sur la console (délai dépassé)") : tr("Connexion refusée");
        userDisconnect_ = r == SC_REJECT_DENIED || r == SC_REJECT_VERSION; // do not insist
        tcp_->abort();
        endSession(why, !userDisconnect_);
        break;
    }
    case SC_MSG_PONG: {
        sc_ping pg;
        if (sc_ping_read(p, n, &pg) == 0) rtt_.sample(double(quint32(clock_.elapsed()) - pg.time_ms));
        break;
    }
    case SC_MSG_PING: sendCtrl(SC_MSG_PONG, p, n); break;
    case SC_MSG_STATS: {
        sc_stats s;
        if (sc_stats_read(p, n, &s) == 0)
            for (int i = 0; i < 2; i++) { last_.consoleFps[i] = s.fps_x10[i] / 10.0; last_.encodeUs[i] = s.encode_us[i]; }
        break;
    }
    case SC_MSG_BYE:
        userDisconnect_ = true; // stopped on the console: do not reconnect behind the user's back
        tcp_->abort();
        endSession(tr("Diffusion arrêtée depuis la console"), false);
        break;
    default: break;
    }
}

void StreamClient::onTcpDisconnected()
{
    if (state_ != State::Disconnected && state_ != State::Reconnecting) endSession(tr("Connexion perdue"), true);
}

void StreamClient::onTcpError(QAbstractSocket::SocketError)
{
    if (state_ == State::Connecting || state_ == State::Reconnecting)
        endSession(tr("Console injoignable (%1)").arg(tcp_->errorString()), true);
}

void StreamClient::onUdpReadyRead()
{
    while (udp_->hasPendingDatagrams()) {
        QNetworkDatagram d = udp_->receiveDatagram(SC_VIDEO_MAX_DATAGRAM + 64);
        if (!token_) continue;
        const QByteArray &b = d.data();
        sc_video_hdr h;
        if (sc_video_hdr_read(reinterpret_cast<const uint8_t *>(b.constData()), size_t(b.size()), &h) != 0) continue;
        if (h.session_token != token_) continue;
        bytesThisSecond_ += b.size();
        sc_reassembler &r = reasm_[h.screen];
        if (!sc_reasm_feed(&r, &h, reinterpret_cast<const uint8_t *>(b.constData()) + SC_VIDEO_HDR_SIZE, size_t(b.size()) - SC_VIDEO_HDR_SIZE))
            continue;
        QImage img = QImage::fromData(r.buf, int(r.size), "JPG");
        if (img.isNull()) continue;
        loss_.completed(h.screen, r.frame_id);
        fps_[h.screen].frame(clock_.elapsed());
        emit frameReady(h.screen, img.convertToFormat(QImage::Format_RGB32));
    }
}

void StreamClient::onTick()
{
    if (state_ != State::Streaming) return;
    sc_ping pg{++pingSeq_, quint32(clock_.elapsed())};
    uint8_t p[SC_PING_SIZE]; sc_ping_write(p, &pg);
    sendCtrl(SC_MSG_PING, p, sizeof p);

    qint64 now = clock_.elapsed();
    for (int i = 0; i < 2; i++) last_.fps[i] = fps_[i].fps(now);
    last_.rttMs = rtt_.valid() ? rtt_.srtt() : -1;
    last_.jitterMs = rtt_.jitter();
    last_.lossRatio = loss_.takeLossRatio();
    last_.qualityScore = rtt_.valid() ? sc::qualityScore(last_.lossRatio, last_.rttMs, last_.jitterMs) : -1;
    last_.kbps = bytesThisSecond_ * 8.0 / 1000.0;
    bytesThisSecond_ = 0;
    emit statsUpdated(last_);
}

void StreamClient::setQuality(int quality, int fpsCap, int screens, int chroma)
{
    q_ = {uint8_t(qBound(10, quality, 95)), uint8_t(qBound(0, fpsCap, 60)), uint8_t(screens & 3), uint8_t(chroma ? 1 : 0)};
    if (state_ == State::Streaming) { uint8_t p[SC_QUALITY_SIZE]; sc_quality_write(p, &q_); sendCtrl(SC_MSG_SET_QUALITY, p, sizeof p); }
}

void StreamClient::setRemoteInputEnabled(bool on)
{
    inputEnabled_ = on;
    if (state_ == State::Streaming) { uint8_t e = on ? 1 : 0; sendCtrl(SC_MSG_SET_INPUT, &e, 1); }
}
