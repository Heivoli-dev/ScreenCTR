// ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
// Draws both screens according to the layout, turns mouse clicks on the bottom
// screen into touch input, and hosts the free layout editor.
#pragma once
#include "Layout.h"
#include <QImage>
#include <QWidget>

class ScreenView : public QWidget {
    Q_OBJECT
public:
    explicit ScreenView(QWidget *parent = nullptr);

    void setLayoutMode(sc::LayoutMode m);
    sc::LayoutMode layoutMode() const { return mode_; }
    void setCustomLayout(const sc::CustomLayout &c) { custom_ = c; update(); }
    const sc::CustomLayout &customLayout() const { return custom_; }
    void setIntegerScale(bool on) { integer_ = on; update(); }
    void setSmooth(bool on) { smooth_ = on; update(); }
    void setEditMode(bool on);
    bool editMode() const { return edit_; }
    void setPlaceholder(const QString &text) { placeholder_ = text; update(); }
    void setTouchEnabled(bool on);

    // Render the current layout at an exact size (screenshots / recording)
    QImage renderComposite(int width, int height) const;
    bool hasFrames() const { return !img_[0].isNull() || !img_[1].isNull(); }

public slots:
    void setFrame(int screen, const QImage &img);
    void clearFrames();

signals:
    void touchChanged(bool down, int x, int y);
    void customLayoutEdited(const sc::CustomLayout &c);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    void drawScreens(QPainter &p, const sc::LayoutResult &r, bool smooth) const;
    sc::Rect *editTarget(const QPointF &pos, bool &corner);

    sc::LayoutMode mode_ = sc::LayoutMode::Vertical;
    sc::CustomLayout custom_;
    bool integer_ = false, smooth_ = false, edit_ = false, touchEnabled_ = true;
    QImage img_[2];
    QString placeholder_;
    bool touching_ = false;
    // editor
    sc::Rect *drag_ = nullptr; bool dragCorner_ = false; QPointF dragStart_; sc::Rect dragOrig_;
};
