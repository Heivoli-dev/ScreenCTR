// ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
#include "MainWindow.h"
#include "GamepadManager.h"
#include "InputSender.h"
#include "KeyMap.h"
#include "Recorder.h"
#include "ScreenView.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <functional>

using sc::LayoutMode;

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("ScreenCTR");
    resize(880, 760);
    Settings &S = Settings::get();

    // Network worker thread
    client_ = new StreamClient;
    client_->moveToThread(&netThread_);
    connect(&netThread_, &QThread::finished, client_, &QObject::deleteLater);
    netThread_.start(QThread::TimeCriticalPriority);

    view_ = new ScreenView;
    pads_ = new GamepadManager(this);
    input_ = new InputSender(this);
    rec_ = new Recorder(this);

    auto *central = new QWidget; auto *v = new QVBoxLayout(central);
    v->setContentsMargins(0, 0, 0, 0); v->setSpacing(0);
    topBar_ = buildTopBar();
    v->addWidget(topBar_);
    v->addWidget(view_, 1);
    setCentralWidget(central);

    inputLbl_ = new QLabel; statusBar()->addPermanentWidget(inputLbl_);
    status_ = new QLabel; statusBar()->addWidget(status_, 1);
    toast_ = new QLabel(view_); toast_->setObjectName("toast"); toast_->hide();

    buildActions();

    connect(client_, &StreamClient::frameReady, view_, &ScreenView::setFrame);
    connect(client_, &StreamClient::stateChanged, this, &MainWindow::onState);
    connect(client_, &StreamClient::statsUpdated, this, &MainWindow::onStats);
    connect(client_, &StreamClient::sessionStarted, this, &MainWindow::onSessionStarted);
    connect(client_, &StreamClient::sessionEnded, this, &MainWindow::onSessionEnded);
    connect(view_, &ScreenView::touchChanged, this, [this](bool down, int x, int y) { input_->mapper().setTouch(down, x, y); input_->changed(); });
    connect(view_, &ScreenView::customLayoutEdited, this, [](const sc::CustomLayout &c) { Settings::get().custom = c; Settings::get().save(); });
    connect(pads_, &GamepadManager::stateChanged, this, [this](const QVector<bool> &b, const QVector<double> &a) {
        for (int i = 0; i < b.size(); i++) input_->mapper().setPadButton(sc::PadButton(i), b[i]);
        for (int i = 0; i < a.size(); i++) input_->mapper().setPadAxis(sc::PadAxis(i), a[i]);
        input_->changed();
    });
    connect(pads_, &GamepadManager::devicesChanged, this, [this] {
        QString n = pads_->activeName();
        toast(n.isEmpty() ? tr("Manette déconnectée") : tr("Manette : %1").arg(n));
        updateInputLabel();
    });
    connect(rec_, &Recorder::finished, this, [this](bool ok, const QString &r) {
        recBtn_->setChecked(false);
        if (ok) toast(tr("Vidéo enregistrée : %1").arg(QDir::toNativeSeparators(r)));
        else QMessageBox::warning(this, tr("Enregistrement"), r);
    });
    recTimer_ = new QTimer(this);
    recTimer_->setTimerType(Qt::PreciseTimer);
    connect(recTimer_, &QTimer::timeout, this, [this] { rec_->addFrame(view_->renderComposite(recW_, recH_)); });

    applySettings();
    refreshConsoleList();
    updateInputLabel();
    onState(StreamClient::State::Disconnected, tr("Prêt — saisissez l'adresse affichée par la console (L+R+START)"));

    // Auto-reconnect to the last console at start-up
    if (S.autoReconnect && S.lastConsole >= 0 && S.lastConsole < S.consoles.size())
        QTimer::singleShot(300, this, &MainWindow::onConnectClicked);
}

MainWindow::~MainWindow()
{
    QMetaObject::invokeMethod(client_, "disconnectFromConsole", Qt::BlockingQueuedConnection);
    netThread_.quit();
    netThread_.wait(2000);
}

QWidget *MainWindow::buildTopBar()
{
    auto *bar = new QWidget; bar->setObjectName("topBar");
    auto *h = new QHBoxLayout(bar); h->setContentsMargins(10, 8, 10, 8); h->setSpacing(8);

    auto *logo = new QLabel("<b>Screen<span style='color:#3DDC97'>CTR</span></b>");
    logo->setObjectName("logo");
    target_ = new QComboBox; target_->setEditable(true); target_->setMinimumWidth(220);
    target_->lineEdit()->setPlaceholderText(tr("192.168.1.42:6464"));
    connectBtn_ = new QPushButton(tr("Connecter")); connectBtn_->setObjectName("primary");
    connect(connectBtn_, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(target_->lineEdit(), &QLineEdit::returnPressed, this, &MainWindow::onConnectClicked);

    fpsLbl_ = new QLabel("— FPS"); latLbl_ = new QLabel("— ms"); qualLbl_ = new QLabel("●");
    for (auto *l : {fpsLbl_, latLbl_, qualLbl_}) l->setObjectName("chip");
    fpsLbl_->setToolTip(tr("Images par seconde reçues (écran supérieur / inférieur)"));
    latLbl_->setToolTip(tr("Latence réseau aller-retour (PING/PONG)"));
    qualLbl_->setToolTip(tr("Qualité de connexion (pertes, latence, gigue)"));

    quality_ = new QSlider(Qt::Horizontal); quality_->setRange(10, 95); quality_->setFixedWidth(110);
    quality_->setToolTip(tr("Qualité vidéo"));
    connect(quality_, &QSlider::sliderReleased, this, [this] {
        Settings &S = Settings::get(); S.quality = quality_->value(); S.save();
        QMetaObject::invokeMethod(client_, [c = client_, q = S.quality, f = S.fpsCap, s = S.screens, ch = S.chroma] { c->setQuality(q, f, s, ch); });
        toast(tr("Qualité %1").arg(S.quality));
    });

    h->addWidget(logo); h->addSpacing(6); h->addWidget(target_); h->addWidget(connectBtn_);
    h->addSpacing(10); h->addWidget(fpsLbl_); h->addWidget(latLbl_); h->addWidget(qualLbl_);
    h->addStretch(1);
    h->addWidget(new QLabel(tr("Qualité"))); h->addWidget(quality_);

    const char *icons[] = {"▤", "▥", "◧", "▭", "▢", "✎"};
    const char *tips[] = {"Superposés (F1)", "Côte à côte (F2)", "Grand + petit (F3)", "Écran supérieur (F4)", "Écran inférieur (F5)", "Personnalisée (F8)"};
    for (int i = 0; i < 6; i++) {
        auto *b = new QToolButton; b->setText(QString::fromUtf8(icons[i])); b->setToolTip(tr(tips[i])); b->setCheckable(true);
        connect(b, &QToolButton::clicked, this, [this, i] { setLayoutMode(i); });
        layoutBtns_ << b; h->addWidget(b);
    }
    auto mk = [&](const QString &txt, const QString &tip, auto slot, bool checkable = false) {
        auto *b = new QToolButton; b->setText(txt); b->setToolTip(tip); b->setCheckable(checkable);
        connect(b, &QToolButton::clicked, this, slot); h->addWidget(b); return b;
    };
    mk("📷", tr("Capture d'écran PNG (F6)"), &MainWindow::takeScreenshot);
    recBtn_ = mk("⏺", tr("Enregistrer en MP4 (F7)"), &MainWindow::toggleRecording, true);
    recBtn_->setObjectName("rec");
    mk("⛶", tr("Plein écran (F11)"), &MainWindow::toggleFullscreen);
    mk("◌", tr("Masquer l'interface (Ctrl+H) — idéal pour OBS"), &MainWindow::toggleHideUi);
    mk("⚙", tr("Paramètres"), &MainWindow::openSettings);
    return bar;
}

void MainWindow::buildActions()
{
    struct A { const char *id; std::function<void()> fn; };
    const QList<A> list = {
        {"layout_vertical", [this] { setLayoutMode(0); }}, {"layout_side", [this] { setLayoutMode(1); }},
        {"layout_large_small", [this] { setLayoutMode(2); }}, {"layout_top", [this] { setLayoutMode(3); }},
        {"layout_bottom", [this] { setLayoutMode(4); }}, {"layout_custom", [this] { setLayoutMode(5); }},
        {"screenshot", [this] { takeScreenshot(); }}, {"record", [this] { toggleRecording(); }},
        {"fullscreen", [this] { toggleFullscreen(); }}, {"hide_ui", [this] { toggleHideUi(); }},
        {"edit_layout", [this] { toggleEditLayout(); }}, {"toggle_input", [this] { toggleRemoteInput(); }},
    };
    for (const auto &a : list) {
        auto *act = new QAction(this);
        act->setShortcutContext(Qt::ApplicationShortcut);
        connect(act, &QAction::triggered, this, a.fn);
        addAction(act); // works even when the toolbar is hidden
        actions_[a.id] = act;
    }
}

void MainWindow::applySettings()
{
    Settings &S = Settings::get();
    for (auto it = actions_.begin(); it != actions_.end(); ++it) it.value()->setShortcut(S.shortcuts.value(it.key()));
    view_->setIntegerScale(S.integerScale);
    view_->setSmooth(S.smooth);
    view_->setCustomLayout(S.custom);
    setLayoutMode(int(S.layout));
    quality_->setValue(S.quality);
    input_->mapper().setProfile(S.profiles.value(S.activeProfile, sc::defaultProfile()));
    input_->setEnabled(S.remoteInput);
    view_->setTouchEnabled(S.remoteInput);
    QMetaObject::invokeMethod(client_, [c = client_, S] {
        c->setAutoReconnect(S.autoReconnect);
        c->setQuality(S.quality, S.fpsCap, S.screens, S.chroma);
        c->setRemoteInputEnabled(S.remoteInput);
    });
    updateInputLabel();
}

void MainWindow::refreshConsoleList()
{
    Settings &S = Settings::get();
    QString current = target_->currentText();
    target_->clear();
    for (const auto &c : S.consoles) target_->addItem(c.label(), QString("%1:%2").arg(c.host).arg(c.port));
    if (S.lastConsole >= 0 && S.lastConsole < target_->count()) target_->setCurrentIndex(S.lastConsole);
    else target_->setEditText(current);
}

bool MainWindow::parseTarget(QString &host, quint16 &port) const
{
    QString t = target_->currentText().trimmed();
    int idx = target_->findText(t);
    if (idx >= 0 && !target_->itemData(idx).toString().isEmpty()) t = target_->itemData(idx).toString();
    if (t.contains(" — ")) t = t.section(" — ", 1);
    port = SC_PORT_CONTROL;
    int colon = t.lastIndexOf(':');
    if (colon > 0) {
        bool ok; uint p = t.mid(colon + 1).toUInt(&ok);
        if (!ok || p == 0 || p > 65535) return false;
        port = quint16(p); t = t.left(colon);
    }
    host = t;
    return !host.isEmpty();
}

void MainWindow::onConnectClicked()
{
    if (state_ != StreamClient::State::Disconnected) {
        QMetaObject::invokeMethod(client_, "disconnectFromConsole");
        return;
    }
    QString host; quint16 port;
    if (!parseTarget(host, port)) { toast(tr("Adresse invalide — exemple : 192.168.1.42:6464")); return; }
    Settings &S = Settings::get();
    QMetaObject::invokeMethod(client_, [c = client_, host, port, id = S.clientId, name = S.pcName, want = S.remoteInput] {
        c->connectTo(host, port, id, name, want);
    });
}

void MainWindow::onState(StreamClient::State s, const QString &msg)
{
    state_ = s;
    status_->setText(msg);
    using St = StreamClient::State;
    connectBtn_->setText(s == St::Disconnected ? tr("Connecter") : tr("Déconnecter"));
    target_->setEnabled(s == St::Disconnected);
    if (s == St::Streaming) {
        QString host; quint16 port;
        if (parseTarget(host, port)) {
            SavedConsole c; c.host = host; c.port = port;
            for (const auto &o : Settings::get().consoles) if (o.host == host && o.port == port) c.name = o.name;
            Settings::get().rememberConsole(c); Settings::get().save();
            QSignalBlocker b(target_); refreshConsoleList();
        }
        view_->setPlaceholder(QString());
    } else if (s == St::Disconnected) {
        view_->clearFrames();
        view_->setPlaceholder(tr("Aucune console connectée\n\nSur la 3DS : L+R+START, puis saisissez l'adresse affichée ci-dessus."));
        fpsLbl_->setText("— FPS"); latLbl_->setText("— ms"); qualLbl_->setText("●"); qualLbl_->setStyleSheet(QString());
    } else if (s == St::WaitingApproval) {
        view_->setPlaceholder(tr("Acceptez la connexion sur la console\n[A] Autoriser   [Y] Autoriser et mémoriser   [B] Refuser"));
    } else {
        view_->setPlaceholder(msg);
    }
}

void MainWindow::onStats(const LinkStats &st)
{
    fpsLbl_->setText(QString("%1 / %2 FPS").arg(st.fps[0], 0, 'f', 0).arg(st.fps[1], 0, 'f', 0));
    latLbl_->setText(st.rttMs < 0 ? "— ms" : QString("%1 ms").arg(st.rttMs, 0, 'f', 0));
    auto q = sc::qualityFromScore(st.qualityScore);
    static const char *colors[] = {"#8A93A6", "#3DDC97", "#9BE15D", "#FFB347", "#FF5C5C"};
    qualLbl_->setText(QString("● %1").arg(QString::fromUtf8(sc::qualityLabel(st.qualityScore < 0 ? sc::LinkQuality::Unknown : q))));
    qualLbl_->setStyleSheet(QString("color:%1").arg(colors[st.qualityScore < 0 ? 0 : int(q)]));
    statusBar()->showMessage(tr("Console : %1 / %2 FPS · encodage %3 / %4 ms · %5 kbit/s · pertes %6 %")
                                 .arg(st.consoleFps[0], 0, 'f', 0).arg(st.consoleFps[1], 0, 'f', 0)
                                 .arg(st.encodeUs[0] / 1000.0, 0, 'f', 1).arg(st.encodeUs[1] / 1000.0, 0, 'f', 1)
                                 .arg(st.kbps, 0, 'f', 0).arg(st.lossRatio * 100, 0, 'f', 1));
}

void MainWindow::onSessionStarted(const QString &host, quint16 inputPort, quint32 token, bool allowed)
{
    sessionHost_ = host;
    input_->startSession(host, inputPort, token, allowed);
    updateInputLabel();
}

void MainWindow::onSessionEnded()
{
    input_->stopSession();
    input_->mapper().setTouch(false, 0, 0);
    if (rec_->recording()) toggleRecording();
    updateInputLabel();
}

void MainWindow::updateInputLabel()
{
    QString pad = pads_->available() ? pads_->activeName() : tr("SDL indisponible");
    QString st = !Settings::get().remoteInput ? tr("Contrôle à distance désactivé")
               : input_->active() ? tr("Contrôle à distance actif") : tr("Contrôle à distance en attente");
    inputLbl_->setText(QString("%1 · %2").arg(st, pad.isEmpty() ? tr("aucune manette") : pad));
}

void MainWindow::setLayoutMode(int m)
{
    m = qBound(0, m, 5);
    view_->setLayoutMode(LayoutMode(m));
    for (int i = 0; i < layoutBtns_.size(); i++) layoutBtns_[i]->setChecked(i == m);
    if (Settings::get().layout != LayoutMode(m)) { Settings::get().layout = LayoutMode(m); Settings::get().save(); toast(QString::fromUtf8(sc::layoutName(LayoutMode(m)))); }
}

void MainWindow::takeScreenshot()
{
    if (!view_->hasFrames()) { toast(tr("Rien à capturer")); return; }
    int w, h; sc::layoutNativeSize(view_->layoutMode(), view_->customLayout(), w, h);
    QImage img = view_->renderComposite(w, h);
    QDir().mkpath(Settings::get().screenshotDir);
    QString path = Settings::get().screenshotDir + "/ScreenCTR_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz") + ".png";
    if (img.save(path, "PNG")) toast(tr("Capture enregistrée : %1").arg(QDir::toNativeSeparators(path)));
    else toast(tr("Échec de l'enregistrement de la capture"));
}

void MainWindow::toggleRecording()
{
    if (rec_->recording()) { recTimer_->stop(); rec_->stop(); toast(tr("Finalisation de la vidéo…")); return; }
    if (state_ != StreamClient::State::Streaming) { recBtn_->setChecked(false); toast(tr("Connectez une console avant d'enregistrer")); return; }
    QString ff = Recorder::findFfmpeg(Settings::get().ffmpegPath);
    if (ff.isEmpty()) {
        recBtn_->setChecked(false);
        QMessageBox::information(this, tr("FFmpeg introuvable"), tr("Placez ffmpeg.exe à côté de ScreenCTR.exe ou indiquez son chemin dans Paramètres > Général."));
        return;
    }
    // Recording at 2x native size keeps pixels crisp after YouTube re-encoding
    sc::layoutNativeSize(view_->layoutMode(), view_->customLayout(), recW_, recH_);
    recW_ *= 2; recH_ *= 2;
    QDir().mkpath(Settings::get().videoDir);
    QString path = Settings::get().videoDir + "/ScreenCTR_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".mp4";
    if (!rec_->start(ff, path, recW_, recH_, 30)) { recBtn_->setChecked(false); toast(tr("Impossible de lancer FFmpeg")); return; }
    recBtn_->setChecked(true);
    recTimer_->start(1000 / 30);
    toast(tr("Enregistrement démarré"));
}

void MainWindow::toggleFullscreen()
{
    if (isFullScreen()) showNormal(); else showFullScreen();
}

void MainWindow::toggleHideUi()
{
    uiHidden_ = !uiHidden_;
    topBar_->setVisible(!uiHidden_);
    statusBar()->setVisible(!uiHidden_);
    if (uiHidden_) toast(tr("Interface masquée — %1 pour l'afficher").arg(Settings::get().shortcuts.value("hide_ui").toString(QKeySequence::NativeText)));
}

void MainWindow::toggleEditLayout()
{
    if (view_->layoutMode() != LayoutMode::Custom) setLayoutMode(int(LayoutMode::Custom));
    view_->setEditMode(!view_->editMode());
}

void MainWindow::toggleRemoteInput()
{
    Settings &S = Settings::get();
    S.remoteInput = !S.remoteInput; S.save();
    input_->setEnabled(S.remoteInput);
    view_->setTouchEnabled(S.remoteInput);
    QMetaObject::invokeMethod(client_, [c = client_, on = S.remoteInput] { c->setRemoteInputEnabled(on); });
    toast(S.remoteInput ? tr("Contrôle à distance activé") : tr("Contrôle à distance désactivé"));
    updateInputLabel();
}

void MainWindow::openSettings()
{
    SettingsDialog d(pads_, this);
    if (d.exec() == QDialog::Accepted) applySettings();
}

void MainWindow::toast(const QString &text)
{
    toast_->setText(text);
    toast_->adjustSize();
    toast_->move((view_->width() - toast_->width()) / 2, view_->height() - toast_->height() - 24);
    toast_->show(); toast_->raise();
    static QTimer *t = nullptr;
    if (!t) { t = new QTimer(this); t->setSingleShot(true); connect(t, &QTimer::timeout, toast_, &QWidget::hide); }
    t->start(2200);
}

bool MainWindow::isShortcutKey(const QKeyEvent *e) const
{
    QKeySequence k(e->keyCombination());
    for (auto *a : actions_) if (!a->shortcut().isEmpty() && a->shortcut() == k) return true;
    return false;
}

void MainWindow::keyPressEvent(QKeyEvent *e)
{
    if (!e->isAutoRepeat() && !isShortcutKey(e) && !target_->hasFocus()) {
        if (int h = hidScancodeFromEvent(e)) { input_->mapper().setKey(h, true); input_->changed(); return; }
    }
    QMainWindow::keyPressEvent(e);
}

void MainWindow::keyReleaseEvent(QKeyEvent *e)
{
    if (!e->isAutoRepeat()) if (int h = hidScancodeFromEvent(e)) { input_->mapper().setKey(h, false); input_->changed(); }
    QMainWindow::keyReleaseEvent(e);
}

void MainWindow::changeEvent(QEvent *e)
{
    // Losing focus must never leave a key stuck down on the console
    if (e->type() == QEvent::ActivationChange && !isActiveWindow()) {
        sc::InputMapper &m = input_->mapper();
        for (int k = 1; k < 512; k++) m.setKey(k, false);
        m.setTouch(false, 0, 0);
        input_->changed();
    }
    QMainWindow::changeEvent(e);
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    if (rec_->recording()) { recTimer_->stop(); rec_->stop(); }
    Settings::get().save();
    QMainWindow::closeEvent(e);
}
