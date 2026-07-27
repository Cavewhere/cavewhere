/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwFixStation.h"

//Qt includes
#include <QSharedData>

class cwFixStationData : public QSharedData
{
public:
    QUuid Id = QUuid::createUuid();
    QString StationName;
    QString InputCS;
    double Easting = 0.0;
    double Northing = 0.0;
    double Elevation = 0.0;
    double HorizontalVariance = 0.0;
    double VerticalVariance = 0.0;
    QString CoordinateText;
    cwCoordinateText::AxisOrder CoordinateTextAxisOrder = cwCoordinateText::EastingNorthing;

    //! The order only means anything alongside the text, so the two go together.
    //! Keeping "no stored text" a single state is what lets operator== compare
    //! the pair without two textless fixes coming out unequal.
    void clearCoordinateText()
    {
        CoordinateText.clear();
        CoordinateTextAxisOrder = cwCoordinateText::EastingNorthing;
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
bool cwFixStation::operator==(const cwFixStation& other) const
{
    if (data == other.data) {
        return true;
    }
    return data->Id == other.data->Id
            && data->StationName == other.data->StationName
            && data->InputCS == other.data->InputCS
            && data->Easting == other.data->Easting
            && data->Northing == other.data->Northing
            && data->Elevation == other.data->Elevation
            && data->HorizontalVariance == other.data->HorizontalVariance
            && data->VerticalVariance == other.data->VerticalVariance
            && data->CoordinateText == other.data->CoordinateText
            && data->CoordinateTextAxisOrder == other.data->CoordinateTextAxisOrder;
}

QUuid cwFixStation::id() const { return data->Id; }
void cwFixStation::setId(const QUuid& id) { data->Id = id; }

QString cwFixStation::stationName() const { return data->StationName; }
void cwFixStation::setStationName(const QString& name) { data->StationName = name; }

QString cwFixStation::inputCS() const { return data->InputCS; }
void cwFixStation::setInputCS(const QString& cs) { data->InputCS = cs; }

QString cwFixStation::effectiveCS(const QString& globalCS) const
{
    const QString own = data->InputCS.trimmed();
    return own.isEmpty() ? globalCS.trimmed() : own;
}

// The three component setters all drop the stored coordinate text: the moment a
// number is written by any path other than the coordinate field, the string the
// user typed no longer describes this fix. Clearing it here rather than at the
// call sites is what makes the invariant hold for call sites that don't exist
// yet — the survex and Walls importers write components directly, and so will
// whatever comes next.
double cwFixStation::easting() const { return data->Easting; }
void cwFixStation::setEasting(double v) { data->Easting = v; data->clearCoordinateText(); }

double cwFixStation::northing() const { return data->Northing; }
void cwFixStation::setNorthing(double v) { data->Northing = v; data->clearCoordinateText(); }

double cwFixStation::elevation() const { return data->Elevation; }
void cwFixStation::setElevation(double v) { data->Elevation = v; data->clearCoordinateText(); }

QString cwFixStation::coordinateText() const { return data->CoordinateText; }

cwCoordinateText::AxisOrder cwFixStation::coordinateTextAxisOrder() const
{
    return data->CoordinateTextAxisOrder;
}

void cwFixStation::setCoordinateText(const QString& text, cwCoordinateText::AxisOrder order)
{
    if (text.isEmpty()) {
        data->clearCoordinateText();
        return;
    }
    data->CoordinateText = text;
    data->CoordinateTextAxisOrder = order;
}

double cwFixStation::horizontalVariance() const { return data->HorizontalVariance; }
void cwFixStation::setHorizontalVariance(double v) { data->HorizontalVariance = v; }

double cwFixStation::verticalVariance() const { return data->VerticalVariance; }
void cwFixStation::setVerticalVariance(double v) { data->VerticalVariance = v; }
