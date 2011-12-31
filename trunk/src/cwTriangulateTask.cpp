//Our includes
#include "cwTriangulateTask.h"
#include "cwCropImageTask.h"
#include "cwDebug.h"
#include "utils/cwTriangulate.h"

//Utils includes
#include "utils/Forsyth.h"

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

    //Triangulate the quads (this will update the outputs data)
    cwTriangulatedData triangleData = createTriangles(pointGrid, gridPointsInScrap, quads, scrapData);

    //Create the matrix that converts the normalized coords to the normalized coords
    QMatrix4x4 toLocal = localNormalizedCoordinates(bounds);

    //Convert the normalized points to local note points
    QVector<QVector3D> localNotePoints = mapToLocalNoteCoordinates(toLocal, triangleData.points());

    //Create the texture coordinates
    QVector<QVector2D> texCoords = mapTexCoordinates(localNotePoints);

    //Morph the points
    QVector<QVector3D> points = morphPoints(triangleData.points(), scrapData, toLocal, croppedImage);

    //For testing
    cwTriangulatedData& outScrapData = TriangulatedScraps[index];
    outScrapData.setIndices(triangleData.indices());
    outScrapData.setPoints(points);
    outScrapData.setTexCoords(texCoords);
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

    double pointsPerMeter = 2; //Grid resolution
    double scale = noteTransform.scale(); //scale for the notes

    double sizeInCaveX = sizeOnPaperX / scale; //in meters in cave
    double sizeInCaveY = sizeOnPaperY / scale; //in meters in cave

    double xDelta = bounds.width() / sizeInCaveX;
    double yDelta = bounds.height() / sizeInCaveY;

    double numberOfPointsX = sizeInCaveX * (double)pointsPerMeter;
    double numberOfPointsY = sizeInCaveY * (double)pointsPerMeter;

    grid.GridSize.setWidth((int)(numberOfPointsX) + 1);
    grid.GridSize.setHeight((int)(numberOfPointsY) + 1);
    grid.Points.resize(grid.GridSize.width() * grid.GridSize.height());

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
                quadDatabase.FullQuads.append(quad);

            } else if(pointsInScrap.contains(quad.topLeft()) ||
                      pointsInScrap.contains(quad.topRight()) ||
                       pointsInScrap.contains(quad.bottomLeft()) ||
                       pointsInScrap.contains(quad.bottomRight())) {
                //Edge case
                quadDatabase.PartialQuads.append(quad);

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
cwTriangulatedData cwTriangulateTask::createTriangles(const cwTriangulateTask::PointGrid &grid,
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
    QVector<uint> fullTriangleIndices = createTrianglesFull(database, mapGridToOutputIndices);
    QVector<QPointF> partialTriangles = createTrianglesPartial(grid, database, inScrapData.outline());

    //Get the final triangle set
    mergeFullAndPartialTriangles(points, fullTriangleIndices, partialTriangles);

    //Optimize triangle indices for graphics card triangle cache preformance
    QVector<uint> optimizedIndices;
    optimizedIndices.reserve(fullTriangleIndices.size());
    optimizedIndices.resize(fullTriangleIndices.size());

    Forsyth::OptimizeFaces(fullTriangleIndices.constData(),
                           fullTriangleIndices.size(),
                           points.size(),
                           optimizedIndices.data(),
                           24); //Optimize for 24 triangle count

    //Set the output's data
    cwTriangulatedData data;
    data.setIndices(optimizedIndices);
    data.setPoints(points);
    return data;
}

/**
    This will go through all full quads in the database and create triangles for them.

    The outputed triangles will be in counter clockwis order

    The grid is used to get the quad
  */
QVector<uint> cwTriangulateTask::createTrianglesFull(const cwTriangulateTask::QuadDatabase &database,
                                                    const QHash<int, int> &mapGridToOutput) {

    QVector<uint> triangles;

    //For all the quads
    foreach(const Quad& quad, database.FullQuads) {
        //Map the quad to the new outputIndices
        uint topLeft = (uint)mapGridToOutput.value(quad.topLeft(), 0);
        uint topRight = (uint)mapGridToOutput.value(quad.topRight(), 0);
        uint bottomLeft = (uint)mapGridToOutput.value(quad.bottomLeft(), 0);
        uint bottomRight = (uint)mapGridToOutput.value(quad.bottomRight(), 0);

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
        if(intesection.first() == intesection.last()) {
            intesection.pop_back();
        }


        //Triangluate polygon!
        QVector<QPointF> triangles;
        bool couldTrianglate = cwTriangulate::Process(intesection, triangles);

        if(!couldTrianglate) {
            qDebug() << "Couldn't triangulate ... Non-simple polygon?! " << LOCATION;

            foreach(QPointF point, intesection) {
                qDebug() << point.x() << point.y();
            }
            qDebug() << "-------------------------------";

        }

        allTriangles += triangles;
    }

    return allTriangles;
}
/**
    This function will add the unAddTriangles into indices.  It will also add triangle's points into
    points.  This function will attemp to reuse points such all of pointSet's points are unique.

    This function assumes that the points in pointSet are already unique before calling this functions.
  */
void cwTriangulateTask::mergeFullAndPartialTriangles(QVector<QVector3D> &pointSet,
                                                     QVector<uint> &indices,
                                                     const QVector<QPointF>& unAddedTriangles)
{
    static const float PointTolerance = 0.000001;

    foreach(QPointF unAddedPoint, unAddedTriangles) {

        bool foundExistIndex = false;
        uint index;

        //Search for the point in the point set
        //This is a slow linear search, could be replace with a kd-tree of the pointSet
        //Good enough for now...
        for(int i = 0; i < pointSet.size(); i++) {
            const QVector3D& point = pointSet[i];
            float xDelta = fabs((float)unAddedPoint.x() - point.x());
            float yDelta = fabs((float)unAddedPoint.y() - point.y());

            if(xDelta <= PointTolerance && yDelta <= PointTolerance) {
                //Hey we found the point
                index = (uint)i;
                foundExistIndex = true;
                break;
            }
        }

        //If we didn't find the index this is a new point, so add it to the pointSet
        if(!foundExistIndex) {
            index = (uint)pointSet.size(); //The last index
            pointSet.append(QVector3D(unAddedPoint));
        }

        //The next index for the triangles
        indices.append(index);
    }
}

/**
  Maps the bounds into local normalized coordinates
  */
QMatrix4x4 cwTriangulateTask::localNormalizedCoordinates(const QRectF &bounds) const
{
    float xScale = 1 / bounds.width();
    float yScale = 1 / bounds.height();

    QPointF topLeft = -bounds.topLeft();

    QMatrix4x4 matrix;
    matrix.scale(xScale, yScale, 1.0);
    matrix.translate(topLeft.x(), topLeft.y());

    return matrix;
}

QVector<QVector3D> cwTriangulateTask::mapToLocalNoteCoordinates(QMatrix4x4 toLocal, const QVector<QVector3D> &normalizeNoteCoords) const {
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
QVector<QVector2D> cwTriangulateTask::mapTexCoordinates(const QVector<QVector3D> &localNoteCoords) const {

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

    QSize imageSize = croppedImage.origianlSize();
    double metersPerDot = 1.0 / (double)croppedImage.originalDotsPerMeter();
    double scrapScale = 1.0 /scrapData.noteTransform().scale();

    //For right now try to map
    QMatrix4x4 toPixels;
    toPixels.scale(imageSize.width(), imageSize.height(), 1.0);

    QMatrix4x4 toMetersOnPaper;
    toMetersOnPaper.scale(metersPerDot, metersPerDot, 1.0);

    QMatrix4x4 toMetersInCave;
    toMetersInCave.scale(scrapScale, scrapScale, 1.0);

    QMatrix4x4 toWorldCoords = toMetersInCave * toMetersOnPaper * toPixels * toLocal;

//    //For testing
//    cwTriangulateStation station = scrapData.stations().first();
//    QPointF stationOnNote = toWorldCoords * station.notePosition();
//    QVector3D stationPos = station.position();

//    QMatrix4x4 offsetToFirstStation;
//    offsetToFirstStation.translate(-stationOnNote.x(), -stationOnNote.y(), 0.0);
//    offsetToFirstStation.translate(stationPos.x(), stationPos.y(), stationPos.z());

//    QMatrix4x4 noteToScene = offsetToFirstStation * toWorldCoords;

    QVector<QVector3D> points;
    points.reserve(notePoints.size());
    points.resize(notePoints.size());
    for(int i = 0; i < notePoints.size(); i++) {

        //Figure out which stations are visible to this point
        QList<cwTriangulateStation> visibleStations = stationsVisibleToPoint(notePoints[i],
                                                                             scrapData.stations(),
                                                                             scrapData.outline());

        //Based on the visible stations morph point into the scene coords
        points[i] = morphPoint(visibleStations, toWorldCoords, notePoints[i]);

//        points[i] = noteToScene.map();
    }

    //qDebug() << "Points:" << points;

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
                                                                      const QPolygonF &scrapOutline) const{

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
        QLineF ray(point2D,station.notePosition());

        //The flag if the station should be added
        bool shouldAdd = true;

        //Do all the intersections
        QPointF intersectionPoint;
        foreach(QLineF line, polygonLines) {
            if(ray.intersect(line, &intersectionPoint) == QLineF::BoundedIntersection) {
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
        if(visibleStations.size() == 1) {
            visibleStations.append(stations.first());
        }
        visibleStations.append(stations.first());
    }

    return visibleStations;
}

/**
  \brief This morphs a single point based on the stations
  that are visible to it.

  This preforms a weighted average based on distance.

  \param visibleStations - The visible stations that the point can see
  \param toWorldCoords - The matrix that can translate a note coordinate into a world coordinate (this doesn't include offset)
  \param point - The point that's going to be morphed, this is in original note coordinates
  */
QVector3D cwTriangulateTask::morphPoint(const QList<cwTriangulateStation> &visibleStations,
                                        const QMatrix4x4& toWorldCoords,
                                        const QVector3D &point)
{

    //Calculate all distances the from the visibleStations, to point in note coordinates
    QVector<double> distances;
    distances.reserve(visibleStations.size());
    int onTopOfStationIndex = -1;
    for(int i = 0; i < visibleStations.size(); i++) {
        double distance = QLineF(visibleStations[i].notePosition(), point.toPointF()).length();
        if(distance != 0.0) {

            //The inverse distance is give a high weight for points near stations
            double inverseDistance = 1.0 / (distance * distance);  //This is the function that allows the weight to be calculated correctly
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
        QPointF stationOnNote = toWorldCoords * station.notePosition(); //In world coordinates
        QVector3D stationPos = station.position(); //In world coordianets

        QMatrix4x4 offsetToFirstStation;
        offsetToFirstStation.translate(-stationOnNote.x(), -stationOnNote.y(), 0.0);
        offsetToFirstStation.translate(stationPos.x(), stationPos.y(), stationPos.z());

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

    return weightPosition;
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
