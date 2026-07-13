#include "actionstatemachine.h"
#include <QTimer>
#include <QRandomGenerator>
#include <QDebug>

ActionStateMachine::ActionStateMachine(QObject *parent)
    : QObject(parent) {}

void ActionStateMachine::setTopology(const ActionTopologyProfile *topology) {
    m_topology = topology;
}

void ActionStateMachine::setDurationResolver(IntResolver resolver) {
    m_resolver = std::move(resolver);
}

void ActionStateMachine::registerBehavior(const QString &name, Behavior fn) {
    m_behaviors.insert(name, std::move(fn));
}

const ActionStateSpec* ActionStateMachine::spec(const QString &id) const {
    if (!m_topology) return nullptr;
    auto it = m_topology->states.constFind(id);
    return (it != m_topology->states.constEnd()) ? &it.value() : nullptr;
}

// =============================================================
// State entry
// =============================================================

void ActionStateMachine::enterState(const QString &id) {
    disarmAll();
    m_current = id;

    const ActionStateSpec *s = spec(id);
    if (!s) return;   // unknown state: no rules, host handles presentation

    // on_enter behaviors (may themselves re-enter another state; if so,
    // m_current changed and we must not arm the stale state's rules)
    runBehaviorList(s->on_enter);
    if (m_current != id) return;

    const auto rules = s->transitions;   // copy: profile may reload mid-loop
    for (const ActionTransitionRule &rule : rules)
        armRule(rule);
}

void ActionStateMachine::stopAll() {
    disarmAll();
}

void ActionStateMachine::disarmAll() {
    for (Armed &armed : m_armed) {
        if (armed.timer) {
            armed.timer->stop();
            armed.timer->deleteLater();   // safe even inside its own timeout
            armed.timer = nullptr;
        }
    }
    m_armed.clear();
}

// =============================================================
// Rule arming / firing
// =============================================================

void ActionStateMachine::armRule(const ActionTransitionRule &rule) {
    const int initialMs  = resolveMs(rule.after_ms, rule.after_ms_key);
    const int periodicMs = resolveMs(rule.periodic_ms, rule.periodic_ms_key);
    if (initialMs < 0 && periodicMs < 0) {
        qWarning() << "[ActionStateMachine] rule" << rule.trigger
                   << "has no resolvable delay — skipped";
        return;
    }

    Armed armed;
    armed.rule  = rule;
    armed.timer = new QTimer(this);
    QTimer *timer = armed.timer;
    connect(timer, &QTimer::timeout, this, [this, timer]() { onTimerFired(timer); });
    m_armed.append(armed);
    startPhase1(m_armed.last());
}

void ActionStateMachine::startPhase1(Armed &armed) {
    const int initialMs  = resolveMs(armed.rule.after_ms, armed.rule.after_ms_key);
    const int periodicMs = resolveMs(armed.rule.periodic_ms, armed.rule.periodic_ms_key);

    if (initialMs >= 0) {
        armed.periodicPhase = false;
        armed.timer->setSingleShot(true);
        armed.timer->start(initialMs);
    } else if (periodicMs > 0) {
        armed.periodicPhase = true;
        armed.timer->setSingleShot(false);
        armed.timer->start(periodicMs);
    }
}

void ActionStateMachine::onTimerFired(QTimer *timer) {
    int idx = -1;
    for (int i = 0; i < m_armed.size(); ++i) {
        if (m_armed[i].timer == timer) { idx = i; break; }
    }
    if (idx < 0) return;   // stale timer (state already changed)

    // Copy everything needed BEFORE running behaviors: they may re-enter
    // enterState() and invalidate m_armed / this timer.
    const ActionTransitionRule rule = m_armed[idx].rule;

    // Phase switch: single-shot delay elapsed -> begin periodic re-checks
    if (!m_armed[idx].periodicPhase) {
        const int periodicMs = resolveMs(rule.periodic_ms, rule.periodic_ms_key);
        if (periodicMs > 0) {
            m_armed[idx].periodicPhase = true;
            timer->setSingleShot(false);
            timer->start(periodicMs);
        }
    }

    // Chance roll (per firing)
    const int chance = resolveChance(rule);
    if (chance < 100 && QRandomGenerator::global()->bounded(100) >= chance)
        return;

    // Pre-transition behaviors; any false vetoes the transition
    const bool allowed = runBehaviorList(rule.behaviors);

    if (allowed && !rule.to.isEmpty())
        emit transitionRequested(rule.to, rule.post_behaviors);
}

// =============================================================
// Interaction rearm
// =============================================================

void ActionStateMachine::rearm() {
    for (Armed &armed : m_armed) {
        if (armed.rule.rearm_on_interaction && armed.timer)
            startPhase1(armed);
    }
}

void ActionStateMachine::refreshAll() {
    for (Armed &armed : m_armed) {
        if (armed.timer)
            startPhase1(armed);
    }
}

// =============================================================
// Behaviors / resolvers
// =============================================================

bool ActionStateMachine::runBehaviorList(const QStringList &names) {
    bool ok = true;
    for (const QString &name : names) {
        auto it = m_behaviors.constFind(name);
        if (it == m_behaviors.constEnd()) {
            qWarning() << "[ActionStateMachine] unknown behavior:" << name;
            continue;
        }
        if (!(it.value())())
            ok = false;
    }
    return ok;
}

int ActionStateMachine::resolveMs(int literal, const QString &key) const {
    if (!key.isEmpty() && m_resolver) {
        const int v = m_resolver(key);
        if (v >= 0) return v;
        qWarning() << "[ActionStateMachine] unresolvable duration key:" << key;
    }
    return literal;
}

int ActionStateMachine::resolveChance(const ActionTransitionRule &rule) const {
    if (!rule.chance_percent_key.isEmpty() && m_resolver) {
        const int v = m_resolver(rule.chance_percent_key);
        if (v >= 0) return qBound(0, v, 100);
    }
    return qBound(0, rule.chance_percent, 100);
}
