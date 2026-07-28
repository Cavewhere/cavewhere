/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWFIXSTATION_H
#define CWFIXSTATION_H

//Qt includes
#include <QSharedDataPointer>
#include <QString>
#include <QUuid>
#include <QMetaType>

//Our includes
#include "cwGlobals.h"

class cwFixStationData;

/**
 * Value class anchoring a named station to absolute coordinates in some
 * input coordinate system. Lives inside cwFixStationModel; serialized as
 * part of cwCave.
 *
 * <b>The coordinate is a string, and the string is the only thing stored.</b>
 * easting(), northing() and elevation() are read out of it by
 * cwCoordinateText::parse(), under the axis order inputCS() implies — so a
 * coordinate means whatever its own text says it means, and correcting a row's
 * coordinate system re-reads it rather than leaving the numbers where a
 * previous reading put them. There is no second representation to keep in step.
 *
 * Copyable / equality-comparable; uses QSharedDataPointer for COW so cheap
 * to pass through QVariant and signals.
 */
class CAVEWHERE_LIB_EXPORT cwFixStation
{
public:
    //! What this fix's coordinate amounts to — see state().
    enum CoordinateState {
        //! No coordinate. A row created by "Mark Station as Fixed" and not yet
        //! filled in, which is not the same as one entered at the origin.
        Empty,
        //! The string reads as a coordinate; the components come from it.
        Valid,
        //! The string is kept, verbatim, but cannot be read as a coordinate,
        //! and the components are all 0. Reachable by hand-editing the project
        //! file — the text is the user's, so it is kept rather than dropped or
        //! repaired. <b>Nothing surfaces this state yet</b>: such a row renders
        //! and solves as a fix at the origin, and only an editor shows the text
        //! that was kept. Writing any component discards it.
        Unreadable
    };

    cwFixStation();
    cwFixStation(const cwFixStation& other);
    cwFixStation& operator=(const cwFixStation& other);
    ~cwFixStation();

    bool operator==(const cwFixStation& other) const;
    bool operator!=(const cwFixStation& other) const { return !(*this == other); }

    QUuid id() const;
    void setId(const QUuid& id);

    QString stationName() const;
    void setStationName(const QString& name);

    //! <b>Set this before any component.</b> It decides which axis the
    //! coordinate leads with, so writing a component while it is still empty
    //! spells the coordinate out easting-first; setting a geographic CS
    //! afterwards re-reads that same text latitude-first and the two
    //! horizontals swap. Both importers already set it first.
    QString inputCS() const;
    void setInputCS(const QString& cs);

    //! The coordinate system this fix is actually expressed in: its own inputCS,
    //! or `globalCS` when it declares none. Trimmed; empty when neither supplies
    //! one. Every consumer of a fix's coordinate has to answer this the same way
    //! — the survex export anchors it under this CS, so the domain check, grid
    //! convergence and auto-declination must all judge it under the same one.
    QString effectiveCS(const QString& globalCS) const;

    //! The coordinate, written out — the whole of what this class stores.
    //! Kept exactly as it was given, including when it doesn't parse.
    QString coordinate() const;

    //! Take \a text as this fix's coordinate and read the components back out
    //! of it. An elevation in \a text must spell out its unit: it is read in
    //! meters, never in the project's display units, so that what a fix means
    //! can't change with a setting (see cwCoordinateText::textToStore(), which
    //! is what the entry surfaces normalize through).
    void setCoordinate(const QString& text);

    //! What coordinate() amounts to. The components are 0 unless this is Valid.
    CoordinateState state() const;

    //! The three components, read out of coordinate(). <b>Writing one writes
    //! the coordinate back out</b> as all three numbers under inputCS()'s axis
    //! order — these setters exist for the callers that have numbers rather
    //! than a string, chiefly the svx and Walls importers, and a coordinate
    //! written that way always spells out an elevation, at 0 if it had none.
    //! Anything the string said that the three numbers don't is therefore lost,
    //! including an Unreadable coordinate's text.
    double easting() const;
    void setEasting(double v);

    double northing() const;
    void setNorthing(double v);

    double elevation() const;
    void setElevation(double v);

    //! Whether the coordinate spelled an elevation out at all. A two-component
    //! coordinate — the shape one copied off a map arrives in — says nothing
    //! about elevation, and says so persistently rather than claiming sea
    //! level. elevation() is 0 either way, which is what every consumer of it
    //! already gets today: survex's *fix takes three numbers, so this is a
    //! distinction for the diagnostics to draw, not for the solve — and
    //! <b>nothing draws it yet</b>. Only a typed coordinate can carry it: the
    //! component setters spell all three out, so an import has no way to say
    //! its fix had no vertical.
    bool hasElevation() const;

    double horizontalVariance() const;
    void setHorizontalVariance(double v);

    double verticalVariance() const;
    void setVerticalVariance(double v);

private:
    QSharedDataPointer<cwFixStationData> data;
};

Q_DECLARE_METATYPE(cwFixStation)

#endif // CWFIXSTATION_H
