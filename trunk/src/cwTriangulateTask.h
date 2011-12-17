#ifndef CWTRIANGULATETASK_H
#define CWTRIANGULATETASK_H

//Our includes
#include "cwTask.h"

//Qt include
#include <QPolygonF>
#include <QVector>
#include <QVector3D>

class cwTriangulateTask : public cwTask
{
    Q_OBJECT
public:
    explicit cwTriangulateTask(QObject *parent = 0);
    
    //Input so the triangle task
    void setPolygon(QPolygonF polygon);

    //Outputs of the task
    QVector<QVector3D> triangles() const;

signals:
    
public slots:
    
};

#endif // CWTRIANGULATETASK_H
