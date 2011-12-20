//Our includes
#include "cwTriangulateTask.h"
#include "cwCropImageTask.h"
#include "cwDebug.h"

//Qt includes
#include <QDebug>

cwTriangulateTask::cwTriangulateTask(QObject *parent) :
    cwTask(parent),
    CropTask(new cwCropImageTask(this))
{
    CropTask->setParentTask(this);

}

void cwTriangulateTask::setScrapData(QList<cwTriangulateInData> scraps) {
    if(isReady()) {
        Scraps = scraps;
    } else {
        qDebug() << "Can't set scraps while the task is still running" << LOCATION;
    }
}

void cwTriangulateTask::setProjectFilename(QString filename)
{
    if(isReady()) {
        ProjectFilename = filename;
    } else {
        qDebug() << "Can't set project filename while the task is still running" << LOCATION;
    }
}

QList<cwTriangulatedData> cwTriangulateTask::triangulatedScrapData() const {
    if(isReady()) {
        return TriangulatedScraps;
    } else {
        qDebug() << "Can't get scrap data because the task is still running" << LOCATION;
    }
    return QList<cwTriangulatedData>();
}

/**
  \brief Does the triangulation
  */
void cwTriangulateTask::runTask() {
    TriangulatedScraps.clear();
    TriangulatedScraps.reserve(Scraps.size());

    //Crop all the scrap data
    cropScraps();


    done();
}

/**
    This runs the cropping task on all the scraps
  */
void cwTriangulateTask::cropScraps() {
    foreach(cwTriangulateInData data, Scraps) {
        QRectF cropArea = data.outline().boundingRect();
        CropTask->setOriginal(data.noteImage());
        CropTask->setRectF(cropArea);
        CropTask->setDatabaseFilename(ProjectFilename);
        CropTask->start();

        cwTriangulatedData triangulatedData;
        triangulatedData.setCroppedImage(CropTask->croppedImage());
        TriangulatedScraps.append(triangulatedData);
    }
}
