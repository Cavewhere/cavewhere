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
        //! repaired. Writing any component discards it.
        //!
        //! Both entry surfaces show the text rather than the zeros the row
        //! reports, tint it, and give the parser's own reason for refusing it
        //! (cwFixStationDiagnosticsModel::CoordinateErrorRole).
        Unreadable,
        //! There is text, but no inputCS() to read it under — so it has no axis
        //! order, and its numbers can't be said to mean anything. Behaviorally
        //! Unreadable (string kept, components 0) but for a different reason,
        //! and it wants a different message: <i>choose a coordinate system</i>,
        //! not <i>this text can't be read</i>. Rows created in the app start on
        //! WGS84, so this arrives from elsewhere: an older project, a hand edit,
        //! or an svx whose <tt>*fix</tt> had no <tt>*cs</tt> before it — a local
        //! grid, which is ordinary.
        //!
        //! The numbers are kept as text, so naming a <i>projected</i> system
        //! reads all three straight back. Naming a geographic one does not:
        //! nothing records which axis order the text was written in, and there
        //! was none to record — so a coordinate stored easting-first is read
        //! back latitude-first and the first two components transpose.
        //!
        //! There is no detecting that afterwards, so both entry surfaces ask
        //! before they let it happen: they show the reading they are about to
        //! commit to and offer the user's own text with its first two numbers
        //! exchanged (CoordinateOrderAskBox.qml, over
        //! cwFixStationDiagnosticsModel::CoordinateOrderUnknownRole and
        //! cwCoordinateText::swapHorizontal()).
        NoSystem
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

    //! The coordinate system this fix is expressed in — <b>its own, always</b>.
    //! There is no fallback to the region's global CS: a row that declares none
    //! is an error (state() == NoSystem), not one that quietly follows the
    //! project, so changing the project's projection never moves a station that
    //! was entered under some other system.
    //!
    //! <b>Set this before any component.</b> It decides which axis the
    //! coordinate leads with, so writing a component while it is still empty
    //! spells the coordinate out easting-first; setting a geographic CS
    //! afterwards re-reads that same text latitude-first and the two
    //! horizontals swap. Both importers already set it first.
    QString inputCS() const;
    void setInputCS(const QString& cs);

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

    //! All three components at once, written out as one coordinate — <b>what a
    //! caller holding three numbers should use</b>. The one-at-a-time setters
    //! below each write the coordinate and read it straight back, so calling
    //! them in sequence on a fix with no inputCS() loses the earlier two: there
    //! is no axis order to read them back under, so each write returns nothing
    //! and the next spells that nothing out. This writes once, and the numbers
    //! survive as text on a row that says it has no system.
    void setCoordinate(double easting, double northing, double elevation);

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
