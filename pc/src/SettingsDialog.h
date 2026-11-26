// ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
#pragma once
#include "Settings.h"
#include <QDialog>

class GamepadManager;
class QComboBox;
class QTableWidget;
class QSlider;
class QLabel;
class QCheckBox;
class QLineEdit;
class QSpinBox;
class QKeySequenceEdit;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    SettingsDialog(GamepadManager *pads, QWidget *parent = nullptr);
    void accept() override;

protected:
    void keyPressEvent(QKeyEvent *e) override;

private:
    QWidget *buildGeneral();
    QWidget *buildVideo();
    QWidget *buildShortcuts();
    QWidget *buildControls();
    void loadProfile(int index);
    void storeProfile();
    void refreshBindings();
    void startCapture(int targetRow);
    void finishCapture(const QString &source);

    GamepadManager *pads_;
    Settings work_;                       // edited copy, committed on OK
    // general
    QCheckBox *autoReconnect_, *integer_, *smooth_;
    QLineEdit *pcName_, *shotDir_, *vidDir_, *ffmpeg_;
    // video
    QSlider *quality_; QComboBox *fps_, *chroma_; QCheckBox *top_, *bottom_;
    // shortcuts
    QMap<QString, QKeySequenceEdit *> keyEdits_;
    // controls
    QCheckBox *remote_;
    QComboBox *profile_;
    QTableWidget *bindings_;
    QSlider *dzL_, *dzR_, *trig_;
    QLabel *devices_, *captureHint_;
    int captureRow_ = -1;
};
