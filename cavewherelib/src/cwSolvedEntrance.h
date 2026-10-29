/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSOLVEDENTRANCE_H
#define CWSOLVEDENTRANCE_H

//Qt includes
#include <QString>
#include <QUuid>
#include <QVector3D>

/**
 * One station the last solve flagged as an entrance: a native entrance the
 * driver wrote as *entrance, a Survex *entrance, a Walls ENTRANCE flag or a
 * Compass .mak fixed point. Grouped by the node that owns the station.
 */
struct cwSolvedEntrance {
    //! The station's name below the owning node, as cavern reports it: lower
    //! case for a native station, so match it case-insensitively.
    QString stationName;
    //! The full cavern name, '.'-separated.
    QString qualifiedName;
    QVector3D position;
    //! True when the data fixes the station; the origin the driver invents for
    //! an unfixed survey reads false.
    bool fixed = false;
    //! The deepest node holding the station: the Compass .dat or Survex block
    //! node below the owning node that cavern scopes an attached station
    //! under, else the owning node itself.
    QUuid deepestNodeId;

    bool operator==(const cwSolvedEntrance& other) const = default;
};

#endif // CWSOLVEDENTRANCE_H
