#ifndef CWTRIANGULATETASK_H
#define CWTRIANGULATETASK_H

//Our includes
#include "cwTask.h"
#include "cwTriangulateInData.h"
#include "cwTriangulatedData.h"
class cwCropImageTask;

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
    void setScrapData(QList<cwTriangulateInData> scraps);
    void setProjectFilename(QString filename);

    //Outputs of the task
    QList<cwTriangulatedData> triangulatedScrapData() const;

signals:
    
public slots:

protected:
    virtual void runTask();


private:
    //Inputs
    QList<cwTriangulateInData> Scraps;
    QString ProjectFilename;

    //Outputs
    QList<cwTriangulatedData> TriangulatedScraps;

    //Sub tasks
    cwCropImageTask* CropTask;

    void cropScraps();

};

#endif // CWTRIANGULATETASK_H
