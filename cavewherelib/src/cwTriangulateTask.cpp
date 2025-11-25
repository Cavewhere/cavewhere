/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwTriangulateTask.h"
#include "cwCropImageTask.h"
#include "cwDebug.h"
#include "utils/cwTriangulate.h"
#include "cwAsyncFuture.h"
#include "cwRunningProfileScrapViewMatrix.h"
#include "cwConcurrent.h"

//Utils includes
#include "utils/Forsyth.h"

//Async future
#include "asyncfuture.h"

//Qt includes
#include <QDebug>
#include <cmath>

void cwTriangulateTask::setScrapData(QList<cwTriangulateInData> scraps) {
    Scraps = scraps;
}

void cwTriangulateTask::setProjectFilename(QString filename)
{
    ProjectFilename = filename;
}

void cwTriangulateTask::setFormatType(cwTextureUploadTask::Format format)
{
    Format = format;
}

QList<QFuture<cwTriangulatedData>> cwTriangulateTask::triangulate() const
{
    Q_ASSERT(!Scraps.isEmpty());

    QString projectFilename = ProjectFilename;
    cwTextureUploadTask::Format format = Format;

    auto triangulateScrap
        = [projectFilename, format](const cwTriangulateInData& scrap)->QFuture<cwTriangulatedData>
    {

        auto cropFuture = cropScrap(scrap, projectFilename, format);

        return AsyncFuture::observe(cropFuture)
            .subscribe([cropFuture, scrap, projectFilename]()
                       {

                           cwTextureUploadTask uploadTask;
                           auto croppedImagePtr = cropFuture.result();

                           if(croppedImagePtr) {
                               cwImage croppedImage = *(cropFuture.result());
                               uploadTask.setImage(croppedImage);
                               uploadTask.setProjectFilename(projectFilename);
                               uploadTask.setType(cwTextureUploadTask::OpenGL_RGBA);
                               auto uploadFuture = uploadTask.mipmaps();

                               return AsyncFuture::observe(uploadFuture)
                                   .subscribe(
                                       [scrap, cropFuture, uploadFuture]() {

                                           return cwConcurrent::run([scrap, cropFuture, uploadFuture]()
                                                                    {
                                                                        return triangulateGeometry(scrap,
                                                                                                   cropFuture.result(),
                                                                                                   uploadFuture.result());
                                                                    });
                                       }).future();
                           }

                           qDebug() << "Problem cropping image, does it not exist";
                           return QtFuture::makeReadyValueFuture(cwTriangulatedData());

                       }).future();
    };

    return cw::transform(Scraps, triangulateScrap);
}

QFuture<cwTrackedImagePtr> cwTriangulateTask::cropScrap(const cwTriangulateInData &scrap,
                                                        const QString &projectFilename,
                                                        cwTextureUploadTask::Format format)
{
    cwCropImageTask cropTask;
    cropTask.setDatabaseFilename(projectFilename);
    cropTask.setFormatType(format);
    cropTask.setOriginal(scrap.noteImage());

    QRectF cropArea = scrap.outline().boundingRect();
    cropTask.setRectF(cropArea);

    return cropTask.crop();
}

cwTriangulatedData cwTriangulateTask::triangulateGeometry(const cwTriangulateInData &scrap,
                                                          cwTrackedImagePtr croppedImage,
                                                          const cwTextureUploadTask::UploadResult& imageData)
{
    QRectF bounds = scrap.outline().boundingRect();

    //Create the regualar mesh that covers the croppedImage
    PointGrid pointGrid = createPointGrid(bounds, scrap);

    //Find all the points in the regualar mesh that are in the scrap's polygon
    QSet<int> gridPointsInScrap = pointsInPolygon(pointGrid, scrap.outline());

    //Creates list of quads that are on the edges or in the scrap
    QuadDatabase quads = createQuads(pointGrid, scrap.outline());

    //Triangulate the quads (this will update the outputs data)
    ScrapGeomtery scrapGeometry = createTriangles(pointGrid, gridPointsInScrap, quads, scrap);

    //Create the matrix that converts the normalized note coords to normalized scrap coords
    QMatrix4x4 toLocal = mapToScrapCoordinates(bounds);

    //Convert the normalized points to local note points
    QVector<QVector3D> localNotePoints = mapToLocalNoteCoordinates(toLocal, scrapGeometry.points);

    //Create the texture coordinates
    QVector<QVector2D> texCoords = mapTexCoordinates(localNotePoints);

    //Morph the points for the scrap
    QVector<QVector3D> points = morphPoints(scrapGeometry.points, scrap, toLocal, *croppedImage);

    //Morph the lead points for the scrap
    QVector<QVector3D> leadPoints = morphPoints(leadPositionToVector3D(scrap.leads()),
                                                scrap,
                                                toLocal,
                                                *croppedImage);


    cwGeometry geometry {
        { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 },
        { cwGeometry::Semantic::TexCoord0, cwGeometry::AttributeFormat::Vec2 }
    };
    geometry.set(cwGeometry::Semantic::Position, points);
    geometry.set(cwGeometry::Semantic::TexCoord0, texCoords);
    geometry.setIndices(scrapGeometry.indices);
    geometry.setCullBackfaces(false);

    cwTriangulatedData outputData;
    outputData.setCroppedImageData(imageData);
    outputData.setCroppedImage(croppedImage);
    outputData.setScrapGeometry(geometry);
    outputData.setLeadPoints(leadPoints);

    return outputData;
}

/**
    \brief Creates a point grid

    This is a regualar grid that has grid  of a meter.

    \param PointGridSize is the size in normalize note coordinates of the grid.
    \param scrapImage is used to get the original size and dotPerMeter

    This returns a regualar grid.
*/
cwTriangulateTask::PointGrid cwTriangulateTask::createPointGrid(QRectF bounds, const cwTriangulateInData& scrapData)  {
    PointGrid grid;

    cwNoteTransformationData noteTransform = scrapData.noteTransform();

    QSize scrapImageSize = scrapData.noteImage().originalSize();
    double sizeOnPaperX = scrapImageSize.width() / scrapData.noteImageResolution(); //in meters
    double sizeOnPaperY = scrapImageSize.height() / scrapData.noteImageResolution(); //in meters

    cwScale noteScale;
    noteScale.setData(noteTransform.scale);
    double scale = noteScale.scale(); //scale for the notes

    QTransform toCave;
    toCave.scale(sizeOnPaperX / scale, sizeOnPaperY / scale);

    QRectF inCave = toCave.mapRect(bounds);

    const auto morphingSettings = scrapData.morphingSettings();
    const double gridResolutionMeters = morphingSettings.gridResolutionMeters > 0.0
            ? morphingSettings.gridResolutionMeters
            : cwTriangulateWarpingData().gridResolutionMeters;

    double numberOfPointsX = inCave.width() / gridResolutionMeters;
    double numberOfPointsY = inCave.height() / gridResolutionMeters;

    double xDelta = bounds.width() / numberOfPointsX;
    double yDelta = bounds.height() / numberOfPointsY;


    grid.GridSize.setWidth((int)(numberOfPointsX) + 2);
    grid.GridSize.setHeight((int)(numberOfPointsY) + 2);
    grid.Points.resize(grid.GridSize.width() * grid.GridSize.height());
    grid.GridDeltaSize = QSizeF(xDelta, yDelta);

    for(int y = 0; y < grid.GridSize.height(); y++) {
        for(int x = 0; x < grid.GridSize.width(); x++) {
            //Grid point is normalized to the paper [0, 1]
            QPointF gridPoint = bounds.topLeft() + QPointF(x * xDelta, y * yDelta);
            int index = grid.index(x, y);
            grid.Points[index] = gridPoint;
        }
    }

    return grid;
}

/**
 * @brief cwTriangulateTask::PointGrid::index
 * @param point
 * @return The top left point of the cell that the point falls in.  If the point
 * is out side of the grid, this returns -1
 */
int cwTriangulateTask::PointGrid::index(QPointF point) const
{
    if(Points.isEmpty()) { return -1; }

    QPointF topLeft = Points.first();
    QPointF bottomRight = Points.last();
    QRectF bounds(topLeft, bottomRight);

    if(bounds.contains(point)) {
        point = (point - topLeft);
        int xIndex = (int)(point.x() / GridDeltaSize.width());
        int yIndex = (int)(point.y() / GridDeltaSize.height());

        return index(xIndex, yIndex);
    }

    return -1;
}


/**
  \brief This creates a database of points that are in the polygon.

  This useful for quering which points are in the polygon which ones aren't.  This should be
  much faster than quering the polygon itself (which is probably much slower).

    This returns a set of indices that are within the polygon.
*/
QSet<int> cwTriangulateTask::pointsInPolygon(const cwTriangulateTask::PointGrid &grid, const QPolygonF &polygon) {
    QSet<int> inPolygon;

    //Go through all the grid points
    for(int i = 0; i < grid.Points.size(); i++) {
        const QPointF& point = grid.Points[i];

        //See if the grid point containts point
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
cwTriangulateTask::QuadDatabase cwTriangulateTask::createQuads(const cwTriangulateTask::PointGrid &grid,
                                                               //                                                               const QSet<int> &pointsInScrap,
                                                               const QPolygonF& polygon) {
    //The valid grid size, crop out the last band of points
    int width = grid.GridSize.width() - 1;
    int height = grid.GridSize.height() - 1;

    QuadDatabase quadDatabase;

    //TODO: Optimization - this loop can be replace by iterating over the pointsInScrap
    for(int y = 0; y < height; y++) {
        for(int x = 0; x < width; x++) {
            int index = grid.index(x, y);
            Quad quad = grid.quad(index);

            if(grid.intersects(quad, polygon)) {
                quadDatabase.PartialQuads.append(quad);
            } else if(grid.quadContainInsideOfPolygon(quad, polygon)) {
                quadDatabase.FullQuads.append(quad);
            }
        }
    }
    return quadDatabase;
}

/**
    This triangulates the database of quads.

    grid - used to extract the point data
    database - used to extract to topology
    index - the location of where the point data will be stored in the output

    This returns the points that make up the polygon (these are in original note cooridates, before cropping)
 */
cwTriangulateTask::ScrapGeomtery cwTriangulateTask::createTriangles(const cwTriangulateTask::PointGrid &grid,
                                                      const QSet<int> pointsContainedInOutline,
                                                      const cwTriangulateTask::QuadDatabase &database,
                                                      const cwTriangulateInData &inScrapData) {

    //Resize the outputScrapData to have all points contained in the scrap outline
    QVector<QVector3D> points;
    points.resize(pointsContainedInOutline.size());

    //Create a map between indices in grid, and indices in the output
    QHash<int, int> mapGridToOutputIndices;
    int current = 0;
    foreach(int gridIndex, pointsContainedInOutline) {
        points[current] = QVector3D(grid.Points[gridIndex]);
        mapGridToOutputIndices[gridIndex] = current;
        current++;
    }

    //Do triangulation
    QVector<uint32_t> fullTriangleIndices = createTrianglesFull(database, mapGridToOutputIndices);
    QVector<QPointF> partialTriangles = createTrianglesPartial(grid, database, inScrapData.outline());

    //Get the final triangle set
    mergeFullAndPartialTriangles(points, fullTriangleIndices, partialTriangles);

    //Optimize triangle indices for graphics card triangle cache preformance
    QVector<uint32_t> optimizedIndices;
    optimizedIndices.reserve(fullTriangleIndices.size());
    optimizedIndices.resize(fullTriangleIndices.size());

    optimizedIndices = fullTriangleIndices;

    //FIXME: Make this work under windows
#ifndef Q_OS_WIN
    //This has a memory issues and crashes on MacOS built in xcode
    // Forsyth::OptimizeFaces(fullTriangleIndices.constData(),
    //                        fullTriangleIndices.size(),
    //                        points.size(),
    //                        optimizedIndices.data(),
    //                        24); //Optimize for 24 triangle count
#endif

    return {
        points,
        optimizedIndices
    };

    // //Set the output's data
    // cwGeometry geometry {
    //     { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 },
    //     { cwGeometry::Semantic::TexCoord0, cwGeometry::AttributeFormat::Vec2 }
    // };
    // geometry.setIndexes(optimizedIndices);
    // geometry.set(cwGeometry::Semantic::Position, points);

    // cwTriangulatedData data;
    // data.setScrapGeometry(std::move(geometry));
    // return data;
}

/**
    This will go through all full quads in the database and create triangles for them.

    The outputed triangles will be in counter clockwis order

    The grid is used to get the quad
  */
QVector<uint32_t> cwTriangulateTask::createTrianglesFull(const cwTriangulateTask::QuadDatabase &database,
                                                     const QHash<int, int> &mapGridToOutput) {

    QVector<uint32_t> triangles;

    //For all the quads
    foreach(const Quad& quad, database.FullQuads) {
        //Map the quad to the new outputIndices
        uint32_t topLeft = (uint32_t)mapGridToOutput.value(quad.topLeft(), 0);
        uint32_t topRight = (uint32_t)mapGridToOutput.value(quad.topRight(), 0);
        uint32_t bottomLeft = (uint32_t)mapGridToOutput.value(quad.bottomLeft(), 0);
        uint32_t bottomRight = (uint32_t)mapGridToOutput.value(quad.bottomRight(), 0);

        //Top triangle
        triangles.append(topLeft);
        triangles.append(bottomLeft);
        triangles.append(topRight);

        //Bottom triangle
        triangles.append(bottomLeft);
        triangles.append(bottomRight);
        triangles.append(topRight);
    }

    return triangles;
}

bool distanceLessThan(const QPair<QPointF, double> &d1, const QPair<QPointF, double> &d2)
{
    return d1.second < d2.second;
}

/**
  This function calculates the edge condition where there are partial boxes

  This function will create a small polygon that's the intersection between the full outline
  and partial quad.
  */
QVector<QPointF> cwTriangulateTask::createTrianglesPartial(const cwTriangulateTask::PointGrid& grid,
                                                           const cwTriangulateTask::QuadDatabase &database,
                                                           const QPolygonF& scrapOutline) {

    QVector<QPointF> allTriangles;

    foreach(const Quad& quad, database.PartialQuads) {
        QPointF topLeft = grid.Points[quad.topLeft()];
        QPointF topRight = grid.Points[quad.topRight()];
        QPointF bottomLeft = grid.Points[quad.bottomLeft()];
        QPointF bottomRight = grid.Points[quad.bottomRight()];

        QPolygonF quadPolygon;
        quadPolygon.reserve(5);
        quadPolygon.append(topLeft);
        quadPolygon.append(topRight);
        quadPolygon.append(bottomRight);
        quadPolygon.append(bottomLeft);
        quadPolygon.append(topLeft);

        //Find the intersection
        QPolygonF intesection = scrapOutline.intersected(quadPolygon);

        if(intesection.isEmpty()) {
            qDebug() << "This is a bug! intesection is empty" << LOCATION;
            continue;
        }

        //Close the polygon
        if(!intesection.isClosed()) {
            intesection.append(intesection.first());
        }

        QPolygonF realIntersectionPolygon = addPointsOnOverlapingEdges(intesection);
        QList<QPolygonF> simplePolygons = createSimplePolygons(realIntersectionPolygon);

        foreach(QPolygonF simplePolygon, simplePolygons) {
            //Triangluate polygon!
            QVector<QPointF> triangles;
            bool couldTrianglate = cwTriangulate::Process(simplePolygon, triangles);

            if(!couldTrianglate) {
                qDebug() << "Couldn't triangulate ... Non-simple polygon?! " << LOCATION;

                foreach(QPointF point, simplePolygon) {
                    qDebug() << point.x() << point.y();
                }
                qDebug() << "**Is closed**:" << simplePolygon.isClosed();
                foreach(QPointF point, intesection) {
                    qDebug() << point.x() << point.y();
                }
                qDebug() << "-------------------------------";
            }

            allTriangles += triangles;
        }
    }

    return allTriangles;
}

/**
 * @brief cwTriangulateTask::fixOverlapingPolygon
 * @param polygon - the polygon that's generated by Qt's intersected
 * @return A polygon that has points on all the overlapping edges
 *
 * This will add points to the polygon where internal axis align edges overlap.
 * This will add points at the begin of the overlapping edges.  This will allow
 * create simplePolygons() to run correctly.
 */
QPolygonF cwTriangulateTask::addPointsOnOverlapingEdges(QPolygonF polygon)
{        //Generate the real polygon
    QPolygonF realIntersectionPolygon;

    //First go through the intersection and find overlapping points
    for(int i = 0; i < polygon.size() - 1; i++) {
        QLineF line(polygon.at(i), polygon.at(i+1));

        realIntersectionPolygon.append(line.p1());

        QList< QPair<QPointF, double> > newPoints; //The new point, distance to p1

        //Go through all the points in the intesection, and see if the point lays on the line
        //This loop will ignore the points that are already in the line
        int endIndex = i;
        int current = (i + 2) % (polygon.size() - 1); //loop around
        while(current != endIndex) {
            QPointF point = polygon.at(current);

            //If point lays on the line, then add it to the realIntersectionPolygon
            //This only checks axis align bits
            if(line.dx() == 0.0) {
                if(point.x() == line.p1().x()) {
                    double minY = qMin(line.p1().y(), line.p2().y());
                    double maxY = qMax(line.p1().y(), line.p2().y());
                    if(minY <= point.y() && point.y() <= maxY) {
                        //Add point to realIntersectionPolygon
                        double distance = QLineF(point, line.p1()).length();
                        newPoints.append(QPair<QPointF, double>(point, distance));

                    }
                }
            }

            if(line.dy() == 0.0) {
                if(point.y() == line.p1().y()) {
                    double minX = qMin(line.p1().x(), line.p2().x());
                    double maxX = qMax(line.p1().x(), line.p2().x());
                    if(minX <= point.x() && point.x() <= maxX) {
                        //Add point to realIntersectionPolygon
                        double distance = QLineF(point, line.p1()).length();
                        newPoints.append(QPair<QPointF, double>(point, distance));
                    }
                }
            }

            current = (current + 1) % (polygon.size() - 1); //loop around
        }


        if(!newPoints.isEmpty()) {
            //Sort new points by distance to line.p1()
            std::stable_sort(newPoints.begin(), newPoints.end(), distanceLessThan);

            //Add the sorted points into realIntersectionPolygon
            for(int ii = 0; ii < newPoints.size(); ii++) {
                realIntersectionPolygon.append(newPoints.at(ii).first);
            }
        }
    }

    //Close it
    if(!realIntersectionPolygon.isClosed()) {
        realIntersectionPolygon.append(realIntersectionPolygon.first());
    }

    return realIntersectionPolygon;
}

/**
 * @brief cwTriangulateTask::createSimplePolygons
 * @param polygon - A complex polygon, that may contain overlapping axis aligned edges
 * @return A list of simple polygons
 */
QList<QPolygonF> cwTriangulateTask::createSimplePolygons(QPolygonF polygon) {
    //Find polygons from the polygon
    QList<QPolygonF> simplePolygons;
    QPolygonF currentSimplePolygon;
    for(int i = 0; i < polygon.size(); i++) {
        QPointF point = polygon.at(i);
        int index = currentSimplePolygon.indexOf(point);

        //Index found?
        if(index >= 0) {
            //Create a simple polygon
            QPolygonF simplePolygon;
            for(int ii = index; ii < currentSimplePolygon.size(); ii++) {
                simplePolygon.append(currentSimplePolygon.at(ii));
            }
            simplePolygon.append(point);

            if(simplePolygon.isClosed()) {
                simplePolygon.pop_back();
            }

            if(simplePolygon.size() > 2) {
                simplePolygons.append(simplePolygon);
            }

            //Remove all the points to and including the index
            currentSimplePolygon.remove(index, currentSimplePolygon.size() - index);
        }

        currentSimplePolygon.append(point);
    }

    return simplePolygons;
}
/**
    This function will add the unAddTriangles into indices.  It will also add triangle's points into
    points.  This function will attemp to reuse points such all of pointSet's points are unique.

    This function assumes that the points in pointSet are already unique before calling this functions.
  */
void cwTriangulateTask::mergeFullAndPartialTriangles(QVector<QVector3D> &pointSet,
                                                     QVector<uint32_t> &indices,
                                                     const QVector<QPointF>& unAddedTriangles)
{
    static const float PointTolerance = 0.000001f;

    foreach(QPointF unAddedPoint, unAddedTriangles) {

        bool foundExistIndex = false;
        uint32_t index;

        //Search for the point in the point set
        //This is a slow linear search, could be replace with a kd-tree of the pointSet
        //Good enough for now...
        for(int i = 0; i < pointSet.size(); i++) {
            const QVector3D& point = pointSet[i];
            float xDelta = fabs((float)unAddedPoint.x() - point.x());
            float yDelta = fabs((float)unAddedPoint.y() - point.y());

            if(xDelta <= PointTolerance && yDelta <= PointTolerance) {
                //Hey we found the point
                index = (uint32_t)i;
                foundExistIndex = true;
                break;
            }
        }

        //If we didn't find the index this is a new point, so add it to the pointSet
        if(!foundExistIndex) {
            index = (uint32_t)pointSet.size(); //The last index
            pointSet.append(QVector3D(unAddedPoint));
        }

        //The next index for the triangles
        indices.append(index);
    }
}

/**
  Maps the bounds into local normalized coordinates
  */
QMatrix4x4 cwTriangulateTask::mapToScrapCoordinates(const QRectF &bounds)
{
    float xScale = 1 / bounds.width();
    float yScale = 1 / bounds.height();

    QPointF topLeft = -bounds.topLeft();

    QMatrix4x4 matrix;
    matrix.scale(xScale, yScale, 1.0);
    matrix.translate(topLeft.x(), topLeft.y());

    return matrix;
}

QVector<QVector3D> cwTriangulateTask::mapToLocalNoteCoordinates(QMatrix4x4 toLocal, const QVector<QVector3D> &normalizeNoteCoords) {
    QVector<QVector3D> mappedCoords;
    mappedCoords.reserve(normalizeNoteCoords.size());
    mappedCoords.resize(normalizeNoteCoords.size());

    for(int i = 0; i < normalizeNoteCoords.size(); i++) {
        mappedCoords[i] = toLocal.map(normalizeNoteCoords[i]);
    }

    return mappedCoords;
}

/**
    This takes the bounds of the cropped scrap and the point list of normalized cooridates of the original image.

    This will tranform the normalized coordinates into local normalized coordinates such that
    the cropped image can be textured in opengl

    This function will produces texcoordinates
  */
QVector<QVector2D> cwTriangulateTask::mapTexCoordinates(const QVector<QVector3D> &localNoteCoords) {

    QVector<QVector2D> texCoords;
    texCoords.reserve(localNoteCoords.size());

    foreach(QVector3D coord, localNoteCoords) {
        QVector2D texCoord = coord.toVector2D();
        texCoords.append(texCoord);
    }

    return texCoords;
}

/**
  \brief This function morphs the points so the texture image aligns with the lineplot
  */
QVector<QVector3D> cwTriangulateTask::morphPoints(const QVector<QVector3D>& notePoints,
                                                  const cwTriangulateInData& scrapData,
                                                  const QMatrix4x4& toLocal,
                                                  const cwImage& croppedImage) {


    /**
      This sorts scrapData stations if the scrap is in running profile mode.

      This returns a list of sorted stations base on stations position in the x-axis
      */
    auto sortScrapStations = [&scrapData]() {
        QList<cwTriangulateStation> stations = scrapData.stations();
        if(scrapData.viewMatrix()->type() == cwScrap::RunningProfile) {
            //This assumes that up on the page is up for the scrap
            auto profileCompare = [&scrapData](const cwTriangulateStation& left, const cwTriangulateStation& right)->bool {
                QMatrix4x4 rotation = toNoteMatrix(scrapData.noteTransform());

                QVector3D leftPoint = rotation.map(left.notePosition());
                QVector3D rightPoint = rotation.map(right.notePosition());

                return leftPoint.x() < rightPoint.x();
            };

            //Sort the stations by note position
            std::sort(stations.begin(), stations.end(), profileCompare);
        }
        return stations;
    };

    /**
     * Given a list of stations this finds the stations to use as control points in the morphing.
     *
     * If scrapData is running profile this will use two stations that notePoint.x() falls between.
     * Those two stations are returned. The notePoint.x() will be rotated to the northUp().
     *
     * If scrapData is plan this will simply return stations.
     */
    auto findStationsToUseForMorphing = [&scrapData](QList<cwTriangulateStation> stations, QVector3D notePoint)->QList<cwTriangulateStation>
    {
        if(scrapData.viewMatrix()->type() == cwScrap::RunningProfile) {

            //Need to have a least two stations
            if(stations.size() < 2) {
                return QList<cwTriangulateStation>();
            }

            //Look for the section for the notePoint, returns the stations we want to warp between
            auto compare = [&scrapData](const cwTriangulateStation& station, const QVector3D& point)->bool {
                QMatrix4x4 rotation = toNoteMatrix(scrapData.noteTransform());

                QVector3D left = rotation.map(station.notePosition());
                QVector3D right = rotation.map(point);

                return left.x() < right.x();
            };

            auto foundStation = std::lower_bound(stations.begin(), stations.end(), notePoint, compare);

            if(stations.size() >= 2) {
                if(foundStation == stations.end()) {
                    foundStation = foundStation - 1; //To from the end
                } else if(foundStation == stations.begin()) {
                    foundStation = foundStation + 1;
                }
            } else {
                foundStation = stations.begin();
            }

            QList<cwTriangulateStation> profileStations;
            profileStations.append(*(foundStation - 1));
            profileStations.append(*(foundStation));
            return profileStations;
        }

        auto findClosestInterpolatedStations = [](const QList<cwTriangulateStation>& stations,
                                                  const QVector3D& point,
                                                  int stationCount)->QList<cwTriangulateStation>
        {
            if(stationCount <= 0 || stations.isEmpty()) {
                return QList<cwTriangulateStation>();
            }

            QList<cwTriangulateStation> closestStations;
            closestStations.reserve(stationCount);
            QVector<double> closestDistances;
            closestDistances.reserve(stationCount);

            for(const auto& station : stations) {
                const double distance = (station.notePosition() - point).lengthSquared();

                if(closestStations.size() < stationCount) {
                    closestStations.append(station);
                    closestDistances.append(distance);
                    continue;
                }

                int worstIndex = 0;
                double worstDistance = closestDistances[0];
                for(int i = 1; i < closestDistances.size(); ++i) {
                    if(closestDistances[i] > worstDistance) {
                        worstDistance = closestDistances[i];
                        worstIndex = i;
                    }
                }

                if(distance < worstDistance) {
                    closestStations[worstIndex] = station;
                    closestDistances[worstIndex] = distance;
                }
            }

            return closestStations;
        };

        // auto findInterpolatedStationsWithinRadius = [](const QList<cwTriangulateStation>& stations,
        //                                                const QVector3D& point,
        //                                                double radius)->QList<cwTriangulateStation>
        // {
        //     if(radius <= 0.0 || stations.isEmpty()) {
        //         return QList<cwTriangulateStation>();
        //     }

        //     QList<cwTriangulateStation> stationsInRadius;
        //     stationsInRadius.reserve(stations.size());

        //     const double radiusSquared = radius * radius;
        //     for(const auto& station : stations) {
        //         const double distanceSquared = (station.notePosition() - point).lengthSquared();
        //         if(distanceSquared <= radiusSquared) {
        //             stationsInRadius.append(station);
        //         }
        //     }

        //     return stationsInRadius;
        // };

        auto interpolatedStations = buildStationsWithInterpolatedShots(scrapData);

        const auto morphingSettings = scrapData.morphingSettings();
        const bool useMaxClosestStations = morphingSettings.useMaxClosestStations;
        const int maxClosestStations = morphingSettings.maxClosestStations > 0
                ? morphingSettings.maxClosestStations
                : cwTriangulateWarpingData().maxClosestStations;
        // constexpr double kStationSearchRadius = .1;

        // double currentRadius = kStationSearchRadius;
        // QList<cwTriangulateStation> stationWithinRadius;
        // do {
        //     stationWithinRadius = findInterpolatedStationsWithinRadius(interpolatedStations,
        //                                                                      notePoint,
        //                                                                      currentRadius);
        //     currentRadius *= 1.25; //Double the radius
        // } while (stationWithinRadius.size() < 1);

        // if(stationsWithinRadius.isEmpty()) {
        const int stationCount = useMaxClosestStations ? maxClosestStations
                                                       : interpolatedStations.size();

        return findClosestInterpolatedStations(interpolatedStations,
                                               notePoint,
                                               stationCount);
        // } else {
        //     interpolatedStations = stationsWithinRadius;
        // }

        // return stationWithinRadius;

        //Figure out which stations are visible to this point
        // stations = stationsVisibleToPoint(notePoint,
        //                                   interpolatedStations,
        //                                   scrapData.outline());
        // return stations;
    };

    /**
      When the scrapData is a plan type, this function returns a identity matrix

      When in running profile mode, this returns a view matrix of the shot between the first
      and last station in stations. This view matrix is in a profile view, where the shot
      is aligned with the x-axis.

      When in projected profile mode, this returns the view matrix from cwScrapViewMatrix.
     */
    auto calculateViewMatrix = [&scrapData](const QList<cwTriangulateStation>& stations) {

        if(scrapData.viewMatrix()->type() == cwAbstractScrapViewMatrix::RunningProfile) {
            //Calculate the rotation matrix for the profile for this point (could be looked up)
            if(stations.size() < 2) {
                return QMatrix4x4();
            }

            QVector3D fromPostion = stations.first().position();
            QVector3D toPosition = stations.last().position();

            //These are rotation offset, so we do the rotation around the from station's position
            QMatrix4x4 translateForward;
            translateForward.translate(fromPostion);

            QMatrix4x4 translateBackward;
            translateBackward.translate(-fromPostion);

            QMatrix4x4 viewRotationMatrix = cwRunningProfileScrapViewMatrix::Data(fromPostion, toPosition).matrix();

            QMatrix4x4 viewMatrix = translateForward * viewRotationMatrix * translateBackward;

            return viewMatrix;
        }
        return scrapData.viewMatrix()->matrix();
    };

    //Smooth points in world space to soften z variation near each point.
    auto smoothZ = [&scrapData](const QVector<QVector3D>& worldPoints, const QMatrix4x4 viewMatrix, double maxDistanceMeters) {
        //Running profile not supported for smoothing
        if(scrapData.viewMatrix()->type() == cwAbstractScrapViewMatrix::RunningProfile) {
            return worldPoints;
        }

        if(worldPoints.isEmpty() || maxDistanceMeters <= 0.0) {
            return worldPoints;
        }

        QVector<QVector3D> smoothed;
        smoothed.reserve(worldPoints.size());
        smoothed.resize(worldPoints.size());

        const double maxDistanceSquared = maxDistanceMeters * maxDistanceMeters;
        const double sigma = maxDistanceMeters * 0.5;
        const double twoSigmaSquared = 2.0 * sigma * sigma;

        const QMatrix4x4 viewMatrixInv = viewMatrix.inverted();

        for(int i = 0; i < worldPoints.size(); ++i) {
            double weightedZ = 0.0;
            double weightSum = 0.0;
            const QVector3D localI = viewMatrix.map(worldPoints[i]); //Rotate into eye space
            const QVector3D xyi = {localI.x(), localI.y(), 0.0};

            for(int j = 0; j < worldPoints.size(); ++j) {
                const QVector3D localJ = viewMatrix.map(worldPoints[j]);
                const QVector3D xyj = {localJ.x(), localJ.y(), 0.0};

                const QVector3D delta = xyj - xyi;
                const double distanceSquared = delta.lengthSquared();
                if(distanceSquared > maxDistanceSquared) {
                    continue;
                }

                //Gaussian kernel that favors neighbors closest to the current point.
                const double weight = std::exp(-distanceSquared / twoSigmaSquared);
                weightedZ += weight * static_cast<double>(localJ.z());
                weightSum += weight;
            }

            if(weightSum > 0.0) {
                const double blendedZ = weightedZ / weightSum;
                smoothed[i] = viewMatrixInv.map(QVector3D(localI.x(),
                                                          localI.y(),
                                                          static_cast<float>(blendedZ)));
            } else {
                smoothed[i] = worldPoints[i];
            }
        }

        return smoothed;
    };

    QSize imageSize = croppedImage.originalSize();
    double metersPerDot = 1.0 / (double)scrapData.noteImageResolution();

    //For right now try to map
    QMatrix4x4 toPixels;
    toPixels.scale(imageSize.width(), imageSize.height(), 1.0);

    QMatrix4x4 toMetersOnPaper;
    toMetersOnPaper.scale(metersPerDot, metersPerDot, 1.0);

    QMatrix4x4 toMetersInCave = toNoteMatrix(scrapData.noteTransform());

    QMatrix4x4 toWorldCoords = toMetersInCave * toMetersOnPaper * toPixels * toLocal;

    QList<cwTriangulateStation> stations = sortScrapStations();

    QVector<QVector3D> points;
    points.reserve(notePoints.size());
    points.resize(notePoints.size());
    for(int i = 0; i < notePoints.size(); i++) {

        //Find the stations we want to use the morph the current note point
        QList<cwTriangulateStation> stationsUsedToMorph = findStationsToUseForMorphing(stations, notePoints[i]);

        //Find the view that we want to use for the morphing
        QMatrix4x4 viewMatrix = calculateViewMatrix(stationsUsedToMorph);

        //Based on the visible stations morph point into the scene coords
        points[i] = morphPoint(stationsUsedToMorph, toWorldCoords, viewMatrix, notePoints[i]);
    }

    const auto morphingSettings = scrapData.morphingSettings();
    double smoothingRadiusMeters = 0.0;
    if(morphingSettings.useSmoothingRadius) {
        smoothingRadiusMeters = morphingSettings.smoothingRadiusMeters > 0.0
                ? morphingSettings.smoothingRadiusMeters
                : cwTriangulateWarpingData().smoothingRadiusMeters;
    }
    points = smoothZ(points, scrapData.viewMatrix()->matrix(), smoothingRadiusMeters);

    return points;
}

/**
  \brief Get's a list of station that are visible to the point.

  This shot a ray between point and each station.  If the ray interects any of the
  polygon's lines, then the station isn't added to the list visble points

  The point should be in normalize note cooridanets.  This isn't local note coordinates
  */
QList<cwTriangulateStation> cwTriangulateTask::stationsVisibleToPoint(const QVector3D &point,
                                                                      const QList<cwTriangulateStation> &stations,
                                                                      const QPolygonF &scrapOutline){

    QList<QLineF> polygonLines;
    //For all the lines it the polygon
    int isClosed = (int)(!scrapOutline.isClosed());
    int numPoints = scrapOutline.size() + isClosed;
    for(int i = 0; i < numPoints - 1; i++) {
        int p1Index = i % scrapOutline.size();
        int p2Index = (i + 1) % scrapOutline.size();

        QLineF line(scrapOutline[p1Index], scrapOutline[p2Index]);
        polygonLines.append(line);
    }

    QList<cwTriangulateStation> visibleStations;

    QPointF point2D = point.toPointF();
    double errorTolerance = 1.0e-6;

    //For each station
    foreach(cwTriangulateStation station, stations) {

        //Create a line between the point and the station
        QLineF ray(point2D,station.notePosition().toPointF());

        //The flag if the station should be added
        bool shouldAdd = true;

        //Do all the intersections
        QPointF intersectionPoint;
        foreach(QLineF line, polygonLines) {
            if(ray.intersects(line, &intersectionPoint) == QLineF::BoundedIntersection) {
                double percentX = fabs(intersectionPoint.x() - point2D.x()) / point2D.x();
                double percentY = fabs(intersectionPoint.y() - point2D.y()) / point2D.y();

                if(percentX >= errorTolerance || percentY >= errorTolerance) {
                    shouldAdd = false;
                    break;
                }
            }
        }

        if(shouldAdd) {
            visibleStations.append(station);
        }
    }

    //If the point has no visibleStation, we need to figure out which station are
    //closest and uses those,  Uses the shot intersection?
    if(visibleStations.size() < 2 && !stations.isEmpty()) {

        //For debugging for now, this should be really obvious that this code is
        //wrong

        visibleStations.clear();

        //The two closest stations
        cwTriangulateStation station1;
        cwTriangulateStation station2;
        double distance1 = std::numeric_limits<double>::max();
        double distance2 = distance1;

        foreach(cwTriangulateStation station, stations) {
            double length = QLineF(point2D, station1.notePosition().toPointF()).length();

            //Heristic
            if(station1.name().isEmpty()) {
                distance1 = length;
                station1 = station;
                continue;
            }

            if(length < distance1) {
                //Push distance1 to distance2
                distance2 = distance1;
                station2 = station1;

                distance1 = length;
                station1 = station;
                continue;
            }

            if(station2.name().isEmpty()) {
                distance2 = length;
                station2 = station;
                continue;
            }

            if(length < distance2) {
                distance2 = length;
                station2 = station;
            }
        }

        visibleStations.append(station1);
        visibleStations.append(station1);
    }

    return visibleStations;
}

/**
  \brief This morphs a single point based on the stations
  that are visible to it.

  This preforms a weighted average based on distance.

  \param visibleStations - The visible stations that the point can see
  \param toWorldCoords - The matrix that can translate a note coordinate into a world coordinate (this doesn't include offset)
  \param viewMatrix - The view that the point will be warped in. You can think of this as the projection.
  In plan view this should be an identitidy matrix. In running profile, this is a a rotation matrix to align the view
  to be perpindicular to the shot's compass bearing.
  \param point - The point that's going to be morphed, this is in original note coordinates
  */
QVector3D cwTriangulateTask::morphPoint(const QList<cwTriangulateStation> &visibleStations,
                                        const QMatrix4x4& toWorldCoords,
                                        const QMatrix4x4& viewMatrix,
                                        const QVector3D &point)
{

    //Calculate all distances the from the visibleStations, to point in note coordinates
    QVector<double> distances;
    distances.reserve(visibleStations.size());
    int onTopOfStationIndex = -1;
    for(int i = 0; i < visibleStations.size(); i++) {
        double distance = (visibleStations[i].notePosition() - point).length();
        // double distance = QLineF(visibleStations[i].notePosition(), point.toPointF()).length();
        if(distance != 0.0) {

            // auto weightFromDistance = [](const double distance) {
            //     const double innerRadius = 0.01;
            //     const double maximumWeight = 1.0; // or whatever scale you want

            //     const double effectiveDistance = std::max(distance, innerRadius);
            //     const double ratio = innerRadius / effectiveDistance;
            //     return maximumWeight * ratio * ratio;
            // };

            // qDebug() << "Adding weight:" << weightFromDistance(distance);
            // distances.append(weightFromDistance(distance));

            // double radius = 1.0; //1.0m
            // double clamped = std::max(distance, );
            // double inverseDistance = 1.0 / (clamped * clamped);

            //The inverse distance is give a high weight for points near stations
            double inverseDistance = 1 / (distance * distance);  //This is the function that allows the weight to be calculated correctly
            distances.append(inverseDistance);
        } else {
            //This is a special case where the point is on station
            onTopOfStationIndex = i;
            break;
        }
    }

    //Special case, where point is on station
    if(onTopOfStationIndex > -1) {
        //Just return the station's position in world coordinates
        return visibleStations[onTopOfStationIndex].position();
    }

    //Calculate the sum the distances
    double sum = 0.0;
    foreach(double distance, distances) {
        sum += distance;
    }

    //Calculate the weights for each station
    QVector<double> weights;
    weights.reserve(distances.size());
    weights.resize(distances.size());
    for(int i = 0; i < distances.size(); i++) {
        weights[i] = distances[i] / sum;
    }

    //Get the point's position in respect to each station
    QVector<QVector3D> pointInRespectToStations;
    pointInRespectToStations.reserve(visibleStations.size());
    foreach(cwTriangulateStation station, visibleStations) {

        //Setup the transformation
        QVector3D stationOnNote = toWorldCoords.map(QVector3D(station.notePosition())); //In view coordinates
        QVector3D stationPos = viewMatrix.map(station.position()); //In view coordianets

        QMatrix4x4 offsetToFirstStation;
        offsetToFirstStation.translate(-stationOnNote);
        offsetToFirstStation.translate(stationPos);

        QMatrix4x4 noteToScene = offsetToFirstStation * toWorldCoords; //outputs in world coordinates

        //Do the transformation
        QVector3D pointInRespectToCurrentStation = noteToScene.map(point);

        //Add to point list
        pointInRespectToStations.append(pointInRespectToCurrentStation);
    }

    Q_ASSERT(pointInRespectToStations.size() == weights.size());

    //Use the weights to find the final position
    QVector3D weightPosition;
    for(int i = 0; i < pointInRespectToStations.size(); i++) {
        weightPosition += weights[i] * pointInRespectToStations[i];
    }

    //Put the weighted position in world coordinates
    weightPosition = viewMatrix.inverted().map(weightPosition);

    return weightPosition;
}

/**
 * @brief cwTriangulateTask::leadPositionToVector3D
 * @param leads - The leads positions to extract
 * @return Returns the leads positions in QVector<QVector3D>
 *
 * This will go through all the leads and append the positions of the leads in order to QVector.
 * The vector is then returned.
 */
QVector<QVector3D> cwTriangulateTask::leadPositionToVector3D(const QList<cwLead> &leads)
{
    QVector<QVector3D> leadPositions;
    leadPositions.reserve(leads.size());
    foreach(const cwLead& lead, leads) {
        leadPositions.append(QVector3D(lead.positionOnNote()));
    }
    return leadPositions;
}

QMatrix4x4 cwTriangulateTask::toNoteMatrix(const cwNoteTransformationData &data)
{
    cwNoteTranformation transform;
    transform.setData(data);
    return transform.matrix();
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

/**
 * @brief cwTriangulateTask::PointGrid::intersects
 * @param polygon
 * @return True if the edges of the quad intersects with the edges of the polygon
 */
bool cwTriangulateTask::PointGrid::intersects(const cwTriangulateTask::Quad& quad, const QPolygonF &polygon) const
{
    if(polygon.size() < 2) { return false; }

    QVector<QLineF> edges;
    edges.reserve(4);
    edges.append(QLineF(Points.at(quad.topLeft()), Points.at(quad.topRight())));
    edges.append(QLineF(Points.at(quad.topRight()), Points.at(quad.bottomRight())));
    edges.append(QLineF(Points.at(quad.bottomRight()), Points.at(quad.bottomLeft())));
    edges.append(QLineF(Points.at(quad.bottomLeft()), Points.at(quad.topLeft())));

    QPointF intersectionPoint;

    for(int i = 0; i < polygon.size() - 1; i++) {
        QLineF polygonEdge(polygon.at(i), polygon.at(i + 1));
        foreach(QLineF edge, edges) {
            if(polygonEdge.intersects(edge, &intersectionPoint) == QLineF::BoundedIntersection) {
                return true;
            }
        }
    }

    if(!polygon.isClosed()) {
        QLineF polygonEdge(polygon.first(), polygon.last());
        foreach(QLineF edge, edges) {
            if(polygonEdge.intersects(edge, &intersectionPoint) == QLineF::BoundedIntersection) {
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief cwTriangulateTask::PointGrid::containInsideOf
 * @param polygon
 * @return True if the quad is completely inside of the polygon
 */
bool cwTriangulateTask::PointGrid::quadContainInsideOfPolygon(const cwTriangulateTask::Quad& quad, const QPolygonF &polygon) const
{
    return polygon.containsPoint(Points.at(quad.topLeft()), Qt::OddEvenFill) &&
           polygon.containsPoint(Points.at(quad.topRight()), Qt::OddEvenFill) &&
           polygon.containsPoint(Points.at(quad.bottomLeft()), Qt::OddEvenFill) &&
           polygon.containsPoint(Points.at(quad.bottomRight()), Qt::OddEvenFill);
}
