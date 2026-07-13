#ifndef ACTIONSTATEMACHINE_H
#define ACTIONSTATEMACHINE_H

#include "profilemanager.h"
#include <QObject>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <functional>

class QTimer;

// =============================================================
// ActionStateMachine — data-driven runtime executor for
// ActionTopologyProfile (character.json "state_transitions").
//
// Responsibilities:
//   - Owns one QTimer per transition rule of the ACTIVE state only.
//   - Two-phase rules: optional initial delay (after_ms[_key]) followed by
//     an optional periodic re-check (periodic_ms[_key]) with a chance roll.
//   - Behaviors are named callbacks registered by the host (Widget).
//     Rule behaviors run BEFORE the transition; any returning false vetoes
//     the transition for that firing (periodic rules keep re-checking).
//   - Emits transitionRequested(to, postBehaviors); the host applies the
//     state change and then runs postBehaviors via runBehaviorList().
//   - rearm() restarts phase 1 of rules flagged rearm_on_interaction
//     (mirrors the legacy resetIdleTimer semantics).
//
// Re-entrancy contract: behaviors may call enterState() (directly or via
// the host). Armed rules are copied by value before execution, and timers
// are torn down with deleteLater(), so firing rules survive state changes
// triggered from inside their own behavior chain.
// =============================================================
class ActionStateMachine : public QObject {
    Q_OBJECT
public:
    using Behavior    = std::function<bool()>;
    using IntResolver = std::function<int(const QString &key)>;   // -1 = unknown key

    explicit ActionStateMachine(QObject *parent = nullptr);

    void setTopology(const ActionTopologyProfile *topology);
    void setDurationResolver(IntResolver resolver);
    void registerBehavior(const QString &name, Behavior fn);

    // Disarms previous rules, runs the new state's on_enter behaviors,
    // then arms its transition rules.
    void enterState(const QString &id);

    // Disarm all rule timers without changing the current state id
    // (used for transient phases like the sit fly-in).
    void stopAll();

    // Restart phase 1 of all rearm_on_interaction rules of the active state.
    void rearm();

    // Restart phase 1 of ALL armed rules of the active state (used when
    // duration config values change at runtime, e.g. settings dialog).
    void refreshAll();

    // Run a behavior list (used by the host for post-transition behaviors).
    // Returns false if any behavior returned false.
    bool runBehaviorList(const QStringList &names);

    const QString& currentState() const { return m_current; }
    const ActionStateSpec* spec(const QString &id) const;

signals:
    void transitionRequested(const QString &to, const QStringList &postBehaviors);

private:
    struct Armed {
        ActionTransitionRule rule;       // by value: survives profile reloads
        QTimer *timer = nullptr;
        bool periodicPhase = false;
    };

    void armRule(const ActionTransitionRule &rule);
    void startPhase1(Armed &armed);
    void onTimerFired(QTimer *timer);
    void disarmAll();
    int  resolveMs(int literal, const QString &key) const;
    int  resolveChance(const ActionTransitionRule &rule) const;

    const ActionTopologyProfile *m_topology = nullptr;
    IntResolver m_resolver;
    QHash<QString, Behavior> m_behaviors;
    QString m_current;
    QList<Armed> m_armed;
};

#endif // ACTIONSTATEMACHINE_H
