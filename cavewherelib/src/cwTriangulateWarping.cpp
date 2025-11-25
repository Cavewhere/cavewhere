#include "cwTriangulateWarping.h"

cwTriangulateWarping::cwTriangulateWarping(QObject* parent) :
    QObject(parent)
{
}

void cwTriangulateWarping::setData(const cwTriangulateWarpingData &data)
{
    setGridResolutionMeters(data.gridResolutionMeters);
    setMaxClosestStations(data.maxClosestStations);
    setShotInterpolationSpacingMeters(data.shotInterpolationSpacingMeters);
    setSmoothingRadiusMeters(data.smoothingRadiusMeters);
    setUseShotInterpolationSpacing(data.useShotInterpolationSpacing);
    setUseMaxClosestStations(data.useMaxClosestStations);
    setUseSmoothingRadius(data.useSmoothingRadius);
}

cwTriangulateWarpingData cwTriangulateWarping::data() const
{
    return m_settings;
}

double cwTriangulateWarping::gridResolutionMeters() const
{
    return m_settings.gridResolutionMeters;
}

void cwTriangulateWarping::setGridResolutionMeters(double distance)
{
    if(m_settings.gridResolutionMeters == distance) {
        return;
    }

    m_settings.gridResolutionMeters = distance;
    emit gridResolutionMetersChanged();
}

int cwTriangulateWarping::maxClosestStations() const
{
    return m_settings.maxClosestStations;
}

void cwTriangulateWarping::setMaxClosestStations(int count)
{
    if(m_settings.maxClosestStations == count) {
        return;
    }

    m_settings.maxClosestStations = count;
    emit maxClosestStationsChanged();
}

double cwTriangulateWarping::shotInterpolationSpacingMeters() const
{
    return m_settings.shotInterpolationSpacingMeters;
}

void cwTriangulateWarping::setShotInterpolationSpacingMeters(double spacing)
{
    if(m_settings.shotInterpolationSpacingMeters == spacing) {
        return;
    }

    m_settings.shotInterpolationSpacingMeters = spacing;
    emit shotInterpolationSpacingMetersChanged();
}

double cwTriangulateWarping::smoothingRadiusMeters() const
{
    return m_settings.smoothingRadiusMeters;
}

void cwTriangulateWarping::setSmoothingRadiusMeters(double distance)
{
    if(m_settings.smoothingRadiusMeters == distance) {
        return;
    }

    m_settings.smoothingRadiusMeters = distance;
    emit smoothingRadiusMetersChanged();
}

bool cwTriangulateWarping::useShotInterpolationSpacing() const
{
    return m_settings.useShotInterpolationSpacing;
}

void cwTriangulateWarping::setUseShotInterpolationSpacing(bool enabled)
{
    if(m_settings.useShotInterpolationSpacing == enabled) {
        return;
    }

    m_settings.useShotInterpolationSpacing = enabled;
    emit useShotInterpolationSpacingChanged();
}

bool cwTriangulateWarping::useMaxClosestStations() const
{
    return m_settings.useMaxClosestStations;
}

void cwTriangulateWarping::setUseMaxClosestStations(bool enabled)
{
    if(m_settings.useMaxClosestStations == enabled) {
        return;
    }

    m_settings.useMaxClosestStations = enabled;
    emit useMaxClosestStationsChanged();
}

bool cwTriangulateWarping::useSmoothingRadius() const
{
    return m_settings.useSmoothingRadius;
}

void cwTriangulateWarping::setUseSmoothingRadius(bool enabled)
{
    if(m_settings.useSmoothingRadius == enabled) {
        return;
    }

    m_settings.useSmoothingRadius = enabled;
    emit useSmoothingRadiusChanged();
}

void cwTriangulateWarping::resetToDefaults()
{
    setData(cwTriangulateWarpingData());
}
