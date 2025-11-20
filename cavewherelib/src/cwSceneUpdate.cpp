#include "cwSceneUpdate.h"

QString cwSceneUpdate::flagToString(Flag flag) {
    QStringList flagNames;
    if (flag == Flag::None) {
        flagNames << "None";
    } else {
        if (isFlagSet(flag, Flag::ViewMatrix)) {
            flagNames << "ViewMatrix";
        }
        if (isFlagSet(flag, Flag::ProjectionMatrix)) {
            flagNames << "ProjectionMatrix";
        }
        if (isFlagSet(flag, Flag::DevicePixelRatio)) {
            flagNames << "DevicePixelRatio";
        }
    }
    return flagNames.join(" | ");
}
