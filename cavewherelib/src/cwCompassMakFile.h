/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCOMPASSMAKFILE_H
#define CWCOMPASSMAKFILE_H

//Our includes
#include "cwGlobals.h"

//Qt includes
#include <QList>
#include <QString>
#include <QStringList>

//Std includes
#include <array>
#include <optional>

/**
 * A Compass project file (.mak) read the way cavern's .mak reader reads it
 * (survex datain.c data_file_compass_mak): the DAT files it lists in order,
 * each with the stations its '#' line fixes and links, and the datum, UTM zone
 * and base location in force on that line.
 *
 * The syntax is free-form: a statement starts with a command character and
 * ends with ';', a comment runs from '/' to the next '/' or the end of the
 * line, and a '#' line's link-station list can span lines. One divergence
 * from cavern: a '/' inside a '#' file name is a path separator, as the
 * scanner has always read it, so a DAT in a subdirectory is found.
 */
namespace cwCompassMakFile {

//! A station a '#' line fixes: "name[f,x,y,z]" or "name[m,x,y,z]".
struct Fix {
    QString station;
    //! The bracket's text as written, trimmed ("f,1568241.5,14534120.7,5429.8").
    QString coordinate;
    //! The coordinate in meters, feet converted the way cavern converts them.
    //! Empty when the bracket does not hold a unit and three numbers.
    std::optional<std::array<double, 3>> meters;

    bool operator==(const Fix& other) const = default;
};

//! A base location ('@'): where Compass computes the declination, in meters,
//! and the UTM zone it is written in. Cavern ignores the convergence field.
struct BaseLocation {
    double easting = 0.0;
    double northing = 0.0;
    double elevation = 0.0;
    int zone = 0;

    bool operator==(const BaseLocation& other) const = default;
};

//! One '#' line: a DAT file and what is in force for it.
struct DatReference {
    //! The file as written, quotes removed, '\' turned into '/', prefixed with
    //! the folders ('[') open around it.
    QString file;
    //! The datum in force: the last '&' line's text, Compass's default when the
    //! .mak names a zone and has no '&' line, or empty.
    QString datum;
    //! The last '$' zone, 0 when none.
    int zone = 0;
    //! True once a '&' or '$' line came before this one, after which the
    //! system around the .mak no longer stands for this one's.
    bool namesSystem = false;
    //! The last base location before this line.
    std::optional<BaseLocation> baseLocation;
    //! Stations listed without a bracket, in order.
    QStringList linkStations;
    QList<Fix> fixes;

    //! The UTM zone the fixes on this line are read in: the '$' zone when a
    //! datum and a zone are both in force, else the base location's zone when
    //! a datum is in force, else 0. Cavern applies a base location only while
    //! no other input system is set; a caller with a system around the .mak
    //! uses it while namesSystem is false.
    int fixZone() const;
    //! "<datum>, UTM zone <zone><N|S>" for fixZone(), or empty when it is 0.
    QString coordinateSystemName() const;
    //! The system cavern reads fixZone() in (survex img.c
    //! img_compass_utm_proj_str), as *cs takes it: "EPSG:<code>", a PROJ
    //! string, or empty when the datum is one cavern does not know.
    QString coordinateSystem() const;
    //! The system the base location is read in, the same way.
    QString baseLocationCoordinateSystem() const;

    bool operator==(const DatReference& other) const = default;
};

struct Project {
    QList<DatReference> references;
    //! True when the .mak names a zone ('$' or '@') and has no '&' line, so
    //! its zones are read in Compass's default datum.
    bool datumDefaulted = false;
    //! The zone the .mak names last, its '$' line's or else its base
    //! location's; 0 when it names none.
    int namedZone = 0;
};

CAVEWHERE_LIB_EXPORT Project parse(const QString& text);

//! True when \a path names a Compass project file (.mak, any case).
CAVEWHERE_LIB_EXPORT bool isMakFile(const QString& path);

/**
 * The survey cavern reads a '#' line's DAT into (survex datain.c
 * mak_dat_survey): the DAT's leaf name without its extension, lower-cased,
 * or "dat" when that leaves nothing.
 */
CAVEWHERE_LIB_EXPORT QString datSurveyName(const QString& datReference);

//! A UTM zone as a system name spells it: "<zone><N|S>", positive zones north.
CAVEWHERE_LIB_EXPORT QString utmZoneName(int zone);

//! A UTM system's name: "<datum>, UTM zone <zone><N|S>", or "UTM zone ..."
//! with no datum.
CAVEWHERE_LIB_EXPORT QString utmSystemName(const QString& datum, int zone);

/**
 * The system cavern reads a Compass datum and UTM zone in (survex img.c
 * img_compass_utm_proj_str): "EPSG:<code>" for the pairs EPSG catalogs, a
 * PROJ string for a NAD27 or NAD83 zone it does not, and empty for a datum
 * cavern does not recognize or a zone outside -60..60.
 */
CAVEWHERE_LIB_EXPORT QString utmCoordinateSystem(const QString& datum, int zone);

} // namespace cwCompassMakFile

#endif // CWCOMPASSMAKFILE_H
