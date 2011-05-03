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

void cwStation::setName(QString name) {
    Name = name;
}

void cwStation::setLeft(QString left) {
    Left = left;
}

void cwStation::setRight(QString right) {
    Right = right;
}

void cwStation::setUp(QString up) {
    Up = up;
}

void cwStation::setDown(QString down) {
    Down = down;
}

void cwStation::setPosition(QVector3D position) {
    Position = position;
}
