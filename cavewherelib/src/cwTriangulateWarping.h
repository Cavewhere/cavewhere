#pragma once

// Qt
#include <QObject>

// Ours
#include "cwGlobals.h"
#include "cwTriangulateWarpingData.h"

class CAVEWHERE_LIB_EXPORT cwTriangulateWarping : public QObject
{
    Q_OBJECT

    Q_PROPERTY(double gridResolutionMeters READ gridResolutionMeters WRITE setGridResolutionMeters NOTIFY gridResolutionMetersChanged)
    Q_PROPERTY(int maxClosestStations READ maxClosestStations WRITE setMaxClosestStations NOTIFY maxClosestStationsChanged)
    Q_PROPERTY(double shotInterpolationSpacingMeters READ shotInterpolationSpacingMeters WRITE setShotInterpolationSpacingMeters NOTIFY shotInterpolationSpacingMetersChanged)
    Q_PROPERTY(double smoothingRadiusMeters READ smoothingRadiusMeters WRITE setSmoothingRadiusMeters NOTIFY smoothingRadiusMetersChanged)
    Q_PROPERTY(bool useShotInterpolationSpacing READ useShotInterpolationSpacing WRITE setUseShotInterpolationSpacing NOTIFY useShotInterpolationSpacingChanged)
    Q_PROPERTY(bool useMaxClosestStations READ useMaxClosestStations WRITE setUseMaxClosestStations NOTIFY useMaxClosestStationsChanged)
    Q_PROPERTY(bool useSmoothingRadius READ useSmoothingRadius WRITE setUseSmoothingRadius NOTIFY useSmoothingRadiusChanged)

public:
    explicit cwTriangulateWarping(QObject* parent = nullptr);

    void setData(const cwTriangulateWarpingData& data);
    cwTriangulateWarpingData data() const;

    double gridResolutionMeters() const;
    void setGridResolutionMeters(double distance);

    int maxClosestStations() const;
    void setMaxClosestStations(int count);

    double shotInterpolationSpacingMeters() const;
    void setShotInterpolationSpacingMeters(double spacing);

    double smoothingRadiusMeters() const;
    void setSmoothingRadiusMeters(double distance);

    bool useShotInterpolationSpacing() const;
    void setUseShotInterpolationSpacing(bool enabled);

    bool useMaxClosestStations() const;
    void setUseMaxClosestStations(bool enabled);

    bool useSmoothingRadius() const;
    void setUseSmoothingRadius(bool enabled);

signals:
    void gridResolutionMetersChanged();
    void maxClosestStationsChanged();
    void shotInterpolationSpacingMetersChanged();
    void smoothingRadiusMetersChanged();
    void useShotInterpolationSpacingChanged();
    void useMaxClosestStationsChanged();
    void useSmoothingRadiusChanged();

private:
    cwTriangulateWarpingData m_settings;
};
