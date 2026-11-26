// ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
#include "Recorder.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QStandardPaths>

Recorder::Recorder(QObject *parent) : QObject(parent)
{
    proc_.setProcessChannelMode(QProcess::MergedChannels);
    connect(&proc_, &QProcess::readyRead, this, [this] { log_ += QString::fromUtf8(proc_.readAll()).right(4000); log_ = log_.right(4000); });
    connect(&proc_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus st) {
        bool ok = st == QProcess::NormalExit && code == 0;
        emit finished(ok, ok ? path_ : tr("FFmpeg a échoué (code %1) :\n%2").arg(code).arg(log_.right(600)));
        stopping_ = false;
    });
}

Recorder::~Recorder() { stop(); proc_.waitForFinished(5000); }

QString Recorder::findFfmpeg(const QString &configured)
{
    if (!configured.isEmpty() && QFileInfo(configured).isExecutable()) return configured;
    QString local = QCoreApplication::applicationDirPath() + "/ffmpeg.exe";
    if (QFileInfo(local).isExecutable()) return local;
    return QStandardPaths::findExecutable("ffmpeg");
}

bool Recorder::start(const QString &ffmpeg, const QString &path, int w, int h, int fps)
{
    if (recording() || ffmpeg.isEmpty()) return false;
    w_ = w; h_ = h; path_ = path; dropped_ = 0; log_.clear(); stopping_ = false;
    QStringList args = {
        "-hide_banner", "-loglevel", "error", "-y",
        "-f", "rawvideo", "-pix_fmt", "bgra", "-s", QString("%1x%2").arg(w).arg(h), "-r", QString::number(fps),
        "-i", "-",
        "-c:v", "libx264", "-preset", "veryfast", "-crf", "18", "-pix_fmt", "yuv420p",
        "-movflags", "+faststart", path,
    };
    proc_.start(ffmpeg, args);
    return proc_.waitForStarted(3000);
}

void Recorder::addFrame(const QImage &img)
{
    if (!recording() || stopping_ || img.width() != w_ || img.height() != h_) return;
    // Never let a slow encoder grow memory without bound: drop instead (~1 s backlog)
    if (proc_.bytesToWrite() > qint64(w_) * h_ * 4 * 30) { dropped_++; return; }
    proc_.write(reinterpret_cast<const char *>(img.constBits()), img.sizeInBytes());
}

void Recorder::stop()
{
    if (!recording() || stopping_) return;
    stopping_ = true;
    proc_.closeWriteChannel(); // ffmpeg finalises the MP4 and exits
}
