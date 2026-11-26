// ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
// MP4 (H.264) recording by piping raw frames to ffmpeg.exe.
#pragma once
#include <QImage>
#include <QObject>
#include <QProcess>

class Recorder : public QObject {
    Q_OBJECT
public:
    explicit Recorder(QObject *parent = nullptr);
    ~Recorder() override;
    static QString findFfmpeg(const QString &configured);
    bool start(const QString &ffmpeg, const QString &path, int w, int h, int fps);
    void addFrame(const QImage &img);   // must be w x h, Format_ARGB32
    void stop();
    bool recording() const { return proc_.state() != QProcess::NotRunning; }
    QString path() const { return path_; }
    int droppedFrames() const { return dropped_; }
signals:
    void finished(bool ok, const QString &pathOrError);
private:
    QProcess proc_;
    QString path_, log_;
    int w_ = 0, h_ = 0, dropped_ = 0;
    bool stopping_ = false;
};
