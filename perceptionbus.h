#ifndef PERCEPTIONBUS_H
#define PERCEPTIONBUS_H

#include <QObject>
#include <QString>

class QTimer;

// =============================================================
// PerceptionBus — v4 environment-awareness scaffold.
//
// Polls lightweight facts about the user's environment (currently the
// foreground window title) and publishes them as signals. The host
// (Widget) moves the bus onto a dedicated worker thread, so polling
// never touches the GUI thread; consumers receive updates through
// auto-queued signal connections. Consumers today: AI chat context
// injection (Widget). Future providers can be added to poll() without
// touching subscribers.
//
// Thread contract: start()/stop() must be invoked in the bus's own
// thread (use QMetaObject::invokeMethod / queued calls from the host).
// foregroundTitle() is bus-thread-only; cross-thread consumers must
// cache the value delivered by foregroundWindowChanged().
// =============================================================
class PerceptionBus : public QObject {
    Q_OBJECT
public:
    explicit PerceptionBus(QObject *parent = nullptr);

    const QString& foregroundTitle() const { return m_foregroundTitle; }

public slots:
    void start(int pollIntervalMs);
    void stop();

signals:
    void foregroundWindowChanged(const QString &title);

private:
    void poll();

    QTimer *m_timer;
    QString m_foregroundTitle;
};

#endif // PERCEPTIONBUS_H
