/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef POCKETTOPOFIXTUREWRITER_H
#define POCKETTOPOFIXTUREWRITER_H

//Our includes
#include "cwPocketTopoFile.h"

//Qt includes
#include <QByteArray>
#include <QDate>
#include <QList>
#include <QString>

//Std includes
#include <variant>

/**
 * Writes byte-exact PocketTopo `.top` version 3 files from an in-memory model,
 * following Beat Heeb's format document, so tests build their fixtures from
 * the spec rather than from third-party files.
 */
namespace PocketTopoFixtureWriter {

struct Point {
    qint32 x = 0; //!< millimeters
    qint32 y = 0; //!< millimeters
};

//! A drawing's scroll and zoom: UI state the reader discards.
struct Mapping {
    qint32 x = 0;
    qint32 y = 0;
    qint32 scale = 500;
};

struct Polygon {
    QList<Point> points;
    quint8 color = 1;
};

struct XSection {
    Point position;
    cwPocketTopoFile::StationId station;
    //! -1 for a horizontal section, else a projection azimuth in 65536ths.
    qint32 direction = -1;
};

using Element = std::variant<Polygon, XSection>;

struct Drawing {
    Mapping mapping;
    QList<Element> elements;
};

//! What follows the side drawing: PocketTopo writes an Int32 0.
enum class Trailer {
    Zero,
    None,
};

struct Fixture {
    quint8 version = 3;
    //! The trips, shots and references written; the element counts are
    //! ignored, the drawings below are written instead.
    cwPocketTopoFile::Document document;
    Mapping overview;
    Drawing plan;
    Drawing side;
    Trailer trailer = Trailer::Zero;
};

QByteArray write(const Fixture& fixture);

//! Writes \a fixture to \a path, returning false when the file cannot be written.
bool writeFile(const Fixture& fixture, const QString& path);

//! DateTime ticks for midnight at the start of \a date.
qint64 ticks(const QDate& date);

//! Degrees as an unsigned azimuth in 65536ths of a circle, rounded.
quint16 azimuthUnits(double degrees);

//! Degrees as a signed angle (inclination, declination) in 65536ths, rounded.
qint16 angleUnits(double degrees);

} // namespace PocketTopoFixtureWriter

#endif // POCKETTOPOFIXTUREWRITER_H
