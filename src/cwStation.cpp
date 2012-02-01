//Our includes
#include "cwStation.h"
#include "cwStationValidator.h"

//Qt includes
#include <QDebug>

cwStation::cwStation() :
    LeftState(cwDistanceStates::Empty),
    RightState(cwDistanceStates::Empty),
    UpState(cwDistanceStates::Empty),
    DownState(cwDistanceStates::Empty),
    Left(0.0),
    Right(0.0),
    Up(0.0),
    Down(0.0)

{ }

cwStation::cwStation(QString name) :
    Name(name),
    LeftState(cwDistanceStates::Empty),
    RightState(cwDistanceStates::Empty),
    UpState(cwDistanceStates::Empty),
    DownState(cwDistanceStates::Empty),
    Left(0.0),
    Right(0.0),
    Up(0.0),
    Down(0.0)
{

}


cwStation::cwStation(QString name, float left, float right, float up, float down) :
        Name(name)
{
    setLeft(QString("%1").arg(left));
    setRight(QString("%1").arg(right));
    setUp(QString("%1").arg(up));
    setDown(QString("%1").arg(down));
}



/**
  \brief Sets the data in the station
  */
void cwStation::setData(QVariant data, DataRoles role) {
    switch(role) {
    case NameRole:
        setName(data.toString());
        break;
    case LeftRole:
        setLeft(data.toString());
        break;
    case RightRole:
        setRight(data.toString());
        break;
    case UpRole:
        setUp(data.toString());
        break;
    case DownRole:
        setDown(data.toString());
        break;
    case PositionRole:
        setPosition(data.value<QVector3D>());
        break;
    }
}

/**
  \brief Gets the data in the station
  */
QVariant cwStation::data(DataRoles role) const {
    switch(role) {
    case NameRole:
        return name();
    case LeftRole:
        return left();
    case RightRole:
        return right();
    case UpRole:
        return up();
    case DownRole:
        return down();
    case PositionRole:
        return position();
    }
    return QVariant();
}

/**
    Must be a valid length
  */
bool cwStation::setLeft(QString left) {
    return setStringValue(Left, LeftState, left);
}

bool cwStation::setLeft(double left)
{
    return setDoubleValue(Left, LeftState, left);
}

/**
    Must be a valid length, or empty
  */
bool cwStation::setRight(QString right) {
    return setStringValue(Right, RightState, right);
}

bool cwStation::setRight(double right)
{
    return setDoubleValue(Right, RightState, right);
}

/**
    Must be a valid length, or empty
  */
bool cwStation::setUp(QString up) {
    return setStringValue(Up, UpState, up);
}

bool cwStation::setUp(double up)
{
    return setDoubleValue(Up, UpState, up);
}

/**
    Must be a valid length, or empty
  */
bool cwStation::setDown(QString down) {
    return setStringValue(Down, DownState, down);
}

bool cwStation::setDown(double down) {
    return setDoubleValue(Down, DownState, down);
}

bool cwStation::setStringValue(double &setValue, cwDistanceStates::State &state, QString value) {
    if(value.isEmpty()) {
        state = cwDistanceStates::Empty;
        setValue = 0.0;
        return true;
    }

    bool okay;
    double doubleValue = value.toDouble(&okay);
    if(okay) {
        return setDoubleValue(setValue, state, doubleValue);
    }
    return false;
}

bool cwStation::setDoubleValue(double &setValue, cwDistanceStates::State &state, double value) {
    if(checkLRUDValue(value)) {
        setValue = value;
        state = cwDistanceStates::Valid;
        return true;
    }
    return false;
}

bool cwStation::setName(QString name) {
    cwStationValidator validator;
    if(QValidator::Acceptable == validator.validate(name)) {
        Name = name;
        return true;
    }
    return false;
}

