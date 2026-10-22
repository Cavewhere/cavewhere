/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPOCKETTOPOFILE_H
#define CWPOCKETTOPOFILE_H

//Our includes
#include "CaveWhereLibExport.h"
#include "Monad/Result.h"

//Qt includes
#include <QByteArray>
#include <QDate>
#include <QList>
#include <QString>

//Std includes
#include <cstdint>
#include <optional>

/**
 * A PocketTopo `.top` file (version 3), read as the values it stores: little
 * endian .NET BinaryWriter records, strings with a 7-bit length prefix and
 * UTF-8 bytes, dates as DateTime ticks, angles in 65536ths of a circle, and
 * distances and coordinates in millimeters. The records keep the raw values
 * so a file reads back exactly; accessors convert them to degrees, meters and
 * dates. The plan and side drawings are skipped and only their element
 * counts kept. See plans/pockettopo-format.md for the layout.
 */
namespace cwPocketTopoFile {

//! The station id a `.top` file stores: undefined (a splay's far end), a
//! plain number ("7"), or major.minor ("1.5"). "0" and "0.0" are different
//! stations.
class CAVEWHERE_LIB_EXPORT StationId
{
public:
    static constexpr quint32 kUndefinedRaw = 0x80000000u;

    StationId() = default;

    static StationId fromRaw(quint32 raw);
    static StationId plain(quint32 number);
    static StationId dotted(quint16 major, quint16 minor);

    quint32 raw() const;
    bool isDefined() const;
    bool isPlain() const;

    //! The name PocketTopo shows: "1.5", "7", or empty for an undefined or
    //! reserved id.
    QString name() const;

    bool operator==(const StationId& other) const = default;

private:
    quint32 m_raw = kUndefinedRaw;
};

struct CAVEWHERE_LIB_EXPORT Trip {
    static constexpr qint16 kAutoDeclination = -32768;

    //! .NET DateTime ticks: 100 ns since 0001-01-01, local calendar time.
    qint64 ticks = 0;
    QString comment;
    //! Signed 65536ths of a circle, kAutoDeclination for Auto.
    qint16 declination = 0;

    QDate date() const;
    bool isAutoDeclination() const;
    //! The manual declination in degrees; 0 for Auto.
    double declinationDegrees() const;

    bool operator==(const Trip& other) const = default;
};

struct CAVEWHERE_LIB_EXPORT Shot {
    enum Flag : quint8 {
        Flipped = 0x01,
        HasComment = 0x02,
    };

    static constexpr qint32 kBlankDistance = INT32_MIN;
    static constexpr qint16 kNoTrip = -1;

    StationId from;
    StationId to;
    qint32 distance = 0;    //!< millimeters, kBlankDistance when blank
    quint16 azimuth = 0;    //!< unsigned 65536ths of a circle
    qint16 inclination = 0; //!< signed 65536ths of a circle
    quint8 flags = 0;
    quint8 roll = 0;        //!< 256ths of a circle
    qint16 tripIndex = kNoTrip;
    //! Stored only when flags has HasComment.
    QString comment;

    bool isSplay() const;
    bool hasComment() const;
    bool isFlipped() const;
    //! Meters, or nothing for a blank distance.
    std::optional<double> distanceMeters() const;
    //! [0, 360)
    double azimuthDegrees() const;
    //! [-180, 180)
    double inclinationDegrees() const;

    bool operator==(const Shot& other) const = default;
};

struct CAVEWHERE_LIB_EXPORT Reference {
    static constexpr qint64 kBlankCoordinate = INT64_MIN;
    static constexpr qint32 kBlankAltitude = INT32_MIN;

    StationId station;
    qint64 east = 0;     //!< millimeters
    qint64 north = 0;    //!< millimeters
    qint32 altitude = 0; //!< millimeters above sea level
    QString comment;

    std::optional<double> eastMeters() const;
    std::optional<double> northMeters() const;
    std::optional<double> altitudeMeters() const;

    bool operator==(const Reference& other) const = default;
};

struct CAVEWHERE_LIB_EXPORT Document {
    QList<Trip> trips;
    QList<Shot> shots;
    QList<Reference> references;
    //! Elements (polylines and cross-sections) in the skipped drawings.
    int planElementCount = 0;
    int sideElementCount = 0;

    bool operator==(const Document& other) const = default;
};

/**
 * The document \a bytes hold, or an error for a file that is not a version 3
 * `.top`, ends before the side drawing does, has a negative table count,
 * holds a date outside DateTime's range, or a string length longer than five
 * bytes. Bytes after the side drawing are ignored.
 */
CAVEWHERE_LIB_EXPORT Monad::Result<Document> read(const QByteArray& bytes);

//! read() on the file at \a path, or an error when it cannot be opened.
CAVEWHERE_LIB_EXPORT Monad::Result<Document> readFile(const QString& path);

} // namespace cwPocketTopoFile

#endif // CWPOCKETTOPOFILE_H
