#ifndef CWNOTELIDARTRANSFORMATIONDATA_H
#define CWNOTELIDARTRANSFORMATIONDATA_H

//Our includes
#include "cwNoteTransformationData.h"

//Qt includes
#include <QQuaternion>

struct cwNoteLiDARTransformationData : public cwNoteTransformationData {
    enum class UpMode : int {
        Custom,
        XisUp,
        YisUp,
        ZisUp
    };

    QQuaternion upRotation;
    UpMode upMode;

};

#endif // CWNOTELIDARTRANSFORMATIONDATA_H
