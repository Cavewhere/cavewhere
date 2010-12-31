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
    Down(station.Down)
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
        Left(left),
        Right(right),
        Up(up),
        Down(down)
{
}

void cwStation::SetName(QString name) {
    if(name != Name) {
        Name = name;
        emit NameChanged();
    }
 }

void cwStation::SetLeft(QVariant left) {
    if(left != Left) {
        Left = left;
        emit LeftChanged();
    }
 }

 void cwStation::SetRight(QVariant right) {
     if(Right != right) {
         Right = right;
         emit RightChanged();
     }
 }

 void cwStation::SetUp(QVariant up) {
     if(Up != up) {
         Up = up;
         emit UpChanged();
     }
 }

 void cwStation::SetDown(QVariant down) {
     if(Down != down) {
         Down = down;
         emit DownChanged();
     }
 }
