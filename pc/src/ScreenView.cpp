// ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
#include "ScreenView.h"
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>

static QRectF toQ(const sc::Rect &r) { return QRectF(r.x, r.y, r.w, r.h); }

ScreenView::ScreenView(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(320, 240);
    placeholder_ = tr("Aucune console connectée");
}

void ScreenView::setLayoutMode(sc::LayoutMode m) { mode_ = m; if (m != sc::LayoutMode::Custom) setEditMode(false); update(); }

void ScreenView::setEditMode(bool on)
{
    edit_ = on && mode_ == sc::LayoutMode::Custom;
    drag_ = nullptr;
    setCursor(edit_ ? Qt::SizeAllCursor : Qt::ArrowCursor);
    update();
}

void ScreenView::setTouchEnabled(bool on)
{
    touchEnabled_ = on;
    if (!on && touching_) { touching_ = false; emit touchChanged(false, 0, 0); }
}

void ScreenView::setFrame(int screen, const QImage &img)
{
    if (screen < 0 || screen > 1) return;
    img_[screen] = img;
    update();
}

void ScreenView::clearFrames() { img_[0] = img_[1] = QImage(); update(); }

void ScreenView::drawScreens(QPainter &p, const sc::LayoutResult &r, bool smooth) const
{
    p.setRenderHint(QPainter::SmoothPixmapTransform, smooth);
    const sc::Rect *rects[2] = {&r.top, &r.bottom};
    for (int s = 0; s < 2; s++) {
        if (rects[s]->empty()) continue;
        if (img_[s].isNull()) p.fillRect(toQ(*rects[s]), QColor(0x1b, 0x1f, 0x2a));
        else p.drawImage(toQ(*rects[s]), img_[s]);
    }
}

void ScreenView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x0b, 0x0d, 0x12));
    sc::LayoutResult r = sc::computeLayout(mode_, width(), height(), custom_, integer_);
    // Integer scaling is pixel-perfect: smoothing only when asked for
    drawScreens(p, r, smooth_);

    if (!hasFrames() && !placeholder_.isEmpty()) {
        p.setPen(QColor(0x8a, 0x93, 0xa6));
        QFont f = font(); f.setPointSizeF(f.pointSizeF() * 1.2); p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap, placeholder_);
    }
    if (edit_) {
        p.setRenderHint(QPainter::Antialiasing);
        const sc::Rect *rs[2] = {&r.top, &r.bottom};
        const char *names[2] = {"Écran supérieur", "Écran inférieur"};
        for (int s = 0; s < 2; s++) {
            QRectF q = toQ(*rs[s]);
            p.setPen(QPen(QColor(0x3d, 0xdc, 0x97), 2, Qt::DashLine));
            p.setBrush(QColor(0x3d, 0xdc, 0x97, 30));
            p.drawRect(q);
            p.setBrush(QColor(0x3d, 0xdc, 0x97));
            p.drawRect(QRectF(q.right() - 12, q.bottom() - 12, 12, 12));
            p.setPen(Qt::white);
            p.drawText(q.adjusted(8, 6, 0, 0), Qt::AlignLeft | Qt::AlignTop, tr(names[s]));
        }
        p.setPen(QColor(0xe8, 0xec, 0xf2));
        p.drawText(rect().adjusted(0, 0, 0, -10), Qt::AlignHCenter | Qt::AlignBottom,
                   tr("Éditeur : glissez pour déplacer, coin vert pour redimensionner — Ctrl+E pour terminer"));
    }
}

QImage ScreenView::renderComposite(int w, int h) const
{
    QImage out(w, h, QImage::Format_ARGB32);
    out.fill(QColor(0, 0, 0));
    QPainter p(&out);
    sc::LayoutResult r = sc::computeLayout(mode_, w, h, custom_, false);
    drawScreens(p, r, smooth_);
    return out;
}

// ---------------- Mouse: touch screen ----------------
sc::Rect *ScreenView::editTarget(const QPointF &pos, bool &corner)
{
    sc::LayoutResult r = sc::computeLayout(mode_, width(), height(), custom_, false);
    // Bottom screen first: it is usually drawn on top / smaller
    sc::Rect *norm[2] = {&custom_.bottom, &custom_.top};
    const sc::Rect *pix[2] = {&r.bottom, &r.top};
    for (int i = 0; i < 2; i++) {
        QRectF q = toQ(*pix[i]);
        if (QRectF(q.right() - 16, q.bottom() - 16, 20, 20).contains(pos)) { corner = true; return norm[i]; }
        if (q.contains(pos)) { corner = false; return norm[i]; }
    }
    return nullptr;
}

void ScreenView::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;
    if (edit_) {
        drag_ = editTarget(e->position(), dragCorner_);
        if (drag_) { dragStart_ = e->position(); dragOrig_ = *drag_; }
        return;
    }
    if (!touchEnabled_) return;
    sc::LayoutResult r = sc::computeLayout(mode_, width(), height(), custom_, integer_);
    int x, y;
    if (sc::windowToTouch(r.bottom, e->position().x(), e->position().y(), x, y)) {
        touching_ = true;
        emit touchChanged(true, x, y);
    }
}

void ScreenView::mouseMoveEvent(QMouseEvent *e)
{
    if (edit_) {
        if (!drag_) {
            bool c; sc::Rect *t = editTarget(e->position(), c);
            setCursor(!t ? Qt::ArrowCursor : c ? Qt::SizeFDiagCursor : Qt::SizeAllCursor);
            return;
        }
        double dx = (e->position().x() - dragStart_.x()) / width(), dy = (e->position().y() - dragStart_.y()) / height();
        if (dragCorner_) {
            // Keep the 3DS aspect ratio while resizing
            bool isTop = drag_ == &custom_.top;
            double aspect = (isTop ? 400.0 : 320.0) / 240.0 * height() / double(width());
            double nw = std::max(0.05, dragOrig_.w + dx);
            drag_->w = nw; drag_->h = nw / aspect;
        } else {
            drag_->x = std::clamp(dragOrig_.x + dx, -0.5, 1.0); drag_->y = std::clamp(dragOrig_.y + dy, -0.5, 1.0);
        }
        update();
        return;
    }
    if (touching_) {
        sc::LayoutResult r = sc::computeLayout(mode_, width(), height(), custom_, integer_);
        int x, y;
        sc::windowToTouchClamped(r.bottom, e->position().x(), e->position().y(), x, y);
        emit touchChanged(true, x, y);
    }
}

void ScreenView::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;
    if (edit_) { if (drag_) emit customLayoutEdited(custom_); drag_ = nullptr; return; }
    if (touching_) { touching_ = false; emit touchChanged(false, 0, 0); }
}

void ScreenView::leaveEvent(QEvent *)
{
    // Mouse grab keeps delivering moves while the button is held, so a drag
    // that leaves the window keeps touching (clamped). Nothing to do here.
}
