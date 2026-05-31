#include "starwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>

StarWidget::StarWidget(const QPixmap &pixmap, const QPointF &velocity,
                       const QPoint &startPos, QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
    , m_pixmap(pixmap.scaled(kDisplaySize, kDisplaySize,
                             Qt::KeepAspectRatio, Qt::SmoothTransformation))
    , m_posF(startPos)
    , m_velocity(velocity)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(kDisplaySize, kDisplaySize);
    move(startPos);
    show();
}

QPointF StarWidget::centerF() const {
    return m_posF + QPointF(width() / 2.0, height() / 2.0);
}

double StarWidget::radius() const {
    return kDisplaySize / 2.0;
}

void StarWidget::setPosF(const QPointF &p) {
    m_posF = p;
    QWidget::move(p.toPoint());
}

void StarWidget::advance(double dt) {
    if (m_dragging) return;
    m_posF += m_velocity * dt;
    QWidget::move(m_posF.toPoint());
}

void StarWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawPixmap(0, 0, m_pixmap);
}

void StarWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->pos();
    }
}

void StarWidget::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging) {
        QPoint newPos = event->globalPosition().toPoint() - m_dragOffset;
        m_posF = QPointF(newPos);
        QWidget::move(newPos);
    }
}

void StarWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
    }
}

void StarWidget::contextMenuEvent(QContextMenuEvent *event) {
    event->ignore();
}
