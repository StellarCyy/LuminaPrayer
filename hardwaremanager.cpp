#include "hardwaremanager.h"

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif

HardwareManager::HardwareManager(int pollIntervalMs, QObject *parent)
    : QObject(parent),
      m_timer(new QTimer(this))
{
    connect(m_timer, &QTimer::timeout, this, &HardwareManager::poll);
    poll();                        // seed previous values
    m_timer->start(pollIntervalMs);
}

void HardwareManager::setPollInterval(int ms) {
    m_timer->setInterval(ms);
}

#ifdef Q_OS_WIN
static quint64 fileTimeToU64(const FILETIME &ft) {
    return (static_cast<quint64>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}
#endif

void HardwareManager::poll() {
#ifdef Q_OS_WIN
    FILETIME idle, kernel, user;
    if (!GetSystemTimes(&idle, &kernel, &user))
        return;

    quint64 curIdle   = fileTimeToU64(idle);
    quint64 curKernel = fileTimeToU64(kernel);
    quint64 curUser   = fileTimeToU64(user);

    if (m_firstPoll) {
        m_prevIdle   = curIdle;
        m_prevKernel = curKernel;
        m_prevUser   = curUser;
        m_firstPoll  = false;
        return;
    }

    quint64 idleDiff   = curIdle   - m_prevIdle;
    quint64 kernelDiff = curKernel - m_prevKernel;
    quint64 userDiff   = curUser   - m_prevUser;
    quint64 totalDiff  = kernelDiff + userDiff; // kernel includes idle on Windows

    m_prevIdle   = curIdle;
    m_prevKernel = curKernel;
    m_prevUser   = curUser;

    if (totalDiff == 0) return;

    m_cpuUsage = 1.0f - static_cast<float>(idleDiff) / static_cast<float>(totalDiff);
    m_cpuUsage = qBound(0.0f, m_cpuUsage, 1.0f);
    emit cpuUsageUpdated(m_cpuUsage);
#else
    m_cpuUsage = 0.0f;
    emit cpuUsageUpdated(m_cpuUsage);
#endif
}
