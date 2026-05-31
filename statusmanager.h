#ifndef STATUSMANAGER_H
#define STATUSMANAGER_H

#include "roleact.h"
#include <QObject>
#include <QMap>
#include <QTimer>
#include <QElapsedTimer>

class StatusManager : public QObject
{
    Q_OBJECT
public:
    enum StatusKind {
        Happiness = 0,
        Interest,
        Sanity,
        Satiety,
        Affection,
        StatusCount
    };
    Q_ENUM(StatusKind)

    explicit StatusManager(QObject *parent = nullptr);

    // --- Existing API (unchanged) ---
    int  value(StatusKind kind) const;
    void setValue(StatusKind kind, int v);
    void addValue(StatusKind kind, int delta);

    static constexpr int MinValue = 0;
    static constexpr int MaxValue = 100;

    // --- Attribute system control ---
    void setStatsVariable(bool enabled);
    void setFrozen(bool frozen);
    void setCurrentAct(RoleAct act);
    void notifyAngry();
    void notifyMouseInteraction();
    void setTickIntervalMs(int ms);

    // Negative overflow: when attribute at 0, remainder deducted from Happiness
    void addWithOverflow(StatusKind kind, int delta);

    static constexpr int AutoFeedThreshold = 10;

signals:
    void statusChanged(StatusKind kind, int newValue);
    void satietyCritical();  // emitted when satiety drops below AutoFeedThreshold (no auto-action)

private slots:
    void onTick();

private:

    int m_values[StatusCount];

    // Tick system
    QTimer *m_tickTimer;
    bool    m_statsVariable  = true;
    bool    m_frozen         = false;
    RoleAct m_currentAct     = RoleAct::Stand;

    // Interest idle tracking
    QElapsedTimer m_lastInteractionTime;
    int m_tickIntervalMs = 10000;  // 10s
    static constexpr int InterestIdleThreshMs = 30000; // 30s
};

#endif // STATUSMANAGER_H
