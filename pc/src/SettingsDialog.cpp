// ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
#include "SettingsDialog.h"
#include "GamepadManager.h"
#include "KeyMap.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

using sc::Target;

static QString sourceLabel(const QString &s)
{
    if (s.startsWith("key:")) return QObject::tr("Clavier %1").arg(hidScancodeName(s.mid(4).toInt()));
    static const QMap<QString, QString> pad = {
        {"south", "Bas (A Xbox / ✕)"}, {"east", "Droite (B Xbox / ○)"}, {"west", "Gauche (X Xbox / □)"}, {"north", "Haut (Y Xbox / △)"},
        {"back", "Back / Share"}, {"guide", "Guide / PS"}, {"start", "Start / Options"}, {"lstick", "Clic stick G"},
        {"rstick", "Clic stick D"}, {"lshoulder", "LB / L1"}, {"rshoulder", "RB / R1"}, {"dpup", "Croix ↑"},
        {"dpdown", "Croix ↓"}, {"dpleft", "Croix ←"}, {"dpright", "Croix →"}, {"misc1", "Capture / Micro"}};
    static const QMap<QString, QString> ax = {
        {"leftx+", "Stick G →"}, {"leftx-", "Stick G ←"}, {"lefty+", "Stick G ↓"}, {"lefty-", "Stick G ↑"},
        {"rightx+", "Stick D →"}, {"rightx-", "Stick D ←"}, {"righty+", "Stick D ↓"}, {"righty-", "Stick D ↑"},
        {"lefttrigger+", "LT / L2"}, {"righttrigger+", "RT / R2"}};
    if (s.startsWith("pad:")) return QObject::tr("Manette %1").arg(pad.value(s.mid(4), s.mid(4)));
    if (s.startsWith("axis:")) return QObject::tr("Manette %1").arg(ax.value(s.mid(5), s.mid(5)));
    return s;
}

SettingsDialog::SettingsDialog(GamepadManager *pads, QWidget *parent) : QDialog(parent), pads_(pads), work_(Settings::get())
{
    setWindowTitle(tr("Paramètres — ScreenCTR"));
    resize(720, 560);
    auto *tabs = new QTabWidget(this);
    tabs->addTab(buildGeneral(), tr("Général"));
    tabs->addTab(buildVideo(), tr("Vidéo"));
    tabs->addTab(buildControls(), tr("Contrôles"));
    tabs->addTab(buildShortcuts(), tr("Raccourcis"));
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    auto *l = new QVBoxLayout(this);
    l->addWidget(tabs); l->addWidget(bb);
    if (pads_) connect(pads_, &GamepadManager::sourceActivated, this, &SettingsDialog::finishCapture);
}

static QWidget *dirRow(QLineEdit *e, QWidget *parent, bool file)
{
    auto *w = new QWidget(parent); auto *h = new QHBoxLayout(w); h->setContentsMargins(0, 0, 0, 0);
    auto *b = new QPushButton(QObject::tr("Parcourir…"), w);
    h->addWidget(e); h->addWidget(b);
    QObject::connect(b, &QPushButton::clicked, w, [e, w, file] {
        QString p = file ? QFileDialog::getOpenFileName(w, QObject::tr("ffmpeg.exe"), e->text(), "ffmpeg (ffmpeg.exe ffmpeg)")
                         : QFileDialog::getExistingDirectory(w, QString(), e->text());
        if (!p.isEmpty()) e->setText(p);
    });
    return w;
}

QWidget *SettingsDialog::buildGeneral()
{
    auto *w = new QWidget; auto *f = new QFormLayout(w);
    pcName_ = new QLineEdit(work_.pcName); pcName_->setMaxLength(31);
    autoReconnect_ = new QCheckBox(tr("Reconnexion automatique à la dernière console")); autoReconnect_->setChecked(work_.autoReconnect);
    integer_ = new QCheckBox(tr("Mise à l'échelle entière (pixels nets)")); integer_->setChecked(work_.integerScale);
    smooth_ = new QCheckBox(tr("Lissage de l'image")); smooth_->setChecked(work_.smooth);
    shotDir_ = new QLineEdit(work_.screenshotDir); vidDir_ = new QLineEdit(work_.videoDir);
    ffmpeg_ = new QLineEdit(work_.ffmpegPath); ffmpeg_->setPlaceholderText(tr("Automatique (ffmpeg.exe à côté de ScreenCTR.exe, puis PATH)"));
    f->addRow(tr("Nom de ce PC (affiché sur la console)"), pcName_);
    f->addRow(autoReconnect_); f->addRow(integer_); f->addRow(smooth_);
    f->addRow(tr("Dossier des captures"), dirRow(shotDir_, w, false));
    f->addRow(tr("Dossier des vidéos"), dirRow(vidDir_, w, false));
    f->addRow(tr("FFmpeg"), dirRow(ffmpeg_, w, true));
    auto *forget = new QPushButton(tr("Oublier les consoles mémorisées (%1)").arg(work_.consoles.size()));
    connect(forget, &QPushButton::clicked, this, [this, forget] { work_.consoles.clear(); work_.lastConsole = -1; forget->setText(tr("Consoles oubliées")); forget->setEnabled(false); });
    f->addRow(forget);
    return w;
}

QWidget *SettingsDialog::buildVideo()
{
    auto *w = new QWidget; auto *f = new QFormLayout(w);
    quality_ = new QSlider(Qt::Horizontal); quality_->setRange(10, 95); quality_->setValue(work_.quality);
    auto *ql = new QLabel(QString::number(work_.quality));
    connect(quality_, &QSlider::valueChanged, ql, [ql](int v) { ql->setNum(v); });
    auto *qrow = new QWidget; auto *qh = new QHBoxLayout(qrow); qh->setContentsMargins(0, 0, 0, 0); qh->addWidget(quality_); qh->addWidget(ql);
    fps_ = new QComboBox; fps_->addItem("20", 20); fps_->addItem("30", 30); fps_->addItem("40", 40); fps_->addItem("60", 60); fps_->addItem(tr("Illimité"), 0);
    fps_->setCurrentIndex(qMax(0, fps_->findData(work_.fpsCap)));
    chroma_ = new QComboBox; chroma_->addItem(tr("4:2:0 — léger (recommandé)"), 0); chroma_->addItem(tr("4:4:4 — couleurs nettes, +40 % de débit"), 1);
    chroma_->setCurrentIndex(work_.chroma);
    top_ = new QCheckBox(tr("Écran supérieur")); top_->setChecked(work_.screens & 1);
    bottom_ = new QCheckBox(tr("Écran inférieur")); bottom_->setChecked(work_.screens & 2);
    f->addRow(tr("Qualité JPEG"), qrow);
    f->addRow(tr("Images par seconde max."), fps_);
    f->addRow(tr("Sous-échantillonnage"), chroma_);
    f->addRow(tr("Écrans diffusés"), top_); f->addRow(QString(), bottom_);
    f->addRow(new QLabel(tr("Ne diffuser qu'un écran libère de la puissance sur la console et du débit Wi-Fi.")));
    return w;
}

QWidget *SettingsDialog::buildShortcuts()
{
    auto *w = new QWidget; auto *f = new QFormLayout(w);
    for (auto it = work_.shortcuts.cbegin(); it != work_.shortcuts.cend(); ++it) {
        auto *e = new QKeySequenceEdit(it.value());
        e->setMaximumSequenceLength(1);
        keyEdits_[it.key()] = e;
        f->addRow(Settings::shortcutLabel(it.key()), e);
    }
    auto *reset = new QPushButton(tr("Rétablir les raccourcis par défaut"));
    connect(reset, &QPushButton::clicked, this, [this] {
        auto d = Settings::defaultShortcuts();
        for (auto it = keyEdits_.begin(); it != keyEdits_.end(); ++it) it.value()->setKeySequence(d.value(it.key()));
    });
    f->addRow(reset);
    return w;
}

QWidget *SettingsDialog::buildControls()
{
    auto *w = new QWidget; auto *v = new QVBoxLayout(w);
    remote_ = new QCheckBox(tr("Activer le contrôle de la console depuis ce PC")); remote_->setChecked(work_.remoteInput);
    devices_ = new QLabel;
    auto refreshDevices = [this] {
        QStringList d = pads_ ? pads_->deviceNames() : QStringList();
        devices_->setText(tr("Manettes détectées : %1").arg(d.isEmpty() ? tr("aucune") : d.join(", ")));
    };
    refreshDevices();
    if (pads_) connect(pads_, &GamepadManager::devicesChanged, this, refreshDevices);

    auto *prow = new QHBoxLayout;
    profile_ = new QComboBox;
    for (auto &p : work_.profiles) profile_->addItem(QString::fromStdString(p.name));
    auto *dup = new QPushButton(tr("Dupliquer")), *ren = new QPushButton(tr("Renommer")), *del = new QPushButton(tr("Supprimer"));
    prow->addWidget(new QLabel(tr("Profil"))); prow->addWidget(profile_, 1); prow->addWidget(dup); prow->addWidget(ren); prow->addWidget(del);

    bindings_ = new QTableWidget(int(Target::Count), 3);
    bindings_->setHorizontalHeaderLabels({tr("Commande 3DS"), tr("Assignations"), QString()});
    bindings_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    bindings_->verticalHeader()->hide();
    bindings_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    bindings_->setSelectionMode(QAbstractItemView::NoSelection);

    auto mkSlider = [](int value) { auto *s = new QSlider(Qt::Horizontal); s->setRange(0, 90); s->setValue(value); return s; };
    dzL_ = mkSlider(15); dzR_ = mkSlider(15); trig_ = mkSlider(35); trig_->setRange(5, 95);
    auto *f = new QFormLayout;
    f->addRow(tr("Zone morte stick gauche (Circle Pad) %"), dzL_);
    f->addRow(tr("Zone morte stick droit (C-Stick) %"), dzR_);
    f->addRow(tr("Seuil des gâchettes ZL/ZR %"), trig_);
    captureHint_ = new QLabel; captureHint_->setStyleSheet("color:#3DDC97");

    v->addWidget(remote_);
    v->addWidget(new QLabel(tr("Le clavier agit quand la fenêtre ScreenCTR est active ; la manette fonctionne aussi en arrière-plan (OBS au premier plan, par exemple).")));
    v->addWidget(devices_); v->addLayout(prow);
    v->addWidget(bindings_, 1); v->addLayout(f); v->addWidget(captureHint_);

    connect(profile_, &QComboBox::currentIndexChanged, this, [this](int i) { storeProfile(); loadProfile(i); });
    connect(dup, &QPushButton::clicked, this, [this] {
        storeProfile();
        sc::Profile p = work_.profiles[work_.activeProfile];
        p.name += " (copie)";
        work_.profiles << p;
        profile_->addItem(QString::fromStdString(p.name));
        profile_->setCurrentIndex(profile_->count() - 1);
    });
    connect(ren, &QPushButton::clicked, this, [this] {
        bool ok; QString n = QInputDialog::getText(this, tr("Renommer le profil"), tr("Nom"), QLineEdit::Normal, profile_->currentText(), &ok);
        if (ok && !n.trimmed().isEmpty()) { work_.profiles[work_.activeProfile].name = n.trimmed().toStdString(); profile_->setItemText(work_.activeProfile, n.trimmed()); }
    });
    connect(del, &QPushButton::clicked, this, [this] {
        if (work_.profiles.size() <= 1) return;
        int i = work_.activeProfile;
        work_.profiles.removeAt(i);
        work_.activeProfile = qMax(0, i - 1);
        QSignalBlocker b(profile_);
        profile_->removeItem(i);
        profile_->setCurrentIndex(work_.activeProfile);
        loadProfile(work_.activeProfile);
    });
    profile_->setCurrentIndex(work_.activeProfile);
    loadProfile(work_.activeProfile);
    return w;
}

void SettingsDialog::loadProfile(int i)
{
    if (i < 0 || i >= work_.profiles.size()) return;
    work_.activeProfile = i;
    const sc::Profile &p = work_.profiles[i];
    dzL_->setValue(int(p.deadzoneLeft * 100)); dzR_->setValue(int(p.deadzoneRight * 100)); trig_->setValue(int(p.triggerThreshold * 100));
    refreshBindings();
}

void SettingsDialog::storeProfile()
{
    int i = work_.activeProfile;
    if (i < 0 || i >= work_.profiles.size()) return;
    sc::Profile &p = work_.profiles[i];
    p.deadzoneLeft = dzL_->value() / 100.0; p.deadzoneRight = dzR_->value() / 100.0; p.triggerThreshold = trig_->value() / 100.0;
}

void SettingsDialog::refreshBindings()
{
    const sc::Profile &p = work_.profiles[work_.activeProfile];
    for (int t = 0; t < int(Target::Count); t++) {
        bindings_->setItem(t, 0, new QTableWidgetItem(QString::fromUtf8(sc::targetLabel(Target(t)))));
        QStringList labels;
        auto it = p.bindings.find(Target(t));
        if (it != p.bindings.end()) for (auto &s : it->second) labels << sourceLabel(QString::fromStdString(s));
        bindings_->setItem(t, 1, new QTableWidgetItem(labels.join("  ·  ")));
        auto *cell = new QWidget; auto *h = new QHBoxLayout(cell); h->setContentsMargins(2, 0, 2, 0);
        auto *add = new QPushButton(tr("Ajouter…")), *clr = new QPushButton(tr("Effacer"));
        h->addWidget(add); h->addWidget(clr);
        connect(add, &QPushButton::clicked, this, [this, t] { startCapture(t); });
        connect(clr, &QPushButton::clicked, this, [this, t] { work_.profiles[work_.activeProfile].bindings[Target(t)].clear(); refreshBindings(); });
        bindings_->setCellWidget(t, 2, cell);
    }
    bindings_->resizeColumnToContents(0);
    bindings_->resizeColumnToContents(2);
}

void SettingsDialog::startCapture(int row)
{
    captureRow_ = row;
    captureHint_->setText(tr("Appuyez sur une touche du clavier ou un bouton de la manette pour « %1 »… (5 s, Échap pour annuler)")
                              .arg(QString::fromUtf8(sc::targetLabel(Target(row)))));
    setFocus();
    QTimer::singleShot(5000, this, [this, row] { if (captureRow_ == row) { captureRow_ = -1; captureHint_->clear(); } });
}

void SettingsDialog::finishCapture(const QString &source)
{
    if (captureRow_ < 0) return;
    auto &v = work_.profiles[work_.activeProfile].bindings[Target(captureRow_)];
    std::string s = source.toStdString();
    if (std::find(v.begin(), v.end(), s) == v.end()) v.push_back(s);
    captureRow_ = -1;
    captureHint_->setText(tr("Assigné : %1").arg(sourceLabel(source)));
    refreshBindings();
}

void SettingsDialog::keyPressEvent(QKeyEvent *e)
{
    if (captureRow_ >= 0) {
        if (e->key() == Qt::Key_Escape) { captureRow_ = -1; captureHint_->clear(); return; }
        if (int h = hidScancodeFromEvent(e)) { finishCapture(QString("key:%1").arg(h)); return; }
    }
    QDialog::keyPressEvent(e);
}

void SettingsDialog::accept()
{
    storeProfile();
    work_.pcName = pcName_->text().trimmed().isEmpty() ? work_.pcName : pcName_->text().trimmed();
    work_.autoReconnect = autoReconnect_->isChecked();
    work_.integerScale = integer_->isChecked();
    work_.smooth = smooth_->isChecked();
    work_.screenshotDir = shotDir_->text(); work_.videoDir = vidDir_->text(); work_.ffmpegPath = ffmpeg_->text();
    work_.quality = quality_->value(); work_.fpsCap = fps_->currentData().toInt(); work_.chroma = chroma_->currentData().toInt();
    work_.screens = (top_->isChecked() ? 1 : 0) | (bottom_->isChecked() ? 2 : 0);
    if (!work_.screens) work_.screens = 3;
    work_.remoteInput = remote_->isChecked();
    for (auto it = keyEdits_.cbegin(); it != keyEdits_.cend(); ++it) work_.shortcuts[it.key()] = it.value()->keySequence();
    Settings::get() = work_;
    Settings::get().save();
    QDialog::accept();
}
