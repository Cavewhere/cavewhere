#ifndef CWSYNCTYPES_H
#define CWSYNCTYPES_H

#include <QObject>

namespace cwSyncTypes {
Q_NAMESPACE

enum class BranchResetMode : int {
    Soft = 0,
    Mixed = 1,
    Hard = 2
};
Q_ENUM_NS(BranchResetMode)

} // namespace cwSyncTypes

#endif // CWSYNCTYPES_H
