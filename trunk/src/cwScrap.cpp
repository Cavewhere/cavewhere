//Our includes
#include "cwScrap.h"
#include "cwDebug.h"

//Qt includes
#include <QDebug>

cwScrap::cwScrap(QObject *parent) :
    QObject(parent)
{

}



/**
  \brief Inserts a point in scrap
  */
void cwScrap::insertPoint(int index, QPointF point) {
    if(index < 0 || index > Points.size()) {
        qDebug() << "Inserting point out of bounds:" << point << index << LOCATION;
    }

    Points.insert(index, point);
    emit insertedPoints(index, index);
}

/**
  \brief Inserts a point in scrap
  */
void cwScrap::removePoint(int index) {
    if(index < 0 || index >= Points.size()) {
        qDebug() << "Removing point out of bounds:" << index << LOCATION;
        return;
    }

    Points.remove(index);
    emit removedPoints(index, index);
}

