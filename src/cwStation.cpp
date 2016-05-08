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
    Data(new PrivateData())
{
    setName(name);
}

cwStation::PrivateData::PrivateData()
{

}

cwStation::PrivateData::PrivateData(QString name) :
    Name(name)
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
        return left().value();
    case RightRole:
        return right().value();
    case UpRole:
        return up().value();
    case DownRole:
        return down().value();
    }
    return QVariant();
}

bool cwStation::setLeft(cwLengthInput left)
{
    Data->Left = left;
    return true;
}

bool cwStation::setRight(cwLengthInput right)
{
    Data->Right = right;
    return true;
}

bool cwStation::setUp(cwLengthInput up)
{
    Data->Up = up;
    return true;
}

bool cwStation::setDown(cwLengthInput down) {
    Data->Down = down;
    return true;
}

/**
 * @brief cwStation::nameIsValid
 * @param stationName
 * @return Returns true if the station name is valid, and false if it isn't
 *
 * This function is useful for writing importers.
 */
bool cwStation::nameIsValid(QString stationName)
{
    cwStationValidator validator;
    return QValidator::Acceptable == validator.validate(stationName);
}

/**
 * @brief cwStation::setName
 * @param name
 * @return True if the name was set, and false if it wasn't.
 *
 * Make sure the nameIsValid() before set the name of the station.
 */
bool cwStation::setName(QString name) {
    if(nameIsValid(name)) {
        Data->Name = name;
        return true;
    }
    return false;
}

