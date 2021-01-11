//Our includes
#include "cwFixedStation.h"
#include "cwUnits.h"
#include "cwGlobals.h"

class cwFixedStationData : public QSharedData
{
public:
    QString latitude;
    QString longitude;
    QString altitude;
    cwUnits::LengthUnit altitudeUnit = cwUnits::Meters;
    QString stationName;
};

cwFixedStation::cwFixedStation() : data(new cwFixedStationData)
{

}

cwFixedStation::cwFixedStation(const cwFixedStation &rhs) : data(rhs.data)
{

}

cwFixedStation &cwFixedStation::operator=(const cwFixedStation &rhs)
{
    if (this != &rhs)
        data.operator=(rhs.data);
    return *this;
}

cwFixedStation::~cwFixedStation()
{

}

QString cwFixedStation::latitude() const
{
    return data->latitude;
}

void cwFixedStation::setLatitude(const QString &latitude)
{
    data->latitude = latitude;
}

QString cwFixedStation::longitude() const
{
    return data->longitude;
}

void cwFixedStation::setLongitude(const QString &longitude)
{
    data->longitude = longitude;
}

QString cwFixedStation::altitude() const
{
    return data->altitude;
}

void cwFixedStation::setAltitude(const QString &altitude)
{
    data->altitude = altitude;
}

cwUnits::LengthUnit cwFixedStation::altitudeUnit() const
{
    return data->altitudeUnit;
}

void cwFixedStation::setAltitudeUnit(cwUnits::LengthUnit unit)
{
    data->altitudeUnit = unit;
}

QString cwFixedStation::stationName() const
{
    return data->stationName;
}

void cwFixedStation::setStationName(const QString &stationName)
{
    data->stationName = stationName;
}

QGeoCoordinate cwFixedStation::toGeoCoordinate() const
{
    bool okay;
    double longitude = data->longitude.toDouble(&okay);
    if(!okay) { return QGeoCoordinate(); }
    double latitude = data->latitude.toDouble(&okay);
    if(!okay) { return QGeoCoordinate(); }

    double altitude = data->altitude.toDouble(&okay);
    QGeoCoordinate coord = [&]() {
        if(okay) {
            auto altitudeMeter = cwUnits::convert(altitude, data->altitudeUnit, cwUnits::Meters);
            return QGeoCoordinate(latitude, longitude, altitudeMeter);
        }
        return QGeoCoordinate(latitude, longitude);
    }();

    return coord;
}

bool cwFixedStation::isValid() const
{
    auto coord = toGeoCoordinate();
    return !stationName().isEmpty() && coord.isValid();
}
