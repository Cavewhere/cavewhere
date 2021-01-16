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
    Q_GADGET

    Q_PROPERTY(QString latitude READ latitude WRITE setLatitude)
    Q_PROPERTY(QString longitude READ longitude WRITE setLongitude)
    Q_PROPERTY(QString altitude READ altitude WRITE setAltitude)
    Q_PROPERTY(cwUnits::LengthUnit altitudeUnit READ altitudeUnit WRITE setAltitudeUnit)
    Q_PROPERTY(QString stationName READ stationName WRITE setStationName)

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

    Q_INVOKABLE QGeoCoordinate toGeoCoordinate() const;
    Q_INVOKABLE bool isValid() const;

    bool operator==(const cwFixedStation& other) const;
    bool operator!=(const cwFixedStation& other) const { return !operator==(other); }

private:
    QSharedDataPointer<cwFixedStationData> data;
};

Q_DECLARE_METATYPE(cwFixedStation)

#endif // CWFIXEDSTATION_H
