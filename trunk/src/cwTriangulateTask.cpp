//Our includes
#include "cwTriangulateTask.h"
#include "cwDebug.h"

//Triangle includes
#include "triangle/triangle.h"

//Qt includes
#include <QDebug>

cwTriangulateTask::cwTriangulateTask(QObject *parent) :
    cwTask(parent)
{
}

//Input so the triangle task
void cwTriangulateTask::setPolygon(QPolygonF polygon) {
    if(isReady()) {
        Polygon = polygon;
    } else {
        qDebug() << "Can't set the polygon while the task is still running" << LOCATION;
    }
}

/**
  \brief Gets all the triangle points
  */
QVector<QVector3D> cwTriangulateTask::trianglePoints() const {
    if(!isReady()) {
        qDebug() << "Can't get triangle points when triangulate task is still running" << LOCATION;
        return QVector<QVector3D>();
    }
    return TrianglePoints;
}

/**
  \brief Gets all the triangle indices
  */
QVector<int> cwTriangulateTask::triangleIndices() const {
    if(!isReady()) {
        qDebug() << "Can't get triangle indices when triangulate task is still running" << LOCATION;
        return QVector<int>();
    }
    return TriangleIndices;

}

/**
  \brief Does the triangulation
  */
void cwTriangulateTask::runTask() {

    triangulateio input;
    triangulateio output;






}
