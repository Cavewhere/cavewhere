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
#include "cwCoordinateText.h"
#include "cwGlobals.h"

class cwFixStationData;

/**
 * Value class anchoring a named station to absolute coordinates in some
 * input coordinate system. Lives inside cwFixStationModel; serialized as
 * part of cwCave.
 *
 * Copyable / equality-comparable; uses QSharedDataPointer for COW so cheap
 * to pass through QVariant and signals.
 */
class CAVEWHERE_LIB_EXPORT cwFixStation
{
public:
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

    QString inputCS() const;
    void setInputCS(const QString& cs);

    //! The coordinate system this fix is actually expressed in: its own inputCS,
    //! or `globalCS` when it declares none. Trimmed; empty when neither supplies
    //! one. Every consumer of a fix's coordinate has to answer this the same way
    //! — the survex export anchors it under this CS, so the domain check, grid
    //! convergence and auto-declination must all judge it under the same one.
    QString effectiveCS(const QString& globalCS) const;

    //! The three components. <b>Writing any of them drops coordinateText()</b>
    //! — a number written by some path other than the coordinate field makes
    //! the stored string a lie about this fix, and the row is better off
    //! rendering from its numbers than offering an editor a string that no
    //! longer describes it. Set the text last when writing both.
    double easting() const;
    void setEasting(double v);

    double northing() const;
    void setNorthing(double v);

    double elevation() const;
    void setElevation(double v);

    //! What the user typed this coordinate as, empty when nobody typed it here
    //! — a fix imported from svx/Compass/Walls, loaded from a project written
    //! before this field existed, or one whose numbers were set directly. The
    //! numbers above stay authoritative for everything downstream; this is
    //! display state, so that an *editor* can re-offer the user their own
    //! string rather than a machine rendering of it (U14, #621).
    QString coordinateText() const;

    //! Which axis coordinateText() leads with. Kept beside the text because the
    //! row's CS can change after the text was typed, and the same string under
    //! the other order is a different coordinate — not a no-op.
    cwCoordinateText::AxisOrder coordinateTextAxisOrder() const;

    //! Keep \a text, read under \a order, as the string this coordinate was
    //! entered as. An empty \a text means the fix has none; the order goes back
    //! to the default with it, so "no stored text" is a single state.
    void setCoordinateText(const QString& text, cwCoordinateText::AxisOrder order);

    double horizontalVariance() const;
    void setHorizontalVariance(double v);

    double verticalVariance() const;
    void setVerticalVariance(double v);

private:
    QSharedDataPointer<cwFixStationData> data;
};

Q_DECLARE_METATYPE(cwFixStation)

#endif // CWFIXSTATION_H
