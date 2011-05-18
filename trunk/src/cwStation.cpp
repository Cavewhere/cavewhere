//Our includes
#include "cwStation.h"

//Qt includes
#include <QDebug>

cwStation::cwStation() {
    Left = "";
    Right = "";
    Up = "";
    Down = "";
}

/**
  \brief Make a copy of the station

  This doesn't copy the parent
  */
cwStation::cwStation(const cwStation& station) :
    Name(station.Name),
    Left(station.Left),
    Right(station.Right),
    Up(station.Up),
    Down(station.Down),
    Position(station.Position)
{

}

cwStation::cwStation(QString name) :
    Name(name),
    Left(""),
    Right(""),
    Up(""),
    Down("")
{
}


cwStation::cwStation(QString name, float left, float right, float up, float down) :
        Name(name),
        Left(QString("%1").arg(left)),
        Right(QString("%1").arg(right)),
        Up(QString("%1").arg(up)),
        Down(QString("%1").arg(down))
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
