#ifndef ROLEACT_H
#define ROLEACT_H

#include <QObject>
#include <QMetaEnum>
#include <QString>

namespace Act {
Q_NAMESPACE
enum class RoleAct {
    Stand,
    Move,
    Sleeping,
    Angry,
    Sitting_1,
    Sitting_2
};
Q_ENUM_NS(RoleAct)

enum class CharacterForm {
    Solyn        = 0,
    StarDaughter = 1
};
Q_ENUM_NS(CharacterForm)

// Canonical string action-ids (JSON-facing keys of the runtime state machine).
namespace Id {
inline const QString Stand     = QStringLiteral("stand");
inline const QString Move      = QStringLiteral("move");
inline const QString Sleeping  = QStringLiteral("sleeping");
inline const QString Angry     = QStringLiteral("angry");
inline const QString Sitting_1 = QStringLiteral("sitting_1");
inline const QString Sitting_2 = QStringLiteral("sitting_2");
}

// Runtime reflection: RoleAct -> lowercase action-id ("Sitting_1" -> "sitting_1").
inline QString actionIdFor(RoleAct act) {
    const QMetaEnum me = QMetaEnum::fromType<RoleAct>();
    const char *key = me.valueToKey(static_cast<int>(act));
    return key ? QString::fromLatin1(key).toLower() : Id::Stand;
}

// Runtime reflection: action-id -> RoleAct. Unknown (custom JSON-defined) ids
// map to `fallback` so RoleAct-typed consumers keep Stand-like semantics.
inline RoleAct roleActFromId(const QString &id, RoleAct fallback = RoleAct::Stand) {
    const QMetaEnum me = QMetaEnum::fromType<RoleAct>();
    for (int i = 0; i < me.keyCount(); ++i) {
        if (id.compare(QLatin1String(me.key(i)), Qt::CaseInsensitive) == 0)
            return static_cast<RoleAct>(me.value(i));
    }
    return fallback;
}
}
using namespace Act;

#endif // ROLEACT_H
