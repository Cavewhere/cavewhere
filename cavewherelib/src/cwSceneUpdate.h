#ifndef CWSCENEUPDATE_H
#define CWSCENEUPDATE_H

//Qt includes
#include <QString>
#include <QDebug>

namespace cwSceneUpdate {
enum class Flag : int {
    None = 0,
    ViewMatrix = 0x1,
    ProjectionMatrix = 0x2,
    DevicePixelRatio = 0x4,
};

inline Flag operator|(Flag lhs, Flag rhs) {
    return static_cast<cwSceneUpdate::Flag>(static_cast<int>(lhs) | static_cast<int>(rhs));
}
inline Flag operator&(Flag lhs, Flag rhs) {
    return static_cast<cwSceneUpdate::Flag>(static_cast<int>(lhs) & static_cast<int>(rhs));
}
inline Flag& operator|=(Flag& lhs, Flag rhs) {
    lhs = lhs | rhs;
    return lhs;
}

inline bool isFlagSet(cwSceneUpdate::Flag combinedFlags, cwSceneUpdate::Flag flagToTest) {
    return (combinedFlags & flagToTest) == flagToTest;
}

// Helper function to convert the enum to a readable string
QString flagToString(Flag flag);

// Overload operator<< for QDebug to handle cwSceneUpdate::Flag
inline QDebug operator<<(QDebug dbg, Flag flag) {
    dbg.nospace() << flagToString(flag);
    return dbg.space();
}

};


#endif // CWSCENEUPDATE_H
