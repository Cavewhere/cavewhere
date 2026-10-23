/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWFILEDECLINATION_H
#define CWFILEDECLINATION_H

//Our includes
#include "cwGeoPoint.h"

//Qt includes
#include <QString>

/**
 * The declination an attached survey file sets for one unit of shots, as
 * cavern applies it.
 *
 * Kind:
 *   Unknown   - the scan cannot tell (the unit mixes declinations, or the
 *               file sets one the scan does not model)
 *   Inherited - a trip-level file is silent, so the driver injects the trip's
 *               own calibration
 *   None      - the file sets nothing, so cavern applies 0
 *   Value     - the file sets degrees, east-positive as cavern applies it
 *   Auto      - the file asks cavern for IGRF at autoLocation, read in
 *               autoCoordinateSystem (the *cs argument as written)
 *   AutoAtNode - the file asks for IGRF but names no location, so CaveWhere
 *               reads it at the owning node's fix station: 0 until the node
 *               has one, and never a value the user types
 */
struct cwFileDeclination
{
    enum class Kind {
        Unknown,
        Inherited,
        None,
        Value,
        Auto,
        AutoAtNode
    };

    Kind kind = Kind::Unknown;
    double degrees = 0.0;
    cwGeoPoint autoLocation;
    QString autoCoordinateSystem;

    bool operator==(const cwFileDeclination& other) const = default;
};

#endif // CWFILEDECLINATION_H
