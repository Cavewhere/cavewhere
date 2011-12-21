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

    //Create the regualar mesh that covers the croppedImage
    PointGrid pointGrid = createPointGrid(bounds, croppedImage, scrapData.noteTransform());

    //Find all the points in the regualar mesh that are in the scrap's polygon
    QSet<int> gridPointsInScrap = pointsInPolygon(pointGrid, scrapData.outline());

    //Creates list of quads that are on the edges or in the scrap
    QuadDatabase quads = createQuads(pointGrid, gridPointsInScrap);

    qDebug() << "Quad database:" << quads.FullQuads.size() << quads.PartialQuads.size();
}

/**
    \brief Creates a point grid

    This is a regualar grid that has grid resolution of a meter.

    \param PointGridSize is the size in normalize note coordinates of the grid.
    \param scrapImage is used to get the original size and dotPerMeter

    This returns a regualar grid.
*/
cwTriangulateTask::PointGrid cwTriangulateTask::createPointGrid(QRectF bounds, cwImage scrapImage, const cwNoteTranformation &noteTransform) const {
    PointGrid grid;

    QSize scrapImageSize = scrapImage.origianlSize();
    double sizeOnPaperX = scrapImageSize.width() / (double)scrapImage.originalDotsPerMeter(); //in meters
    double sizeOnPaperY = scrapImageSize.height() / (double)scrapImage.originalDotsPerMeter(); //in meters

    double pointsPerMeter = 1.0; //Grid resolution
    double scale = noteTransform.scale();
    grid.GridSize.setWidth((int)(sizeOnPaperX / scale * (double)pointsPerMeter));
    grid.GridSize.setHeight((int)(sizeOnPaperY / scale * (double)pointsPerMeter));
    grid.Points.resize(grid.GridSize.width() * grid.GridSize.height());

    double xDelta = bounds.width() / (double)grid.GridSize.width(); //xDelta in normalized note coordinates
    double yDelta = bounds.height() / (double)grid.GridSize.height();

    for(int y = 0; y < grid.GridSize.height(); y++) {
        for(int x = 0; x < grid.GridSize.width(); x++) {
            QPointF gridPoint = bounds.topLeft() + QPointF(x * xDelta, y * yDelta);
            int index = grid.index(x, y);
            grid.Points[index] = gridPoint;
        }
    }

    return grid;
}

/**
  \brief This creates a database of points that are in the polygon.

  This useful for quering which points are in the polygon which ones aren't.  This should be
  much faster than quering the polygon itself (which is probably much slower).

    This returns a set of indices that are within the polygon.
*/
QSet<int> cwTriangulateTask::pointsInPolygon(const cwTriangulateTask::PointGrid &grid, const QPolygonF &polygon) const {
    QSet<int> inPolygon;

    for(int i = 0; i < grid.Points.size(); i++) {
        const QPointF& point = grid.Points[i];
        if(polygon.containsPoint(point, Qt::OddEvenFill)) {
            inPolygon.insert(i);
        }
    }

    return inPolygon;
}

/**
  \brief This creats a quad database

  The database stores indices of the origin of quads.  There are two types of quads in the database.
  The quads that are complete in the scrap, and quads that are on the edge of the scrap.

  Quads that are outside of the scrap's outline aren't stored in the database, and simply discarded.
  */
cwTriangulateTask::QuadDatabase cwTriangulateTask::createQuads(const cwTriangulateTask::PointGrid &grid, const QSet<int> &pointsInScrap) {
    //The valid grid size, crop out the last band of points
    int width = grid.GridSize.width() - 1;
    int height = grid.GridSize.height() - 1;

    QuadDatabase quadDatabase;

    for(int y = 0; y < height; y++) {
        for(int x = 0; x < width; x++) {
            int index = grid.index(x, y);
            Quad quad = grid.quad(index);

            //If quad is in the outline
            if(pointsInScrap.contains(quad.topLeft()) &&
                   pointsInScrap.contains(quad.topRight()) &&
                    pointsInScrap.contains(quad.bottomLeft()) &&
                    pointsInScrap.contains(quad.bottomRight())) {
                //Quad is completely in the outline
                quadDatabase.FullQuads.append(index);

            } else if(pointsInScrap.contains(quad.topLeft()) ||
                      pointsInScrap.contains(quad.topRight()) ||
                       pointsInScrap.contains(quad.bottomLeft()) ||
                       pointsInScrap.contains(quad.bottomRight())) {
                //Edge case
                quadDatabase.PartialQuads.append(index);

            }
        }
    }

    return quadDatabase;
}


/**
  \brief This is a helper function that gets the quad at an origin

  If the origin is out of bounds or it's on the bottom row or far right column, this will
  functon will ABORT!

  This will always return a valid quad, or the function will just abort!
  */
cwTriangulateTask::Quad cwTriangulateTask::PointGrid::quad(int origin) const {
    QPoint pointIndex = xyIndices(origin);

    int topLeft = origin;
    int topRight = index(pointIndex.x() + 1, pointIndex.y());
    int bottomLeft = index(pointIndex.x(), pointIndex.y() + 1);
    int bottomRight = index(pointIndex.x() + 1, pointIndex.y() + 1);

    Q_ASSERT(isValid(topLeft));
    Q_ASSERT(isValid(topRight));
    Q_ASSERT(isValid(bottomLeft));
    Q_ASSERT(isValid(bottomRight));

    return cwTriangulateTask::Quad(topLeft, topRight, bottomLeft, bottomRight);
}
