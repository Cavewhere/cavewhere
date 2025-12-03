#pragma once

#include <QtGlobal>

struct cwTriangulateWarpingData
{
#ifdef QT_DEBUG
    //Testcases run really slow with high resolution, likely a bug
    double gridResolutionMeters = 2.5;
#else
    double gridResolutionMeters = 0.5;
#endif
    int maxClosestStations = 10;
    double shotInterpolationSpacingMeters = 0.5;
    double smoothingRadiusMeters = 2.0;
    bool useShotInterpolationSpacing = true;
    bool useMaxClosestStations = true;
    bool useSmoothingRadius = true;
};
