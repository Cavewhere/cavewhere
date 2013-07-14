/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwStation.h"
#include "cwStationValidator.h"

//Qt includes
#include <QDebug>

cwStation::cwStation() :
    Data(new PrivateData())

{ }

cwStation::cwStation(const cwStation &station) :
    Data(station.Data)
{
}

cwStation::cwStation(QString name) :
    Data(new PrivateData(name))
{

}

cwStation::PrivateData::PrivateData() :
    LeftState(cwDistanceStates::Empty),
    RightState(cwDistanceStates::Empty),
    UpState(cwDistanceStates::Empty),
    DownState(cwDistanceStates::Empty),
    Left(0.0),
    Right(0.0),
    Up(0.0),
    Down(0.0) {

}

cwStation::PrivateData::PrivateData(QString name) :
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
//    case PositionRole:
//        setPosition(data.value<QVector3D>());
//        break;
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
//    case PositionRole:
//        return position();
    }
    return QVariant();
}

/**
    Must be a valid length
  */
bool cwStation::setLeft(QString left) {
    return setStringValue(Data->Left, Data->LeftState, left);
}

bool cwStation::setLeft(double left)
{
    return setDoubleValue(Data->Left, Data->LeftState, left);
}

void cwStation::setLeftInputState(cwDistanceStates::State state)
{
    setPrivateLRUDState(Data->LeftState, state);
}

/**
    Must be a valid length, or empty
  */
bool cwStation::setRight(QString right) {
    return setStringValue(Data->Right, Data->RightState, right);
}

bool cwStation::setRight(double right)
{
    return setDoubleValue(Data->Right, Data->RightState, right);
}

void cwStation::setRightInputState(cwDistanceStates::State state)
{
    setPrivateLRUDState(Data->RightState, state);
}

/**
    Must be a valid length, or empty
  */
bool cwStation::setUp(QString up) {
    return setStringValue(Data->Up, Data->UpState, up);
}

bool cwStation::setUp(double up)
{
    return setDoubleValue(Data->Up, Data->UpState, up);
}

void cwStation::setUpInputState(cwDistanceStates::State state)
{
    setPrivateLRUDState(Data->UpState, state);
}

/**
    Must be a valid length, or empty
  */
bool cwStation::setDown(QString down) {
    return setStringValue(Data->Down, Data->DownState, down);
}

bool cwStation::setDown(double down) {
    return setDoubleValue(Data->Down, Data->DownState, down);
}

void cwStation::setDownInputState(cwDistanceStates::State state)
{
    setPrivateLRUDState(Data->DownState, state);
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

/**
  Sets the LRUD State, this function uses a switch statement to verify the newState
  */
void cwStation::setPrivateLRUDState(cwDistanceStates::State &memberState, cwDistanceStates::State newState)
{
    //We need this switch statement because serialization class (or any other class) could send in interger
    switch(newState) {
    case cwDistanceStates::Empty:
        memberState = cwDistanceStates::Empty;
        break;
    case cwDistanceStates::Valid:
        memberState = cwDistanceStates::Valid;
        break;
    default:
        memberState = cwDistanceStates::Empty;
        break;
    }
}

bool cwStation::setName(QString name) {
    cwStationValidator validator;
    if(QValidator::Acceptable == validator.validate(name)) {
        Data->Name = name;
        return true;
    }
    return false;
}

