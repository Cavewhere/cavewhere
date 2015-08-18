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

class cwStation {

public:
    enum DataRoles {
        NameRole,
        LeftRole,
        RightRole,
        UpRole,
        DownRole
//        PositionRole
    };

    cwStation();
    cwStation(const cwStation& station);
    cwStation(QString name);

    QString name() const;
    double left() const;
    double right() const;
    double up() const;
    double down() const;

    cwDistanceStates::State leftInputState() const;
    cwDistanceStates::State rightInputState() const;
    cwDistanceStates::State upInputState() const;
    cwDistanceStates::State downInputState() const;

    void setData(QVariant data, DataRoles role);
    QVariant data(DataRoles role) const;

    bool setName(QString Name);

    bool setLeft(QString left);
    bool setLeft(double left);
    void setLeftInputState(cwDistanceStates::State state);

    bool setRight(QString right);
    bool setRight(double right);
    void setRightInputState(cwDistanceStates::State state);

    bool setUp(QString up);
    bool setUp(double up);
    void setUpInputState(cwDistanceStates::State state);

    bool setDown(QString down);
    bool setDown(double down);
    void setDownInputState(cwDistanceStates::State state);

//    QVector3D position() const;
//    void setPosition(QVector3D position); //This is set by the loop closer

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

        cwDistanceStates::State LeftState;
        cwDistanceStates::State RightState;
        cwDistanceStates::State UpState;
        cwDistanceStates::State DownState;

        double Left;
        double Right;
        double Up;
        double Down;
    };

    QSharedDataPointer<PrivateData> Data;

    bool checkLRUDValue(double value) const;
    bool setStringValue(double& setValue, cwDistanceStates::State& state, QString value);
    bool setDoubleValue(double& setValue, cwDistanceStates::State& state, double value);
    void setPrivateLRUDState(cwDistanceStates::State &memberState, cwDistanceStates::State newState);
};

inline uint qHash(const cwStation& station) {
    return qHash(station.name().toLower());
}

inline QString cwStation::name() const { return Data->Name; }
inline double cwStation::left() const { return Data->Left; }
inline double cwStation::right() const { return Data->Right; }
inline double cwStation::up() const { return Data->Up; }
inline double cwStation::down() const { return Data->Down; }

inline cwDistanceStates::State cwStation::leftInputState() const {
    return Data->LeftState;
}

inline cwDistanceStates::State cwStation::rightInputState() const {
    return Data->RightState;
}

inline cwDistanceStates::State cwStation::upInputState() const {
    return Data->UpState;
}

inline cwDistanceStates::State cwStation::downInputState() const {
    return Data->DownState;
}

//inline QVector3D cwStation::position() const { return Data->Position; }


//inline void cwStation::setPosition(QVector3D position) {
//    Data->Position = position;
//}

inline bool cwStation::checkLRUDValue(double value) const
{
    return cwDistanceValidator::check(value);
}


inline bool cwStation::operator ==(const cwStation &station) const
{
    return station.name().compare(station.name()) == 0;
}

inline bool cwStation::operator !=(const cwStation &station) const
{
    return ! operator ==(station);
}


#endif // cwStation_H
