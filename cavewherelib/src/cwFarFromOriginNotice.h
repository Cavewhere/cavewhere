/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWFARFROMORIGINNOTICE_H
#define CWFARFROMORIGINNOTICE_H

//Qt includes
#include <QObject>
#include <QQmlEngine>

//Our includes
#include "cwGlobals.h"

class cwCavingRegion;
class cwStationPositionLookup;

/**
 * Whether the region's solved survey sits far from the scene origin in a
 * project with no geographic coordinate system. With no frame, the solve's raw
 * output is the scene, so a file fixed in projected coordinates lands
 * thousands of kilometers from where the 3D view opens. The banner that tells
 * the user so reads shown and distanceFromOrigin from here.
 *
 * Re-evaluated once per published solve and once per frame change. Choosing a
 * system clears shown as soon as the frame lands, before the re-solve.
 * Owned by cwCavingRegion (region.farFromOriginNotice).
 */
class CAVEWHERE_LIB_EXPORT cwFarFromOriginNotice : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(FarFromOriginNotice)
    QML_UNCREATABLE("Owned by CavingRegion; access via region.farFromOriginNotice")

    //! The horizontal distance, in meters, of the solved stations' bounding
    //! box center from the scene origin; 0 when nothing is solved.
    Q_PROPERTY(double distanceFromOrigin READ distanceFromOrigin NOTIFY distanceFromOriginChanged FINAL)

    //! True when the project has no frame and the survey sits farther out
    //! than kFarFromOriginMeters.
    Q_PROPERTY(bool shown READ isShown NOTIFY shownChanged FINAL)

public:
    //! Every projected easting lies beyond this (UTM's start at 166 km, state
    //! plane false eastings are hundreds of km), and local site grids such as
    //! 1000/1000 or 5000/5000 lie well within it.
    static constexpr double kFarFromOriginMeters = 50000.0;

    explicit cwFarFromOriginNotice(cwCavingRegion* region);

    double distanceFromOrigin() const { return m_distanceFromOrigin; }
    bool isShown() const { return m_shown; }

    //! The horizontal distance of \a positions' axis-aligned bounding box
    //! center from the origin, or 0 for an empty lookup.
    static double solvedCenterDistance(const cwStationPositionLookup& positions);

signals:
    void distanceFromOriginChanged();
    void shownChanged();

private:
    void setDistanceFromOrigin(double distance);
    void setShown(bool shown);
    void updateShown();

    cwCavingRegion* m_region = nullptr;
    double m_distanceFromOrigin = 0.0;
    bool m_shown = false;
};

#endif // CWFARFROMORIGINNOTICE_H
