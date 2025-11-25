#pragma once

struct cwTriangulateWarpingData
{
    double gridResolutionMeters = 0.5;
    int maxClosestStations = 10;
    double shotInterpolationSpacingMeters = 0.5;
    double smoothingRadiusMeters = 2.0;
    bool useShotInterpolationSpacing = true;
    bool useMaxClosestStations = true;
    bool useSmoothingRadius = true;
};
