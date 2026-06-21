/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCOORDINATEPICKER_H
#define CWCOORDINATEPICKER_H

//Our includes
#include "cwScenePicker.h"
#include "cwGeoPoint.h"
class cwCoordinateTransform;
class cwGeoReference;

//Qt includes
#include <QPointer>
#include <QPointF>
#include <QVector3D>
#include <QString>

//Std
#include <memory>

/**
 * Interaction that ray-casts a screen-space click against the geometry
 * intersector and exposes the hit point in three coordinate frames:
 * local scene XYZ, the region's projected CRS, and WGS84 lat/lon.
 *
 * Lives in InteractionManager like any other Interaction — toggle via
 * activate()/deactivate(); the manager's signal wiring restores the
 * default (turn-table) interaction when deactivated.
 */
class cwCoordinatePicker : public cwScenePicker
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CoordinatePicker)

    Q_PROPERTY(cwGeoReference* geoReference READ geoReference WRITE setGeoReference NOTIFY geoReferenceChanged)

    Q_PROPERTY(bool hasPick READ hasPick NOTIFY pickChanged)
    Q_PROPERTY(QPointF pickScreenPoint READ pickScreenPoint NOTIFY pickChanged)
    Q_PROPERTY(QVector3D scenePoint READ scenePoint NOTIFY pickChanged)
    Q_PROPERTY(double csX READ csX NOTIFY pickChanged)
    Q_PROPERTY(double csY READ csY NOTIFY pickChanged)
    Q_PROPERTY(double csZ READ csZ NOTIFY pickChanged)
    Q_PROPERTY(double wgs84Latitude  READ wgs84Latitude  NOTIFY pickChanged)
    Q_PROPERTY(double wgs84Longitude READ wgs84Longitude NOTIFY pickChanged)
    Q_PROPERTY(double elevation READ elevation NOTIFY pickChanged)
    Q_PROPERTY(QString globalCoordinateSystem READ globalCoordinateSystem NOTIFY pickChanged)
    Q_PROPERTY(bool hasWgs84 READ hasWgs84 NOTIFY pickChanged)

public:
    explicit cwCoordinatePicker(QQuickItem* parent = nullptr);
    ~cwCoordinatePicker() override;

    cwGeoReference* geoReference() const;
    void setGeoReference(cwGeoReference* geoReference);

    bool hasPick() const { return m_hasPick; }
    QPointF pickScreenPoint() const { return m_pickScreenPoint; }
    QVector3D scenePoint() const { return m_scenePoint; }
    double csX() const { return m_globalPoint.x; }
    double csY() const { return m_globalPoint.y; }
    double csZ() const { return m_globalPoint.z; }
    double wgs84Latitude()  const { return m_wgs84Lat; }
    double wgs84Longitude() const { return m_wgs84Lon; }
    double elevation() const { return m_globalPoint.z; }
    QString globalCoordinateSystem() const { return m_globalCoordinateSystemCached; }
    bool hasWgs84() const { return m_hasWgs84; }

    Q_INVOKABLE void pick(QPointF screenPoint);
    Q_INVOKABLE void clearPick();

signals:
    void geoReferenceChanged();
    void pickChanged();

private slots:
    void rebuildWgs84Transform();

private:
    QPointer<cwGeoReference> m_geoReference;

    bool m_hasPick = false;
    bool m_hasWgs84 = false;
    QPointF m_pickScreenPoint;
    QVector3D m_scenePoint;
    cwGeoPoint m_globalPoint;
    double m_wgs84Lat = 0.0;
    double m_wgs84Lon = 0.0;
    QString m_globalCoordinateSystemCached;

    // PROJ setup (proj_create_crs_to_crs + normalize) is non-trivial. Cache the
    // transform and rebuild only when the geo-reference's CS changes.
    std::unique_ptr<cwCoordinateTransform> m_wgs84Transform;
};

#endif // CWCOORDINATEPICKER_H
