// ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
#pragma once
#include "StreamClient.h"
#include <QMainWindow>
#include <QThread>

class ScreenView;
class GamepadManager;
class InputSender;
class Recorder;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class QToolButton;
class QAction;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void keyReleaseEvent(QKeyEvent *e) override;
    void changeEvent(QEvent *e) override;
    void closeEvent(QCloseEvent *e) override;

private slots:
    void onConnectClicked();
    void onState(StreamClient::State s, const QString &msg);
    void onStats(const LinkStats &st);
    void onSessionStarted(const QString &host, quint16 inputPort, quint32 token, bool allowed);
    void onSessionEnded();
    void takeScreenshot();
    void toggleRecording();
    void toggleFullscreen();
    void toggleHideUi();
    void toggleEditLayout();
    void toggleRemoteInput();
    void openSettings();
    void setLayoutMode(int mode);

private:
    QWidget *buildTopBar();
    void buildActions();
    void applySettings();
    void refreshConsoleList();
    void toast(const QString &text);
    bool parseTarget(QString &host, quint16 &port) const;
    void updateInputLabel();
    bool isShortcutKey(const QKeyEvent *e) const;

    QThread netThread_;
    StreamClient *client_ = nullptr;
    ScreenView *view_ = nullptr;
    GamepadManager *pads_ = nullptr;
    InputSender *input_ = nullptr;
    Recorder *rec_ = nullptr;
    QTimer *recTimer_ = nullptr;
    int recW_ = 0, recH_ = 0;

    QWidget *topBar_ = nullptr;
    QComboBox *target_ = nullptr;
    QPushButton *connectBtn_ = nullptr;
    QLabel *status_ = nullptr, *fpsLbl_ = nullptr, *latLbl_ = nullptr, *qualLbl_ = nullptr, *inputLbl_ = nullptr, *toast_ = nullptr;
    QSlider *quality_ = nullptr;
    QToolButton *recBtn_ = nullptr;
    QList<QToolButton *> layoutBtns_;
    QMap<QString, QAction *> actions_;
    StreamClient::State state_ = StreamClient::State::Disconnected;
    bool uiHidden_ = false;
    QString sessionHost_;
};
