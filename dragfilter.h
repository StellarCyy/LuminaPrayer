#ifndef DRAGFILTER_H
#define DRAGFILTER_H

#include <QObject>
#include <QPoint>
#include <QEvent>

class DragFilter : public QObject {
public:
    explicit DragFilter(QObject *parent = nullptr);
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QPoint m_dragOffset;
    QPoint m_leftPressGlobalPos;
    bool m_leftPressActive;
    bool m_pullDragStarted = false;
};

#endif // DRAGFILTER_H
