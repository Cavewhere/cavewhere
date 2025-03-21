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
// #include "cwReadingStates.h"
// #include "cwDistanceValidator.h"
#include "cwGlobals.h"
#include "cwDistanceReading.h"

class CAVEWHERE_LIB_EXPORT cwStation {

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

    cwDistanceReading left() const;
    void setLeft(const cwDistanceReading& left) { Data->m_left = left; }

    cwDistanceReading right() const;
    void setRight(const cwDistanceReading& right) { Data->m_right = right; }

    cwDistanceReading up() const;
    void setUp(const cwDistanceReading& up) { Data->m_up = up; }

    cwDistanceReading down() const;
    void setDown(const cwDistanceReading& down) { Data->m_down = down; }

    // cwDistanceStates::State leftInputState() const;
    // cwDistanceStates::State rightInputState() const;
    // cwDistanceStates::State upInputState() const;
    // cwDistanceStates::State downInputState() const;

    void setData(QVariant data, DataRoles role);
    QVariant data(DataRoles role) const;

    bool setName(QString Name);

    // bool setLeft(QString left);
    // bool setLeft(double left);
    // void setLeftInputState(cwDistanceStates::State state);

    // bool setRight(QString right);
    // bool setRight(double right);
    // void setRightInputState(cwDistanceStates::State state);

    // bool setUp(QString up);
    // bool setUp(double up);
    // void setUpInputState(cwDistanceStates::State state);

    // bool setDown(QString down);
    // bool setDown(double down);
    // void setDownInputState(cwDistanceStates::State state);

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

        cwDistanceReading m_left;
        cwDistanceReading m_right;
        cwDistanceReading m_up;
        cwDistanceReading m_down;


        // cwDistanceStates::State LeftState;
        // cwDistanceStates::State RightState;
        // cwDistanceStates::State UpState;
        // cwDistanceStates::State DownState;

        // double Left;
        // double Right;
        // double Up;
        // double Down;

//        QVector3D Position;
    };

    QSharedDataPointer<PrivateData> Data;

    // bool checkLRUDValue(double value) const;
    // bool setStringValue(double& setValue, cwDistanceStates::State& state, QString value);
    // bool setDoubleValue(double& setValue, cwDistanceStates::State& state, double value);
    // void setPrivateLRUDState(cwDistanceStates::State &memberState, cwDistanceStates::State newState);
};

inline size_t qHash(const cwStation& station) {
    return qHash(station.name().toLower());
}

inline QString cwStation::name() const { return Data->Name; }
inline cwDistanceReading cwStation::left() const { return Data->m_left; }
inline cwDistanceReading cwStation::right() const { return Data->m_right; }
inline cwDistanceReading cwStation::up() const { return Data->m_up; }
inline cwDistanceReading cwStation::down() const { return Data->m_down; }

// inline cwDistanceStates::State cwStation::leftInputState() const {
//     return Data->LeftState;
// }

// inline cwDistanceStates::State cwStation::rightInputState() const {
//     return Data->RightState;
// }

// inline cwDistanceStates::State cwStation::upInputState() const {
//     return Data->UpState;
// }

// inline cwDistanceStates::State cwStation::downInputState() const {
//     return Data->DownState;
// }

//inline QVector3D cwStation::position() const { return Data->Position; }


//inline void cwStation::setPosition(QVector3D position) {
//    Data->Position = position;
//}

// inline bool cwStation::checkLRUDValue(double value) const
// {
//     return cwDistanceValidator::check(value);
// }

inline bool cwStation::operator ==(const cwStation &station) const
{
    return station.name().compare(station.name()) == 0;
}

inline bool cwStation::operator !=(const cwStation &station) const
{
    return ! operator ==(station);
}

inline QDebug operator<<(QDebug debug, const cwStation &station)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << station.name();
    return debug;
}



#endif // cwStation_H
