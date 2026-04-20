#ifndef CWNOTETRANSFORMATIONDATA_H
#define CWNOTETRANSFORMATIONDATA_H

#include "cwScale.h"
#include "cwUnits.h"

// Defaults form an identity: north = 0°, scale = 1 m / 1 m (ratio 1.0).
// A zero-initialized scale would give 0 / 0 = NaN and collapse downstream
// math like cwTriangulateTask::createPointGrid.
struct cwNoteTransformationData {
    double north = 0.0;
    cwScale::Data scale = {
        {cwUnits::Meters, 1.0, false},
        {cwUnits::Meters, 1.0, false}
    };
};

#endif // CWNOTETRANSFORMATIONDATA_H
