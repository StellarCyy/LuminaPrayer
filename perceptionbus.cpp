#include "perceptionbus.h"
#include "platformhal.h"
#include <QTimer>

PerceptionBus::PerceptionBus(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    connect(m_timer, &QTimer::timeout, this, &PerceptionBus::poll);
}

void PerceptionBus::start(int pollIntervalMs) {
    m_timer->start(qMax(250, pollIntervalMs));
    poll();   // immediate first sample
}

void PerceptionBus::stop() {
    m_timer->stop();
}

void PerceptionBus::poll() {
    const QString title = PlatformHAL::foregroundWindowTitle();
    // Empty = no meaningful foreground (desktop / own process): keep the
    // last known external title instead of flapping to empty.
    if (title.isEmpty() || title == m_foregroundTitle) return;
    m_foregroundTitle = title;
    emit foregroundWindowChanged(title);
}
