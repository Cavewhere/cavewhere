/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCOORDINATETEXT_H
#define CWCOORDINATETEXT_H

//Qt includes
#include <QObject>
#include <QQmlEngine>
#include <QString>

//Our includes
#include "Monad/Result.h"
#include "cwGlobals.h"
#include "cwUnits.h"
#include "cwValidator.h"

/**
 * Reads and writes a whole fix-station coordinate as one string, so a coordinate
 * copied from somewhere else can be pasted in one go (#621):
 *
 *     lat/long    46.12113, -115.59902, 304m
 *                 46.12113, -115.59902, 304ft
 *                 46.12113, -115.59902, 304          (the project's units)
 *                 46.12113, -115.59902               (no elevation)
 *
 *     UTM         610016.792, 5615117.075, 2545.340 feet
 *
 * The first two components are not always the same axis. A geographic CS
 * is written the way people write it and the way #621 asks for it —
 * <i>latitude first</i> — while a projected one is easting first. The two can't
 * be told apart from the numbers (46.12113, -115.59902 is a legal, if absurd,
 * UTM pair), so the caller states which it is via AxisOrder, resolved from the
 * row's coordinate system by axisOrderFor(). parse() and format() must always
 * be handed the same order, or a value round-tripping through the field would
 * come back transposed.
 *
 * Both coordinate-entry surfaces — the FixStationPage rows and the inline
 * FixStationPopup — go through this one class, so the accepted formats can't
 * drift apart. QML reaches format() off the singleton; parse() is reached
 * through cwFixStationModel::setCoordinateText(), the surface that refuses a
 * coordinate it can't read, through cwFixStation, which reads its components
 * back out of a coordinate it has already accepted, and through
 * cwCoordinateTextValidator below.
 *
 * The elevation is meters, always. Nothing downstream of cwFixStation
 * carries a unit: cwGeoPoint has none, and survex reads a *fix's three numbers
 * raw and hands them straight to proj_trans with no *units scaling
 * (survex/src/commands.c). Every CS CaveWhere offers is 2D, so PROJ passes z
 * through untouched, and survex's own internal length unit is metres — a fix's
 * elevation therefore has to be metres to line up with the survey it anchors.
 * A "ft" suffix is converted here, at parse time, and never stored. The
 * horizontal components are in their coordinate system's own units, which this
 * class has no way to know, so they cannot carry a unit suffix at all.
 *
 * A bare elevation is read in the project's unit system (Metric ⇒ meters,
 * Imperial ⇒ feet), which is what #621 asks for. format() therefore writes the
 * unit suffix in that same system: the single field is both the display and the
 * input, so an untouched value that round-trips through it has to come back
 * unchanged.
 */
class CAVEWHERE_LIB_EXPORT cwCoordinateText : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CoordinateText)
    QML_SINGLETON

public:
    //! Which axis the text writes first. Never inferred from the numbers — see
    //! the class note.
    enum AxisOrder {
        //! "easting, northing, elevation" — a projected CS, and the fallback
        //! for a CS that can't be resolved.
        EastingNorthing,
        //! "latitude, longitude, elevation" — a geographic CS, written the way
        //! people write it rather than in the model's easting/northing order.
        LatitudeLongitude
    };
    Q_ENUM(AxisOrder)

    struct Coordinate {
        double easting = 0.0;
        double northing = 0.0;
        //! Meters — see the class note. Only meaningful when hasElevation.
        double elevation = 0.0;
        //! Whether the text carried a third component at all. A two-component
        //! paste — the form a coordinate copied from a map most often takes —
        //! says nothing about elevation, so a caller must leave the one it has
        //! alone rather than reading this zero as an instruction.
        bool hasElevation = false;
        //! Whether that third component spelled out its own unit. A bare
        //! elevation means "in the project's units", which is a *different*
        //! coordinate once the project's unit system changes — see textToStore().
        bool hasElevationUnit = false;
    };

    explicit cwCoordinateText(QObject* parent = nullptr) : QObject(parent) {}

    //! How a coordinate in \a cs is written out. Geographic ⇒ latitude first;
    //! anything else, including an empty or unresolvable \a cs, is
    //! EastingNorthing.
    //!
    //! An empty \a cs is not a coordinate system, and the answer here is not
    //! a reading of one. A fix that declares none has no axis order at all
    //! (cwFixStation::NoSystem) and derives nothing from its text; the fallback
    //! exists only so that a caller writing three numbers into a CS-less row
    //! spells them out in some fixed order rather than none. Which order that is
    //! goes unrecorded, which is why naming a geographic system afterwards
    //! transposes the row — see swapHorizontal().
    Q_INVOKABLE static AxisOrder axisOrderFor(const QString& cs);

    //! The coordinate \a text spells out, or the reason it couldn't be read.
    //! Two components leave the elevation at 0. \a units resolves a bare
    //! elevation's unit and nothing else; \a order says which axis comes first.
    static Monad::Result<Coordinate> parse(const QString& text,
                                           cwUnits::UnitSystem units,
                                           AxisOrder order);

    //! \a easting, \a northing and \a elevationInMeters as one string, with the
    //! elevation expressed in \a units. Given the same \a units and \a order,
    //! parse() reads the horizontals back bit-for-bit; the <i>elevation</i>
    //! crosses a unit conversion in each direction, and m→ft→m is not an
    //! identity in IEEE arithmetic, so an imperial round trip can land one ulp
    //! away. Nothing stores that round trip: an editor opens on the coordinate
    //! as it was written, and cwFixStation writes and reads its own in meters.
    Q_INVOKABLE static QString format(double easting,
                                      double northing,
                                      double elevationInMeters,
                                      cwUnits::UnitSystem units,
                                      AxisOrder order);

    //! \a text with its first two numbers exchanged and everything else left
    //! exactly as written — the separators, the elevation, its unit, and the
    //! absence of an elevation. Empty when \a text doesn't read as a
    //! coordinate, which is not the same as holding fewer than two numbers:
    //! "N 46 07 16 W 115 35 56" holds six and is not a coordinate.
    //!
    //! Textual on purpose, so it takes neither an axis order nor a unit system:
    //! which axis a number <i>is</i> has no bearing on moving it, applying this
    //! twice returns the original string, and a coordinate whose text is all it
    //! has keeps that text's own shape. It exists for the one case nothing else
    //! can recover — a coordinate stored under no coordinate system, whose axis
    //! order was never written down (cwFixStation::NoSystem). Naming a
    //! geographic system on such a row reads it latitude-first whatever it was
    //! written as, and only the user knows which was meant.
    Q_INVOKABLE static QString swapHorizontal(const QString& text);

    //! The form of \a text to keep as what the user typed (U14): trimmed, and
    //! with the elevation's unit spelled out when it was written bare.
    //! \a coordinate is what parse() made of \a text.
    //!
    //! Appending the unit is the only edit this makes to the user's own words,
    //! and it exists because a bare elevation doesn't survive a change of
    //! project units: "46.1, -115.6, 304" stored verbatim in a metric project
    //! re-reads as 304 ft once the project goes imperial, moving the fix 213 m
    //! the next time the string is accepted unchanged. Nothing else about the
    //! text can change meaning that way — the horizontal components carry their
    //! coordinate system's own units, which no setting here can flip.
    static QString textToStore(const QString& text,
                               const Coordinate& coordinate,
                               cwUnits::UnitSystem units);

    //! The unit a bare elevation is read in, and the one format() writes.
    static cwUnits::LengthUnit elevationUnit(cwUnits::UnitSystem units)
    {
        return cwUnits::surveyUnit(units);
    }
};

/**
 * Installable validator for a free-form coordinate field.
 *
 * Never returns Invalid. CoreClickTextInput hands its validator to the
 * live text input, where Invalid rejects the keystroke — and every coordinate
 * is half-typed on its way to being whole, so blocking keystrokes is the wrong
 * mechanism for this field. Incomplete text is Intermediate instead, which
 * CoreClickTextInput::commitChanges() refuses to commit: it holds the editor
 * open, keeps what the user typed, and shows errorText. That is the "validator"
 * #621 asks for — one that judges on commit.
 *
 * FixStationPopup deliberately installs none of this. QC.TextField withholds
 * editingFinished while a validator reports unacceptable input, which would
 * leave the popup silent instead of showing the reason, so it validates by
 * committing through cwFixStationModel::setCoordinateText() and displaying what
 * comes back.
 */
class CAVEWHERE_LIB_EXPORT cwCoordinateTextValidator : public cwValidator
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CoordinateTextValidator)
    Q_PROPERTY(cwCoordinateText::AxisOrder axisOrder READ axisOrder WRITE setAxisOrder NOTIFY axisOrderChanged)

public:
    explicit cwCoordinateTextValidator(QObject* parent = nullptr);

    cwCoordinateText::AxisOrder axisOrder() const { return m_axisOrder; }
    void setAxisOrder(cwCoordinateText::AxisOrder order);

    //! The verdict is genuinely order-independent — which axis comes first
    //! decides what the numbers *mean*, never whether they can be read — but
    //! the <i>message</i> is not: it names the axes and picks the worked
    //! example. A row whose CS is geographic must be told about a latitude and
    //! a longitude, so each field sets axisOrder from its own row's CS.
    State validate(QString& input, int& position) const override;
    Q_INVOKABLE int validate(QString input) const override;

signals:
    void axisOrderChanged();

private:
    cwCoordinateText::AxisOrder m_axisOrder = cwCoordinateText::EastingNorthing;
};

#endif // CWCOORDINATETEXT_H
