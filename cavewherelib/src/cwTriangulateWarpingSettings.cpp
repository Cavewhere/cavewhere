#include "cwTriangulateWarpingSettings.h"

cwTriangulateWarpingSettings::cwTriangulateWarpingSettings(cwTriangulateWarping* warping, QObject* parent)
    : QObject(parent)
    , m_warping(warping)
{
    load();

    if(!m_warping) {
        return;
    }

    auto connectAndSave = [this](auto signal) {
        connect(m_warping, signal, this, &cwTriangulateWarpingSettings::save);
    };

    connectAndSave(&cwTriangulateWarping::gridResolutionMetersChanged);
    connectAndSave(&cwTriangulateWarping::maxClosestStationsChanged);
    connectAndSave(&cwTriangulateWarping::shotInterpolationSpacingMetersChanged);
    connectAndSave(&cwTriangulateWarping::smoothingRadiusMetersChanged);
    connectAndSave(&cwTriangulateWarping::useShotInterpolationSpacingChanged);
    connectAndSave(&cwTriangulateWarping::useMaxClosestStationsChanged);
    connectAndSave(&cwTriangulateWarping::useSmoothingRadiusChanged);
}

void cwTriangulateWarpingSettings::load()
{
    if(!m_warping) {
        return;
    }

    m_loading = true;
    QSettings settings;
    settings.beginGroup(QStringLiteral("TriangulateWarping"));

    const auto getDouble = [&](const QString& name, double fallback) {
        return settings.value(name, fallback).toDouble();
    };

    const auto getInt = [&](const QString& name, int fallback) {
        return settings.value(name, fallback).toInt();
    };

    const auto getBool = [&](const QString& name, bool fallback) {
        return settings.value(name, fallback).toBool();
    };

    m_warping->setGridResolutionMeters(getDouble(QStringLiteral("gridResolutionMeters"),
                                                 m_warping->gridResolutionMeters()));
    m_warping->setMaxClosestStations(getInt(QStringLiteral("maxClosestStations"),
                                            m_warping->maxClosestStations()));
    m_warping->setShotInterpolationSpacingMeters(getDouble(QStringLiteral("shotInterpolationSpacingMeters"),
                                                           m_warping->shotInterpolationSpacingMeters()));
    m_warping->setSmoothingRadiusMeters(getDouble(QStringLiteral("smoothingRadiusMeters"),
                                                  m_warping->smoothingRadiusMeters()));

    m_warping->setUseShotInterpolationSpacing(getBool(QStringLiteral("useShotInterpolationSpacing"),
                                                      m_warping->useShotInterpolationSpacing()));
    m_warping->setUseMaxClosestStations(getBool(QStringLiteral("useMaxClosestStations"),
                                                m_warping->useMaxClosestStations()));
    m_warping->setUseSmoothingRadius(getBool(QStringLiteral("useSmoothingRadius"),
                                             m_warping->useSmoothingRadius()));

    settings.endGroup();
    m_loading = false;
}

void cwTriangulateWarpingSettings::save() const
{
    if(m_loading || !m_warping) {
        return;
    }

    QSettings settings;
    settings.beginGroup(QStringLiteral("TriangulateWarping"));
    settings.setValue(QStringLiteral("gridResolutionMeters"), m_warping->gridResolutionMeters());
    settings.setValue(QStringLiteral("maxClosestStations"), m_warping->maxClosestStations());
    settings.setValue(QStringLiteral("shotInterpolationSpacingMeters"), m_warping->shotInterpolationSpacingMeters());
    settings.setValue(QStringLiteral("smoothingRadiusMeters"), m_warping->smoothingRadiusMeters());
    settings.setValue(QStringLiteral("useShotInterpolationSpacing"), m_warping->useShotInterpolationSpacing());
    settings.setValue(QStringLiteral("useMaxClosestStations"), m_warping->useMaxClosestStations());
    settings.setValue(QStringLiteral("useSmoothingRadius"), m_warping->useSmoothingRadius());
    settings.endGroup();
}
