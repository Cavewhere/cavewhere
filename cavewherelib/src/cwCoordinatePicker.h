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
 * local scene XYZ, the region's projected CRS, and lat/lon on the datum the
 * host names.
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
    Q_PROPERTY(QString datum READ datum WRITE setDatum NOTIFY datumChanged)

    Q_PROPERTY(bool hasPick READ hasPick NOTIFY pickChanged)
    Q_PROPERTY(QPointF pickScreenPoint READ pickScreenPoint NOTIFY pickChanged)
    Q_PROPERTY(QVector3D scenePoint READ scenePoint NOTIFY pickChanged)
    Q_PROPERTY(double latitude  READ latitude  NOTIFY pickChanged)
    Q_PROPERTY(double longitude READ longitude NOTIFY pickChanged)
    Q_PROPERTY(double elevation READ elevation NOTIFY pickChanged)
    Q_PROPERTY(bool hasCoordinateSystem READ hasCoordinateSystem NOTIFY coordinateSystemChanged)
    Q_PROPERTY(bool hasLatLon READ hasLatLon NOTIFY pickChanged)

public:
    explicit cwCoordinatePicker(QQuickItem* parent = nullptr);
    ~cwCoordinatePicker() override;

    cwGeoReference* geoReference() const;
    void setGeoReference(cwGeoReference* geoReference);

    //! The geographic datum latitude() and longitude() read on, as a geographic
    //! EPSG code. Hosts bind it to cwCavingRegion::defaultFixDatum so numbers
    //! copied out of the readout land on the same datum a fix defaults to.
    //!
    //! Reads back the datum the numbers are actually on, which is why a code
    //! cwCoordinateSystem's table doesn't name resolves to WGS84 here rather
    //! than being stored as written.
    QString datum() const { return m_datum; }
    void setDatum(const QString& datum);

    bool hasPick() const { return m_hasPick; }
    QPointF pickScreenPoint() const { return m_pickScreenPoint; }
    QVector3D scenePoint() const { return m_scenePoint; }
    double latitude()  const { return m_latitude; }
    double longitude() const { return m_longitude; }
    double elevation() const { return double(m_scenePoint.z()); }

    //! Whether the pick can be placed in a real-world CRS. Delegates to the
    //! geo-reference's single definition (cwGeoReference::hasCoordinateSystem)
    //! so consumers don't re-derive the rule.
    bool hasCoordinateSystem() const;

    bool hasLatLon() const { return m_hasLatLon; }

    Q_INVOKABLE void pick(QPointF screenPoint);
    Q_INVOKABLE void clearPick();

signals:
    void geoReferenceChanged();
    void datumChanged();
    void pickChanged();
    void coordinateSystemChanged();

private slots:
    void rebuildLatLonTransform();

private:
    void updateLatLon();

    QPointer<cwGeoReference> m_geoReference;
    QString m_datum;

    bool m_hasPick = false;
    bool m_hasLatLon = false;
    QPointF m_pickScreenPoint;
    QVector3D m_scenePoint;
    double m_latitude = 0.0;
    double m_longitude = 0.0;

    // PROJ setup (proj_create_crs_to_crs + normalize) is non-trivial. Cache the
    // transform and rebuild only when the geo-reference's frame or the datum
    // changes.
    std::unique_ptr<cwCoordinateTransform> m_latLonTransform;
};

#endif // CWCOORDINATEPICKER_H
