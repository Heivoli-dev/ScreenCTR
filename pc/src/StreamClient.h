// ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
// Network client. Lives in its own QThread: TCP session, UDP video reception,
// reassembly and JPEG decoding never block the UI.
#pragma once
#include "StreamStats.h"
#include "screenctr_protocol.h"
#include <QElapsedTimer>
#include <QImage>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QUdpSocket>
#include <memory>

struct LinkStats {
    double fps[2] = {0, 0};          // frames decoded on the PC per second
    double consoleFps[2] = {0, 0};   // reported by the console
    double rttMs = -1, jitterMs = 0;
    double lossRatio = 0;
    int qualityScore = -1;
    double kbps = 0;
    int encodeUs[2] = {0, 0};
};
Q_DECLARE_METATYPE(LinkStats)

class StreamClient : public QObject {
    Q_OBJECT
public:
    enum class State { Disconnected, Connecting, WaitingApproval, Streaming, Reconnecting };
    Q_ENUM(State)
    explicit StreamClient(QObject *parent = nullptr);
    ~StreamClient() override;

public slots:
    void connectTo(const QString &host, quint16 port, const QByteArray &clientId, const QString &pcName, bool wantInput);
    void disconnectFromConsole();
    void setQuality(int quality, int fpsCap, int screens, int chroma);
    void setRemoteInputEnabled(bool on);
    void setAutoReconnect(bool on) { autoReconnect_ = on; }

signals:
    void stateChanged(StreamClient::State state, const QString &message);
    void frameReady(int screen, const QImage &image);
    void statsUpdated(const LinkStats &stats);
    // Session established: where to send controller input
    void sessionStarted(const QString &host, quint16 inputPort, quint32 token, bool inputAllowed);
    void sessionEnded();

private slots:
    void onTcpConnected();
    void onTcpReadyRead();
    void onTcpDisconnected();
    void onTcpError(QAbstractSocket::SocketError);
    void onUdpReadyRead();
    void onTick();
    void retry();

private:
    void sendCtrl(uint8_t type, const uint8_t *p = nullptr, uint16_t n = 0);
    void handleMessage(uint8_t type, const uint8_t *p, uint16_t n);
    void setState(State s, const QString &msg = {});
    void endSession(const QString &why, bool allowReconnect);

    QTcpSocket *tcp_ = nullptr;
    QUdpSocket *udp_ = nullptr;
    QTimer *tick_ = nullptr, *retry_ = nullptr;
    QByteArray rx_;
    State state_ = State::Disconnected;
    QString host_; quint16 port_ = 0; QByteArray clientId_; QString pcName_; bool wantInput_ = true;
    bool autoReconnect_ = true, userDisconnect_ = false;
    quint32 token_ = 0;
    sc_quality q_{70, 30, 3, 0};
    bool inputEnabled_ = true;

    std::unique_ptr<uint8_t[]> buffers_[2];
    sc_reassembler reasm_[2];
    sc::FpsCounter fps_[2];
    sc::RttEstimator rtt_;
    sc::LossTracker loss_;
    LinkStats last_;
    QElapsedTimer clock_;
    quint32 pingSeq_ = 0;
    qint64 bytesThisSecond_ = 0;
    int retryDelayMs_ = 1000;
};
