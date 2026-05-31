#ifndef STARWIDGET_H
#define STARWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QPointF>

class StarWidget : public QWidget {
    Q_OBJECT
public:
    explicit StarWidget(const QPixmap &pixmap, const QPointF &velocity,
                        const QPoint &startPos, QWidget *parent = nullptr);

    QPointF velocity() const { return m_velocity; }
    void setVelocity(const QPointF &v) { m_velocity = v; }
    bool isDragging() const { return m_dragging; }

    QPointF posF() const { return m_posF; }
    void setPosF(const QPointF &p);
    QPointF centerF() const;
    double radius() const;

    void advance(double dt);

    static constexpr int kDisplaySize = 64;

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void contextMenuEvent(QContextMenuEvent *) override;

private:
    QPixmap m_pixmap;
    QPointF m_posF;
    QPointF m_velocity;
    bool m_dragging = false;
    QPoint m_dragOffset;
};

#endif // STARWIDGET_H
