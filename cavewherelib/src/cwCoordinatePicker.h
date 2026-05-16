/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCOORDINATEPICKER_H
#define CWCOORDINATEPICKER_H

//Our includes
#include "cwInteraction.h"
#include "cwGeoPoint.h"
class cwCamera;
class cwScene;
class cwCavingRegion;
class cwCoordinateTransform;

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
class cwCoordinatePicker : public cwInteraction
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CoordinatePicker)

    Q_PROPERTY(cwCamera* camera READ camera WRITE setCamera NOTIFY cameraChanged)
    Q_PROPERTY(cwScene* scene READ scene WRITE setScene NOTIFY sceneChanged)
    Q_PROPERTY(cwCavingRegion* region READ region WRITE setRegion NOTIFY regionChanged)

    Q_PROPERTY(bool hasPick READ hasPick NOTIFY pickChanged)
    Q_PROPERTY(QPointF pickScreenPoint READ pickScreenPoint NOTIFY pickChanged)
    Q_PROPERTY(QVector3D scenePoint READ scenePoint NOTIFY pickChanged)
    Q_PROPERTY(double csX READ csX NOTIFY pickChanged)
    Q_PROPERTY(double csY READ csY NOTIFY pickChanged)
    Q_PROPERTY(double csZ READ csZ NOTIFY pickChanged)
    Q_PROPERTY(double wgs84Latitude  READ wgs84Latitude  NOTIFY pickChanged)
    Q_PROPERTY(double wgs84Longitude READ wgs84Longitude NOTIFY pickChanged)
    Q_PROPERTY(double elevation READ elevation NOTIFY pickChanged)
    Q_PROPERTY(QString globalCS READ globalCS NOTIFY pickChanged)
    Q_PROPERTY(bool hasWgs84 READ hasWgs84 NOTIFY pickChanged)

public:
    explicit cwCoordinatePicker(QQuickItem* parent = nullptr);
    ~cwCoordinatePicker() override;

    cwCamera* camera() const { return m_camera; }
    void setCamera(cwCamera* camera);

    cwScene* scene() const { return m_scene; }
    void setScene(cwScene* scene);

    cwCavingRegion* region() const { return m_region; }
    void setRegion(cwCavingRegion* region);

    bool hasPick() const { return m_hasPick; }
    QPointF pickScreenPoint() const { return m_pickScreenPoint; }
    QVector3D scenePoint() const { return m_scenePoint; }
    double csX() const { return m_globalPoint.x; }
    double csY() const { return m_globalPoint.y; }
    double csZ() const { return m_globalPoint.z; }
    double wgs84Latitude()  const { return m_wgs84Lat; }
    double wgs84Longitude() const { return m_wgs84Lon; }
    double elevation() const { return m_globalPoint.z; }
    QString globalCS() const { return m_globalCSCached; }
    bool hasWgs84() const { return m_hasWgs84; }

    Q_INVOKABLE void pick(QPointF screenPoint);
    Q_INVOKABLE void clearPick();

signals:
    void cameraChanged();
    void sceneChanged();
    void regionChanged();
    void pickChanged();

private slots:
    void rebuildWgs84Transform();

private:
    QPointer<cwCamera> m_camera;
    QPointer<cwScene> m_scene;
    QPointer<cwCavingRegion> m_region;

    bool m_hasPick = false;
    bool m_hasWgs84 = false;
    QPointF m_pickScreenPoint;
    QVector3D m_scenePoint;
    cwGeoPoint m_globalPoint;
    double m_wgs84Lat = 0.0;
    double m_wgs84Lon = 0.0;
    QString m_globalCSCached;

    // PROJ setup (proj_create_crs_to_crs + normalize) is non-trivial. Cache the
    // transform and rebuild only when the region's globalCS changes.
    std::unique_ptr<cwCoordinateTransform> m_wgs84Transform;
};

#endif // CWCOORDINATEPICKER_H
