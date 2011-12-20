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

    //Triangulate scraps
    triangulateScraps();

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

/**
  \brief triangulate the scrap data
  */
void cwTriangulateTask::triangulateScraps() {
    //For each scrap
    for(int i = 0; i < Scraps.size(); i++) {
        triangulateScrap(i);
    }
}

/**
    \brief triangulate the scrap data
  */
void cwTriangulateTask::triangulateScrap(int index) {
    cwTriangulateInData& scrapData = Scraps[index];
    QRectF bounds = scrapData.outline().boundingRect();

    cwImage croppedImage = TriangulatedScraps[index].croppedImage();
    PointGrid pointGrid = createPointGrid(bounds, croppedImage, scrapData.noteTransform());



}

/**
    \brief Creates a point grid

    This is a regualar grid that has grid resolution of a meter.

    \param PointGridSize is the size in normalize note coordinates of the grid.
    \param scrapImage is used to get the original size and dotPerMeter

    This returns a regualar grid.
*/
cwTriangulateTask::PointGrid cwTriangulateTask::createPointGrid(QRectF bounds, cwImage scrapImage, const cwNoteTranformation &noteTransform) {
    PointGrid grid;

    QSize scrapImageSize = scrapImage.origianlSize();
    double sizeOnPaperX = scrapImageSize.width() / scrapImage.originalDotsPerMeter(); //in meters
    double sizeOnPaperY = scrapImageSize.height() / scrapImage.originalDotsPerMeter(); //in meters

    double pointsPerMeter = 1.0; //Grid resolution
    double scale = noteTransform.scale();
    grid.GridSize.setWidth((int)(sizeOnPaperX * scale * pointsPerMeter));
    grid.GridSize.setHeight((int)(sizeOnPaperY * scale * pointsPerMeter));
    grid.Points.resize(grid.GridSize.width() * grid.GridSize.height());

    double xDelta = bounds.width() / grid.GridSize.width(); //xDelta in normalized note coordinates
    double yDelta = bounds.height() / grid.GridSize.height();

    for(int y = 0; y < grid.GridSize.height(); y++) {
        for(int x = 0; x < grid.GridSize.width(); x++) {
            QPointF gridPoint = bounds.topLeft() + QPointF(x * xDelta, y * yDelta);
            int index = y * grid.GridSize.width() + x;
            grid.Points[index] = gridPoint;
        }
    }

    return grid;
}
