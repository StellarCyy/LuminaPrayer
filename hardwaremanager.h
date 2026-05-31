#ifndef HARDWAREMANAGER_H
#define HARDWAREMANAGER_H

#include <QObject>
#include <QTimer>

class HardwareManager : public QObject
{
    Q_OBJECT
public:
    explicit HardwareManager(int pollIntervalMs = 100, QObject *parent = nullptr);

    float cpuUsage() const { return m_cpuUsage; }
    void setPollInterval(int ms);

signals:
    void cpuUsageUpdated(float usage);

private slots:
    void poll();

private:
    QTimer *m_timer;
    float   m_cpuUsage = 0.0f;

#ifdef Q_OS_WIN
    quint64 m_prevIdle   = 0;
    quint64 m_prevKernel = 0;
    quint64 m_prevUser   = 0;
    bool    m_firstPoll  = true;
#endif
};

#endif // HARDWAREMANAGER_H
