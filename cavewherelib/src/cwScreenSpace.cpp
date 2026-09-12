// Our includes
#include "cwScreenSpace.h"

// Qt includes
#include <QVector3D>

// Std includes
#include <algorithm>
#include <limits>

namespace {

    //Clip space spans [-1, 1] vertically, so a projected meter covers
    //absP11 / kClipHeight of the viewport per unit w
    constexpr double kClipHeight = 2.0;

    constexpr int kBoxCornerCount = 8;

    //Sentinel for "no corner projected in front of the eye"
    constexpr double kNoPositiveCorner = std::numeric_limits<double>::max();
}

namespace cw::sse {

double nearestClipW(const QBox3D& worldBounds, const QMatrix4x4& viewProjection)
{
    const QVector3D minimum = worldBounds.minimum();
    const QVector3D maximum = worldBounds.maximum();

    double nearest = kNoPositiveCorner;
    for(int corner = 0; corner < kBoxCornerCount; corner++) {
        const QVector3D point((corner & 1) != 0 ? maximum.x() : minimum.x(),
                              (corner & 2) != 0 ? maximum.y() : minimum.y(),
                              (corner & 4) != 0 ? maximum.z() : minimum.z());
        const double w = double(viewProjection(3, 0)) * double(point.x())
                         + double(viewProjection(3, 1)) * double(point.y())
                         + double(viewProjection(3, 2)) * double(point.z())
                         + double(viewProjection(3, 3));
        if(w > 0.0) {
            nearest = std::min(nearest, w);
        }
    }

    if(nearest == kNoPositiveCorner) {
        return kMinimumClipW;
    }
    return std::max(nearest, kMinimumClipW);
}

double pixelsPerMeter(double absP11, int viewportHeightPx, double clipW)
{
    return absP11 * double(viewportHeightPx) / (kClipHeight * clipW);
}

double projectedPixels(double worldLength,
                       const QBox3D& worldBounds,
                       const QMatrix4x4& viewProjection,
                       double absP11,
                       int viewportHeightPx)
{
    const double clipW = nearestClipW(worldBounds, viewProjection);
    return worldLength * pixelsPerMeter(absP11, viewportHeightPx, clipW);
}

}
