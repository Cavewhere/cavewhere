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
cwStation::cwStation(const cwStation& station) : QObject(),
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
    if(name != Name) {
        Name = name;
        emit nameChanged();
    }
 }

void cwStation::setLeft(QString left) {
    if(left != Left) {
        Left = left;
        emit leftChanged();
    }
 }

 void cwStation::setRight(QString right) {
     if(Right != right) {
         Right = right;
         emit rightChanged();
     }
 }

 void cwStation::setUp(QString up) {
     if(Up != up) {
         Up = up;
         emit upChanged();
     }
 }

 void cwStation::setDown(QString down) {
     if(Down != down) {
         Down = down;
         emit downChanged();
     }
 }

 void cwStation::setPosition(QVector3D position) {
     if(Position != position) {
         Position = position;
         emit positionChanged();
     }
 }
