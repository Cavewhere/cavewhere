/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwFixStation.h"

//Our includes
#include "cwCoordinateText.h"
#include "cwUnits.h"

//Qt includes
#include <QSharedData>

namespace {

//! The unit system a stored coordinate is read and written under — never the
//! project's. A stored elevation always spells its own unit out, so there is
//! nothing here for a unit system to resolve, and passing one would make what a
//! fix *means* depend on how the project happens to display it.
constexpr cwUnits::UnitSystem kStoredUnits = cwUnits::Metric;

}

class cwFixStationData : public QSharedData
{
public:
    QUuid Id = QUuid::createUuid();
    QString StationName;
    QString InputCS;
    double HorizontalVariance = 0.0;
    double VerticalVariance = 0.0;

    //! The coordinate, as it was written.
    QString Coordinate;

    //! Read out of Coordinate under InputCS's axis order. refresh() is their
    //! only writer, which is what keeps them a pure function of the two fields
    //! above rather than a second copy of the coordinate that could drift.
    double Easting = 0.0;
    double Northing = 0.0;
    double Elevation = 0.0;
    bool HasElevation = false;
    cwFixStation::CoordinateState State = cwFixStation::Empty;

    void refresh()
    {
        Easting = 0.0;
        Northing = 0.0;
        Elevation = 0.0;
        HasElevation = false;

        if (Coordinate.trimmed().isEmpty()) {
            State = cwFixStation::Empty;
            return;
        }

        // No system, no axis order — there is no reading of the text to make.
        // Checked before parse() rather than after, so an easting-first guess
        // never reaches the components and gets treated as a coordinate.
        if (InputCS.trimmed().isEmpty()) {
            State = cwFixStation::NoSystem;
            return;
        }

        const auto result = cwCoordinateText::parse(Coordinate, kStoredUnits,
                                                    cwCoordinateText::axisOrderFor(InputCS));
        if (result.hasError()) {
            State = cwFixStation::Unreadable;
            return;
        }

        const cwCoordinateText::Coordinate coordinate = result.value();
        Easting = coordinate.easting;
        Northing = coordinate.northing;
        HasElevation = coordinate.hasElevation;
        Elevation = coordinate.hasElevation ? coordinate.elevation : 0.0;
        State = cwFixStation::Valid;
    }

    //! write() for a caller that set one component at a time: the two it left
    //! in place, plus the one it just changed.
    void reformat() { write(Easting, Northing, Elevation); }

    //! refresh() the other way: three numbers written back out as the
    //! coordinate.
    //!
    //! It reads its own output back rather than asserting what it just wrote,
    //! so the derived fields stay a pure function of the coordinate even when
    //! format() produces something parse() won't take — a non-finite component
    //! renders as "inf", and a fix carrying one is honestly Unreadable rather
    //! than Valid with a string that disagrees with it. That read-back is also
    //! why a caller holding all three numbers has to come through here in one
    //! call: on a fix with no InputCS the read finds no axis order and returns
    //! nothing, so three reformat()s in a row would each spell out what the one
    //! before it lost.
    void write(double easting, double northing, double elevation)
    {
        Coordinate = cwCoordinateText::format(easting, northing, elevation, kStoredUnits,
                                              cwCoordinateText::axisOrderFor(InputCS));
        refresh();
    }
};

cwFixStation::cwFixStation() :
    data(new cwFixStationData)
{
}

cwFixStation::cwFixStation(const cwFixStation& other) :
    data(other.data)
{
}

cwFixStation& cwFixStation::operator=(const cwFixStation& other)
{
    if (this != &other) {
        data = other.data;
    }
    return *this;
}

cwFixStation::~cwFixStation() = default;

// Equality includes Id because it is part of the fix's persistent identity:
// two default-constructed cwFixStation values therefore compare unequal (each
// gets a fresh QUuid in its data block). cwFixStationModel::setData relies on
// per-field comparison rather than this operator for no-op detection.
//
// The components are left out deliberately. They are a pure function of the
// coordinate and the CS, both compared here, so adding them could only ever
// restate an answer already given — or, worse, make equality depend on
// refresh() having run.
bool cwFixStation::operator==(const cwFixStation& other) const
{
    if (data == other.data) {
        return true;
    }
    return data->Id == other.data->Id
            && data->StationName == other.data->StationName
            && data->InputCS == other.data->InputCS
            && data->Coordinate == other.data->Coordinate
            && data->HorizontalVariance == other.data->HorizontalVariance
            && data->VerticalVariance == other.data->VerticalVariance;
}

QUuid cwFixStation::id() const { return data->Id; }
void cwFixStation::setId(const QUuid& id) { data->Id = id; }

QString cwFixStation::stationName() const { return data->StationName; }
void cwFixStation::setStationName(const QString& name) { data->StationName = name; }

QString cwFixStation::inputCS() const { return data->InputCS; }

// Re-reads the coordinate: which axis it leads with comes from the CS, so the
// same text under a new one is a different coordinate. A user correcting a row
// that was pasted under the wrong system is telling us how to read what they
// already typed, not asking us to leave it read the old way.
void cwFixStation::setInputCS(const QString& cs)
{
    data->InputCS = cs;
    data->refresh();
}

QString cwFixStation::coordinate() const { return data->Coordinate; }

void cwFixStation::setCoordinate(const QString& text)
{
    data->Coordinate = text;
    data->refresh();
}

void cwFixStation::setCoordinate(double easting, double northing, double elevation)
{
    data->write(easting, northing, elevation);
}

cwFixStation::CoordinateState cwFixStation::state() const { return data->State; }

// Each component setter spells the coordinate back out, so the string stays the
// one thing this class stores. Doing it here rather than at the call sites is
// what makes it hold for the call sites that don't exist yet — the svx and
// Walls importers write components directly, and so will whatever comes next.
double cwFixStation::easting() const { return data->Easting; }
void cwFixStation::setEasting(double v) { data->Easting = v; data->reformat(); }

double cwFixStation::northing() const { return data->Northing; }
void cwFixStation::setNorthing(double v) { data->Northing = v; data->reformat(); }

double cwFixStation::elevation() const { return data->Elevation; }
void cwFixStation::setElevation(double v) { data->Elevation = v; data->reformat(); }

bool cwFixStation::hasElevation() const { return data->HasElevation; }

double cwFixStation::horizontalVariance() const { return data->HorizontalVariance; }
void cwFixStation::setHorizontalVariance(double v) { data->HorizontalVariance = v; }

double cwFixStation::verticalVariance() const { return data->VerticalVariance; }
void cwFixStation::setVerticalVariance(double v) { data->VerticalVariance = v; }
