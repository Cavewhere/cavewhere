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
            && data->VerticalVariance == other.data->VerticalVariance;
}

QUuid cwFixStation::id() const { return data->Id; }
void cwFixStation::setId(const QUuid& id) { data->Id = id; }

QString cwFixStation::stationName() const { return data->StationName; }
void cwFixStation::setStationName(const QString& name) { data->StationName = name; }

QString cwFixStation::inputCS() const { return data->InputCS; }
void cwFixStation::setInputCS(const QString& cs) { data->InputCS = cs; }

double cwFixStation::easting() const { return data->Easting; }
void cwFixStation::setEasting(double v) { data->Easting = v; }

double cwFixStation::northing() const { return data->Northing; }
void cwFixStation::setNorthing(double v) { data->Northing = v; }

double cwFixStation::elevation() const { return data->Elevation; }
void cwFixStation::setElevation(double v) { data->Elevation = v; }

double cwFixStation::horizontalVariance() const { return data->HorizontalVariance; }
void cwFixStation::setHorizontalVariance(double v) { data->HorizontalVariance = v; }

double cwFixStation::verticalVariance() const { return data->VerticalVariance; }
void cwFixStation::setVerticalVariance(double v) { data->VerticalVariance = v; }
