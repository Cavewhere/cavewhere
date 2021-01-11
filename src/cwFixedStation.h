#ifndef CWFIXEDSTATION_H
#define CWFIXEDSTATION_H

//Qt includes
#include <QSharedDataPointer>
#include <QGeoCoordinate>

//Our includes
#include "cwUnits.h"
#include "cwGlobals.h"

class cwFixedStationData;

class CAVEWHERE_LIB_EXPORT cwFixedStation
{
public:
    cwFixedStation();
    cwFixedStation(const cwFixedStation &);
    cwFixedStation &operator=(const cwFixedStation &);
    ~cwFixedStation();

    QString latitude() const;
    void setLatitude(const QString& latitude);

    QString longitude() const;
    void setLongitude(const QString& longitude);

    QString altitude() const;
    void setAltitude(const QString& altitude);

    cwUnits::LengthUnit altitudeUnit() const;
    void setAltitudeUnit(cwUnits::LengthUnit unit);

    QString stationName() const;
    void setStationName(const QString& stationName);

    QGeoCoordinate toGeoCoordinate() const;

    bool isValid() const;

private:
    QSharedDataPointer<cwFixedStationData> data;
};

#endif // CWFIXEDSTATION_H
