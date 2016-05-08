/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef cwStation_H
#define cwStation_H

//Qt includes
//#include <QVector3D>
#include <QSharedDataPointer>

//Our includes
#include "cwReadingStates.h"
#include "cwDistanceValidator.h"
#include "cwGlobals.h"
#include "cwStringDouble.h"
#include "cwLengthInput.h"

class CAVEWHERE_LIB_EXPORT cwStation {

public:
    enum DataRoles {
        NameRole,
        LeftRole,
        RightRole,
        UpRole,
        DownRole
    };

    cwStation();
    cwStation(const cwStation& station);
    cwStation(QString name);

    QString name() const;
    cwLengthInput left() const;
    cwLengthInput right() const;
    cwLengthInput up() const;
    cwLengthInput down() const;

    void setData(QVariant data, DataRoles role);
    QVariant data(DataRoles role) const;

    bool setName(QString Name);

    bool setLeft(cwLengthInput left);
    bool setRight(cwLengthInput right);
    bool setUp(cwLengthInput up);
    bool setDown(cwLengthInput down);

    bool isValid() const { return !Data->Name.isEmpty(); }

    bool operator ==(const cwStation& station) const;
    bool operator !=(const cwStation& station) const;

    static bool nameIsValid(QString stationName);

private:
    class PrivateData : public QSharedData {
    public:
        PrivateData();
        PrivateData(QString name);

        QString Name;

        cwLengthInput Left;
        cwLengthInput Right;
        cwLengthInput Up;
        cwLengthInput Down;
    };

    QSharedDataPointer<PrivateData> Data;
};

inline uint qHash(const cwStation& station) {
    return qHash(station.name().toLower());
}

inline QString cwStation::name() const { return Data->Name; }
inline cwLengthInput  cwStation::left() const { return Data->Left; }
inline cwLengthInput cwStation::right() const { return Data->Right; }
inline cwLengthInput cwStation::up() const { return Data->Up; }
inline cwLengthInput cwStation::down() const { return Data->Down; }

//inline QVector3D cwStation::position() const { return Data->Position; }


//inline void cwStation::setPosition(QVector3D position) {
//    Data->Position = position;
//}



inline bool cwStation::operator ==(const cwStation &station) const
{
    return station.name().compare(station.name()) == 0;
}

inline bool cwStation::operator !=(const cwStation &station) const
{
    return ! operator ==(station);
}


#endif // cwStation_H
