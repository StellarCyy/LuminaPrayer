#ifndef ROLEACT_H
#define ROLEACT_H

#include <QObject>

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
}
using namespace Act;

#endif // ROLEACT_H
