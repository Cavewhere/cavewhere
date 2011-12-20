#ifndef CWTRIANGULATETASK_H
#define CWTRIANGULATETASK_H

//Our includes
#include "cwTask.h"
#include "cwTriangulateInData.h"
#include "cwTriangulatedData.h"
#include "cwImage.h"
#include "cwNoteTranformation.h"
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
    class PointGrid {
    public:
        QSize GridSize;
        QVector<QPointF> Points;
    };

    //Inputs
    QList<cwTriangulateInData> Scraps;
    QString ProjectFilename;

    //Outputs
    QList<cwTriangulatedData> TriangulatedScraps;

    //Sub tasks
    cwCropImageTask* CropTask;

    void cropScraps();

    void triangulateScraps();
    void triangulateScrap(int index);
    PointGrid createPointGrid(QRectF bounds, cwImage scrapImage, const cwNoteTranformation &noteTransform);

};

#endif // CWTRIANGULATETASK_H
