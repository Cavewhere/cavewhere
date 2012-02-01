#ifndef cwStation_H
#define cwStation_H

//Qt includes
#include <QVector3D>
#include <QVariant>

//Our includes
#include "cwReadingStates.h"

class cwStation {

public:
    enum DataRoles {
        NameRole,
        LeftRole,
        RightRole,
        UpRole,
        DownRole,
        PositionRole
    };

    cwStation();
    cwStation(const cwStation& station);
    cwStation(QString name);
    cwStation(QString name, float left, float right, float up, float down);

    QString name() const;
    double left() const;
    double right() const;
    double up() const;
    double down() const;

    cwDistanceStates::State leftInputState() const;
    cwDistanceStates::State rightInputState() const;
    cwDistanceStates::State upInputState() const;
    cwDistanceStates::State downInputState() const;

    QVector3D position() const;

    void setData(QVariant data, DataRoles role);
    QVariant data(DataRoles role) const;

    bool isValid() const { return !Name.isEmpty(); }

public:
    bool setName(QString Name);

    bool setLeft(QString left);
    bool setLeft(double left);

    bool setRight(QString right);
    bool setRight(double right);

    bool setUp(QString up);
    bool setUp(double up);

    bool setDown(QString down);
    bool setDown(double down);

    void setPosition(QVector3D position); //This is set by the loop closer

protected:
    QString Name;

    cwDistanceStates::State LeftState;
    cwDistanceStates::State RightState;
    cwDistanceStates::State UpState;
    cwDistanceStates::State DownState;

    double Left;
    double Right;
    double Up;
    double Down;

    QVector3D Position;

    bool checkLRUDValue(double value) const;
    bool setStringValue(double& setValue, cwDistanceStates::State& state, QString value);
    bool setDoubleValue(double& setValue, cwDistanceStates::State& state, double value);
};

inline QString cwStation::name() const { return Name; }
inline double cwStation::left() const { return Left; }
inline double cwStation::right() const { return Right; }
inline double cwStation::up() const { return Up; }
inline double cwStation::down() const { return Down; }

inline cwDistanceStates::State cwStation::leftInputState() const {
    return LeftState;
}

inline cwDistanceStates::State cwStation::rightInputState() const {
    return RightState;
}

inline cwDistanceStates::State cwStation::upInputState() const {
    return DownState;
}

inline cwDistanceStates::State cwStation::downInputState() const {
    return UpState;
}

inline QVector3D cwStation::position() const { return Position; }


inline void cwStation::setPosition(QVector3D position) {
    Position = position;
}

inline bool cwStation::checkLRUDValue(double value) const
{
    return value >= 0.0;
}



#endif // cwStation_H
