#include "statusmanager.h"
#include <algorithm>

StatusManager::StatusManager(QObject *parent)
    : QObject(parent),
      m_tickTimer(new QTimer(this))
{
    // Initial values: Affection=100, all others=80
    m_values[Happiness] = 80;
    m_values[Interest]  = 80;
    m_values[Sanity]    = 80;
    m_values[Satiety]   = 80;
    m_values[Affection] = 100;

    // Start idle tracker
    m_lastInteractionTime.start();

    // 10s tick timer
    connect(m_tickTimer, &QTimer::timeout, this, &StatusManager::onTick);
    m_tickTimer->start(m_tickIntervalMs);
}

// ==========================================
// Existing API (preserved)
// ==========================================

int StatusManager::value(StatusKind kind) const {
    if (kind < 0 || kind >= StatusCount) return 0;
    return m_values[kind];
}

void StatusManager::setValue(StatusKind kind, int v) {
    if (kind < 0 || kind >= StatusCount) return;
    const int clamped = std::clamp(v, MinValue, MaxValue);
    if (m_values[kind] == clamped) return;
    m_values[kind] = clamped;
    emit statusChanged(kind, clamped);
}

void StatusManager::addValue(StatusKind kind, int delta) {
    setValue(kind, value(kind) + delta);
}

// ==========================================
// Attribute system control
// ==========================================

void StatusManager::setStatsVariable(bool enabled) {
    m_statsVariable = enabled;
}

void StatusManager::setFrozen(bool frozen) {
    m_frozen = frozen;
}

void StatusManager::setCurrentAct(RoleAct act) {
    m_currentAct = act;
}

void StatusManager::notifyAngry() {
    if (!m_statsVariable || m_frozen) return;
    // Immediate Sanity penalty on Angry trigger
    addWithOverflow(Sanity, -5);
}

void StatusManager::notifyMouseInteraction() {
    m_lastInteractionTime.restart();
}

void StatusManager::setTickIntervalMs(int ms) {
    ms = qMax(1000, ms);
    if (m_tickIntervalMs == ms) return;
    m_tickIntervalMs = ms;
    if (m_tickTimer->isActive()) m_tickTimer->start(m_tickIntervalMs);
}

// ==========================================
// 10-second tick
// ==========================================

void StatusManager::onTick() {
    if (!m_statsVariable || m_frozen) return;

    // Affection: +1
    addValue(Affection, +1);

    // Satiety: -1
    addWithOverflow(Satiety, -1);

    // Sanity: +1
    addValue(Sanity, +1);

    // Notify observers when satiety is critically low (no auto-action here)
    if (value(Satiety) < AutoFeedThreshold) {
        emit satietyCritical();
    }

    // Interest: -1 only if idle > 30s AND in Stand or Move
    if (m_currentAct == RoleAct::Stand || m_currentAct == RoleAct::Move) {
        if (m_lastInteractionTime.elapsed() >= InterestIdleThreshMs) {
            addWithOverflow(Interest, -1);
        }
    }
}

// ==========================================
// Negative-overflow redirect to Happiness
// ==========================================

void StatusManager::addWithOverflow(StatusKind kind, int delta) {
    if (delta >= 0) {
        addValue(kind, delta);
        return;
    }
    // Negative delta: if the attribute is already at 0, redirect to Happiness
    const int cur = value(kind);
    if (cur <= 0) {
        // Entire penalty overflows to Happiness
        addValue(Happiness, delta);
    } else if (cur + delta < 0) {
        // Partial: bring attribute to 0, overflow remainder to Happiness
        const int overflow = cur + delta;  // negative remainder
        setValue(kind, 0);
        addValue(Happiness, overflow);
    } else {
        addValue(kind, delta);
    }
}
