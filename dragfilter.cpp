#include "dragfilter.h"
#include "widget.h"
#include "profilemanager.h"
#include <QMouseEvent>
#include <cmath>

DragFilter::DragFilter(QObject *parent)
    : QObject(parent), m_leftPressActive(false) {}

bool DragFilter::eventFilter(QObject *obj, QEvent *event) {
    auto w = dynamic_cast<Widget*>(obj);
    if (!w) return false;

    // H-05: resetIdleTimer on any mouse activity
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseMove) {
        w->resetIdleTimer();
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto *e = dynamic_cast<QMouseEvent*>(event);
        if (e && e->button() == Qt::LeftButton) {
            // H-05: addClickCount only on explicit left-button press (not move)
            w->addClickCount();
            m_leftPressActive = true;
            m_pullDragStarted = false;
            m_leftPressGlobalPos = e->globalPosition().toPoint();
            w->onPrimaryLeftClick();
        } else {
            m_leftPressActive = false;
        }
        w->stopWalking();
        if (e) m_dragOffset = e->pos();

    } else if (event->type() == QEvent::MouseButtonRelease) {
        auto *e = dynamic_cast<QMouseEvent*>(event);
        if (e && e->button() == Qt::LeftButton) {
            // Exit pull-drag if active
            if (m_pullDragStarted) {
                w->exitPullDrag();
                // After a drag, also do gomoku self-heal if applicable
                if (w->isInGomokuMode() && !w->isGomokuSuspended()) {
                    w->gomokuDragSelfHeal();
                }
            } else if (m_leftPressActive) {
                // Was a click (never exceeded threshold) — trigger shake
                w->triggerStandClickShake();
            }
            m_leftPressActive = false;
            m_pullDragStarted = false;
        }

    } else if (event->type() == QEvent::ActivationChange) {
        // Focus-loss safety: reset internal drag state so next press starts clean.
        // Widget::changeEvent handles the sprite/visual reset separately.
        if (!w->isActiveWindow()) {
            m_leftPressActive = false;
            m_pullDragStarted = false;
        }

    } else if (event->type() == QEvent::MouseMove) {
        auto *e = dynamic_cast<QMouseEvent*>(event);
        if (e && (e->buttons() & Qt::LeftButton) && m_leftPressActive) {
            const QPoint globalPos = e->globalPosition().toPoint();

            if (!m_pullDragStarted) {
                // Check if mouse moved enough to transition from click to drag
                const QPoint delta = globalPos - m_leftPressGlobalPos;
                const double distance = std::sqrt(static_cast<double>(
                    delta.x() * delta.x() + delta.y() * delta.y()));
                if (distance >= ProfileManager::instance()->behavior().drag_click_threshold_px) {
                    m_pullDragStarted = true;
                    w->enterPullDrag(globalPos);
                }
            } else {
                // Already in pull-drag: update position with hand anchor
                w->updatePullDrag(globalPos);
            }
        }
    }

    return QObject::eventFilter(obj, event);
}
