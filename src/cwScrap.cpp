//Our includes
#include "cwScrap.h"
#include "cwTrip.h"
#include "cwCave.h"
#include "cwNote.h"
#include "cwDebug.h"
#include "cwLength.h"
#include "cwGlobals.h"
#include "cwTrip.h"

//Qt includes
#include <QDebug>

//Std includes
#include <limits>
#include <cmath>

cwScrap::cwScrap(QObject *parent) :
    QObject(parent),
    NoteTransformation(new cwNoteTranformation(this)),
    CalculateNoteTransform(false),
    ParentNote(NULL),
    ParentCave(NULL)
{
    setCalculateNoteTransform(true);
}

cwScrap::cwScrap(const cwScrap& other)
    : QObject(NULL),
      NoteTransformation(new cwNoteTranformation(this)),
      CalculateNoteTransform(false),
      ParentNote(NULL),
      ParentCave(NULL)
{
    setCalculateNoteTransform(true);
    copy(other);
}

/**
  \brief Inserts a point in scrap
  */
void cwScrap::insertPoint(int index, QPointF point) {
    if(index < 0 || index > OutlinePoints.size()) {
        qDebug() << "Inserting point out of bounds:" << point << index << LOCATION;
    }

    bool closed = isClosed();

    OutlinePoints.insert(index, point);

    emit insertedPoints(index, index);

    //Has closed changed for the insert
    if(closed != isClosed()) {
        emit closeChanged();
    }
}

/**
  \brief Inserts a point in scrap
  */
void cwScrap::removePoint(int index) {
    if(index < 0 || index >= OutlinePoints.size()) {
        qDebug() << "Removing point out of bounds:" << index << LOCATION;
        return;
    }

    bool closed = isClosed();

    //If the polygon is a close polygon, the first point == to the last point
    int lastPointIndex = OutlinePoints.size() - 1;
    int firstPointIndex = 0;
    if((index == firstPointIndex || index == lastPointIndex) &&
            (polygon().isClosed() && OutlinePoints.size() > 1)) {

        //Remove the first and last point, they are the same
        OutlinePoints.remove(lastPointIndex);
        emit removedPoints(lastPointIndex, lastPointIndex);

        OutlinePoints.remove(firstPointIndex);
        emit removedPoints(firstPointIndex, firstPointIndex);

        //Add a new point at the end
        if(OutlinePoints.size() > 2) {
            //Make the polygon closed again
            addPoint(OutlinePoints.first());
        }

    } else if(index >= 0 && index < OutlinePoints.size()) {
        OutlinePoints.remove(index);
        emit removedPoints(index, index);
    }

    if(OutlinePoints.isEmpty()) {
        //Deleted the last point
        if(parentNote()) {
            int scrapIndex = parentNote()->indexOfScrap(this);
            parentNote()->removeScraps(scrapIndex, scrapIndex);
        }
    }

    if(closed != isClosed()) {
        emit closeChanged();
    }
}

/**
  \brief Resets all the points
  */
void cwScrap::setPoints(QVector<QPointF> points) {
    OutlinePoints = points;
    emit pointsReset();
}

/**
 * @brief cwScrap::setPoint
 * @param index - The index of the point
 * @param point - The point's geometry
 */
void cwScrap::setPoint(int index, QPointF point)
{
    if(index < 0 || index >= OutlinePoints.size()) {
        qDebug() << "Set point out of bounds:" << index << LOCATION;
        return;
    }

    if(polygon().isClosed()) {
        int lastPointIndex = OutlinePoints.size() - 1;
        int firstPointIndex = 0;
        if(index == 0) {
            //Updateh the last point, because it's the same as the first
            OutlinePoints[lastPointIndex] = point;
            emit pointChanged(lastPointIndex, lastPointIndex);
        } else if(index == lastPointIndex) {
            //Update the first point, because it's the same as the last
            OutlinePoints[firstPointIndex] = point;
            emit pointChanged(firstPointIndex, firstPointIndex);
        }
    }

    OutlinePoints[index] = point;
    emit pointChanged(index, index);
}

/**
 * @brief cwScrap::close
 *
 * Closes the scrap. If the scrap isn't already closed
 */
void cwScrap::close()
{
    if(numberOfPoints() >= 3 && !isClosed()) {
        int firstIndex = 0;
        QPointF firstPoint = OutlinePoints[firstIndex];
        addPoint(firstPoint);
    }
}

/**
  Resets all the stations in the scrap
  */
void cwScrap::setStations(QList<cwNoteStation> stations)  {
    Stations = stations;

    updateNoteTransformation();

    emit stationsReset();
}

/**
  \brief Adds a station to the note
  */
void cwScrap::addStation(cwNoteStation station) {
    Stations.append(station);
    updateNoteTransformation();

    emit stationAdded();
}

/**
  \brief Removes a station from the note

  The index needs to be valid
*/
void cwScrap::removeStation(int index) {
    if(index < 0 || index >= Stations.size()) {
        return;
    }

    Stations.removeAt(index);
    updateNoteTransformation();

    emit stationRemoved(index);
}

/**
  \brief Gets the station data at role at noteStationIndex with the role
  */
QVariant cwScrap::stationData(StationDataRole role, int noteStationIndex) const {
    //Make sure the noteStationIndex is valid
    if(noteStationIndex < 0 || noteStationIndex >= Stations.size()) {
        return QVariant();
    }

    cwNoteStation noteStation = Stations[noteStationIndex];

    switch(role) {
    case StationName:
        return noteStation.name();
    case StationPosition:
        return noteStation.positionOnNote();
    }
    return QVariant();
}

/**
  \brief Sets the station data on the note
  */
void cwScrap::setStationData(StationDataRole role, int noteStationIndex, QVariant value)  {

    //Make sure the noteStationIndex is valid
    if(noteStationIndex < 0 || noteStationIndex >= Stations.size()) {
        return;
    }

    cwNoteStation& noteStation = Stations[noteStationIndex];

    switch(role) {
    case StationName:
        if(noteStation.name() != value.toString()) {
            noteStation.setName(value.toString());
            updateNoteTransformation();
            emit stationNameChanged(noteStationIndex);
        }
        break;
    case StationPosition:
        if(noteStation.positionOnNote() != value.toPointF()) {
            QPointF clampedPosition = clampToScrap(value.toPointF());
            noteStation.setPositionOnNote(clampedPosition);
            updateNoteTransformation();
            emit stationPositionChanged(noteStationIndex, noteStationIndex);
        }
        break;
    }
}

/**
  \brief Gets the station at stationId

  stationId - The station at stationId, if the station id is invalid, this returns
  an empty cwNoteStation
  */
cwNoteStation cwScrap::station(int stationId) {
    if(stationId < 0 || stationId >= Stations.size()) { return cwNoteStation(); }
    return Stations[stationId];
}

/**
 This goes through all the station in this note and finds the note page's average transformatio.

 This find the average scale and rotatation to north, based on the station locations on the page.
  */
void cwScrap::updateNoteTransformation() {
    if(parentNote() == NULL) {
        return;
    }

    if(!calculateNoteTransform()) {
        //User is entering note transform manually
        return;
    }

    //Get all the stations that make shots on the page of notes
    QList< QPair<cwNoteStation, cwNoteStation> > shotStations = noteShots();
    QList<cwNoteTranformation> transformations = calculateShotTransformations(shotStations);
    cwNoteTranformation averageTransformation = averageTransformations(transformations);

    noteTransformation()->setScale(averageTransformation.scale());
    NoteTransformation->setNorthUp(averageTransformation.northUp());
}

/**
  \brief Returns all the shots that are located on the page of notes

  Currently this only works for plan shots.

  This is a helper to updateNoteTransformation
  */
QList< QPair <cwNoteStation, cwNoteStation> > cwScrap::noteShots() const {

    if(Stations.size() <= 1) { return QList< QPair<cwNoteStation, cwNoteStation> >(); } //Need more than 1 station to update.
    if(parentNote() == NULL || parentNote()->parentTrip() == NULL) { return QList< QPair<cwNoteStation, cwNoteStation> >(); }
    if(parentCave() == NULL) { return QList< QPair<cwNoteStation, cwNoteStation> >(); }

//    //Find the valid stations
//    QSet<cwNoteStation> validStationsSet;
//    cwStationPositionLookup stationPositionLookup = parentCave()->stationPositionLookup();
//    foreach(cwNoteStation noteStation, Stations) {
//        if(stationPositionLookup.hasPosition(noteStation)) { // && noteStation.station().cave() != NULL) {
//            validStationsSet.insert(noteStation);
//        }
//    }

    //Get the parent trip of for these notes
    cwTrip* trip = parentNote()->parentTrip();

    //Go through all the valid stations get the
    QList<cwNoteStation> validStationList = stations(); //validStationsSet.toList();

    //Generate all the neighbor list for each station
    QList< QSet< cwStation > > stationNeighbors;
    foreach(cwNoteStation station, validStationList) {
        QSet<cwStation> neighbors = trip->neighboringStations(station.name());
        stationNeighbors.append(neighbors);
    }

    QList< QPair<cwNoteStation, cwNoteStation> > shotList;
    for(int i = 0; i < validStationList.size(); i++) {
        for(int j = i; j < validStationList.size(); j++) {
            cwNoteStation station1 = validStationList[i];
            cwNoteStation station2 = validStationList[j];

            //Get neigbor lookup
            QSet< cwStation > neighborsStation1 = stationNeighbors[i];
            QSet< cwStation > neighborsStation2 = stationNeighbors[j];

            //See if they make up a shot
            if(neighborsStation1.contains(station2.name().toLower()) && neighborsStation2.contains(station1.name().toLower())) {
                shotList.append(QPair<cwNoteStation, cwNoteStation>(station1, station2));
            }
        }
    }

    return shotList;
}

/**
  This will create cwNoteTransformation for each shot in the list
  */
QList< cwNoteTranformation > cwScrap::calculateShotTransformations(QList< QPair <cwNoteStation, cwNoteStation> > shots) const {
    QList<cwNoteTranformation> transformations;
    for(int i = 0; i < shots.size(); i++) {
        QPair< cwNoteStation, cwNoteStation >& shot = shots[i];
        cwNoteTranformation transformation = calculateShotTransformation(shot.first, shot.second);
        transformations.append(transformation);
    }

    return transformations;
}

/**
  This will caluclate the transfromation between station1 and station2
  */
cwNoteTranformation cwScrap::calculateShotTransformation(cwNoteStation station1, cwNoteStation station2) const {
    if(parentCave() == NULL) {
        qDebug() << "Can't calculate shot transformation because parentCave is null" << LOCATION;
        return cwNoteTranformation();
    }

    cwStationPositionLookup positionLookup = parentCave()->stationPositionLookup();

    //Make sure station1 and station2 exist in the lookup
    if(!positionLookup.hasPosition(station1.name()) || !positionLookup.hasPosition(station2.name())) {
        return cwNoteTranformation();
    }

    QVector3D station1RealPos = positionLookup.position(station1.name());
    QVector3D station2RealPos = positionLookup.position(station2.name());

    //Remove the z for plan view
    station1RealPos.setZ(0.0);
    station2RealPos.setZ(0.0);

    QVector3D station1NotePos(station1.positionOnNote()); //In normalized coordinates
    QVector3D station2NotePos(station2.positionOnNote());

    //Scale the normalized points into pixels
    QMatrix4x4 matrix = parentNote()->metersOnPageMatrix();
    station1NotePos = matrix * station1NotePos; //Now in meters
    station2NotePos = matrix * station2NotePos;

    QVector3D realVector = station2RealPos - station1RealPos; //In meters
    QVector3D noteVector = station2NotePos - station1NotePos; //In meters on page

    double lengthOnPage = noteVector.length(); //Length on page
    double lengthInCave = realVector.length(); //Length in cave

    //calculate the scale
    double scale = lengthInCave / lengthOnPage;

    //calculate the north
    double angle = acos(QVector3D::dotProduct(realVector.normalized(), noteVector.normalized())) * cwGlobals::RadiansToDegrees;

    cwNoteTranformation noteTransform;
    noteTransform.scaleNumerator()->setValue(1);
    noteTransform.scaleDenominator()->setValue(scale);
    noteTransform.setNorthUp(angle);

    return noteTransform;
}

/**
  This will average all the transformatons into one transfromation
  */
cwNoteTranformation cwScrap::averageTransformations(QList< cwNoteTranformation > shotTransforms) {

    if(shotTransforms.empty()) {
        return cwNoteTranformation();
    }

    double angleAverage = 0.0;
    double scaleAverage = 0.0;

    foreach(cwNoteTranformation transformation, shotTransforms) {
        //Make sure the note transform scale is valid
        if(transformation.scale() != 1.0) {
            angleAverage  += transformation.northUp();
            scaleAverage += transformation.scaleDenominator()->value();
        }
    }

    angleAverage = angleAverage / (double)shotTransforms.size();
    scaleAverage = scaleAverage / (double)shotTransforms.size();

    cwNoteTranformation transformation;
    transformation.setNorthUp(angleAverage);
    transformation.scaleDenominator()->setValue(scaleAverage);

    return transformation;
}

/**
Sets calculateNoteTransform

If set to true, this will cause the scrap to automatically calculate the note
transform
*/
void cwScrap::setCalculateNoteTransform(bool calculateNoteTransform) {
    if(CalculateNoteTransform != calculateNoteTransform) {
        CalculateNoteTransform = calculateNoteTransform;

        if(CalculateNoteTransform) {
            connect(noteTransformation()->scaleDenominator(), SIGNAL(unitChanged()), SLOT(updateNoteTransformation()));
            connect(noteTransformation()->scaleNumerator(), SIGNAL(unitChanged()), SLOT(updateNoteTransformation()));
            updateNoteTransformation();
        } else {
            disconnect(noteTransformation()->scaleDenominator(), NULL, this, NULL);
            disconnect(noteTransformation()->scaleNumerator(), NULL, this, NULL);
        }

        emit calculateNoteTransformChanged();
    }
}

/**
  This guess the station based on the position on the Notes

  If it doesn't know the station name.  If a station is within 5%, then it'll automatically
  update the station name.

  This only works for the plan view, z is always 0

  \param previousStation - The current station that's select.  This will only look at neighboring
  station of this station
  \param stationNotePosition - The position on this page of notes of the station that needs to be guessed

  \return An empty string if no station is within 5% of the guess
  */
QString cwScrap::guessNeighborStationName(const cwNoteStation& previousStation, QPointF stationNotePosition) {
    if(parentNote() == NULL) { return QString(); }
    if(parentNote()->parentTrip() == NULL) { return QString(); }
    if(parentNote()->parentTrip()->parentCave() == NULL) { return QString(); }

    cwTrip* parentTrip = parentNote()->parentTrip();
    cwCave* parentCave = parentNote()->parentTrip()->parentCave();
    cwStationPositionLookup stationLookup = parentCave->stationPositionLookup();

    QSet<cwStation> neigborStations = parentTrip->neighboringStations(previousStation.name());

    //Make sure we have neigbors
    if(neigborStations.isEmpty()) {
        return QString();
    }

    //The matrix the maps the note station to the world coordinates
    QMatrix4x4 worldToNoteMatrix = mapWorldToNoteMatrix(previousStation);

    //The best station name
    QString bestStationName;
    double bestNormalizeError = 1.0;

    foreach(cwStation station, neigborStations) {
        if(stationLookup.hasPosition(station.name())) {
            QVector3D stationPosition = stationLookup.position(station.name());

            //Figure out the predicited position of the station on the notes
            QPointF predictedPosition = worldToNoteMatrix.map(stationPosition).toPointF();

            //Figure out the error between stationNotePosition and predictedPosition
            QPointF errorVector = predictedPosition - stationNotePosition;
            QPointF noteVector = previousStation.positionOnNote() - predictedPosition;
            double normalizedError = QVector2D(errorVector).length() / QVector2D(noteVector).length();

            if(normalizedError < bestNormalizeError) {
                //Station is probably the one
                bestStationName = station.name();
            }
        }
    }

    return bestStationName;
}

/**
  This creates a QMatrix4x4 that can be used to transform a station's position in
  3D to normalize note coordinates
  */
QMatrix4x4 cwScrap::mapWorldToNoteMatrix(cwNoteStation referenceStation) {
    if(parentNote() == NULL) { return QMatrix4x4(); }
    if(parentNote()->parentTrip() == NULL) { return QMatrix4x4(); }
    if(parentNote()->parentTrip()->parentCave() == NULL) { return QMatrix4x4(); }

    cwCave* parentCave = parentNote()->parentTrip()->parentCave();
    cwStationPositionLookup stationLookup = parentCave->stationPositionLookup();

    //The position of the selected station
    QVector3D stationPos = stationLookup.position(referenceStation.name());

    //Create the matrix to covert global position into note position
    QMatrix4x4 noteTransformMatrix = noteTransformation()->matrix(); //Matrix from page coordinates to cave coordinates
    noteTransformMatrix = noteTransformMatrix.inverted(); //From cave coordinates to page coordinates

    QMatrix4x4 dotsOnPageMatrix = parentNote()->metersOnPageMatrix().inverted();

    QMatrix4x4 offsetMatrix;
    offsetMatrix.translate(-stationPos);

    QMatrix4x4 noteStationOffset;
    noteStationOffset.translate(QVector3D(referenceStation.positionOnNote()));

    QMatrix4x4 toNormalizedNote = noteStationOffset *
            dotsOnPageMatrix *
            noteTransformMatrix *
            offsetMatrix;

    return toNormalizedNote;
}




/**
  \brief Sets the parent trip
  */
void cwScrap::setParentNote(cwNote* note) {
    if(ParentNote != note) {
        if(ParentNote != NULL) {
            disconnect(ParentNote, NULL, this, NULL);
        }

        ParentNote = note;

        if(ParentNote != NULL) {
            ParentCave = parentNote()->parentCave();
//            connect(ParentNote, SIGNAL(parentTripChanged()), SLOT(updateStationsWithNewCave()));
        }

//        updateStationsWithNewCave();
    }
}

//void cwScrap::updateStationsWithNewCave() {
//    for(int i = 0; i < Stations.size(); i++) {
//        Stations[i].setCave(parentCave());
//    }
//}

/**
  Clamps the point within the scrap

    If the point is in the polygon, this returns the point
  */
QPointF cwScrap::clampToScrap(QPointF point) {

    if(OutlinePoints.containsPoint(point, Qt::OddEvenFill) || OutlinePoints.isEmpty()) {
        //Point is in the polygon
        return point;
    }

    //    double bestLength = std::numeric_limits<double>::max();
    //    QPointF bestPoint;

    //    //Go through all the lines in the polygon
    //    for(int i = 0; i < OutlinePoints.size(); i++) {
    //        QPointF p1 = OutlinePoints[i];
    //        QPointF p2 = i + 1 < OutlinePoints.size() ? OutlinePoints[i + 1] : OutlinePoints[0];

    //        //Create the lines to find the intesection
    //        QLineF line(p1, p2);
    //        QLineF normal = line.normalVector().unitVector();
    //        normal.translate(-line.p1());
    //        normal.translate(point);

    //        //Do the intesection
    //        QPointF interectionPoint;
    //        QLineF::IntersectType type = normal.intersect(line, &interectionPoint);

    //        if(type != QLineF::NoIntersection && pointOnLine(line, interectionPoint)) {
    //            //This is a good line add this line
    //            double length = QLineF(interectionPoint, point).length();
    //            if(length < bestLength) {
    //                bestLength = length;
    //                bestPoint = interectionPoint;
    //            }
    //        }
    //    }

    //    //Didn't find any good lines, find the nearest point
    //    if(bestLength == std::numeric_limits<double>::max()) {
    //        int bestIndex = -1;

    //        //Find the nearest in the polygon to point
    //        for(int i = 0; i < OutlinePoints.size(); i++) {
    //            QPointF currentPoint = OutlinePoints[i];
    //            double currentLength = QLineF(currentPoint, point).length();

    //            if(currentLength < bestLength) {
    //                bestLength = currentLength;
    //                bestIndex = i;
    //            }
    //        }

    //        return OutlinePoints[bestIndex];
    //    }

    //    return bestPoint;

    //The best length and index
    double bestLength = std::numeric_limits<double>::max();
    int bestIndex = -1;

    //Find the nearest in the polygon to point
    for(int i = 0; i < OutlinePoints.size(); i++) {
        QPointF currentPoint = OutlinePoints[i];
        double currentLength = QLineF(currentPoint, point).length();

        if(currentLength < bestLength) {
            bestLength = currentLength;
            bestIndex = i;
        }
    }

    //Now get the left and right lines
    int leftIndex = bestIndex - 1;
    int rightIndex = bestIndex + 1;

    //Make sure the index are good
    if(leftIndex <= -1) {
        leftIndex = OutlinePoints.size() - 1; //wrap around
    }

    if(rightIndex >= OutlinePoints.size()) {
        rightIndex = 0; //wrap around
    }

    //Get the left and right lines
    QLineF leftLine(OutlinePoints[bestIndex], OutlinePoints[leftIndex]);
    QLineF rightLine(OutlinePoints[bestIndex], OutlinePoints[rightIndex]);

    //Get the normals for each of the left and right lines
    QLineF leftNormal = leftLine.normalVector().unitVector();
    QLineF rightNormal = rightLine.normalVector().unitVector();

    //Translate the left line and the right line
    leftNormal.translate(-leftNormal.p1());
    rightNormal.translate(-rightNormal.p2());
    leftNormal.translate(point);
    rightNormal.translate(point);

    //Left intersection
    QPointF leftIntersectionPoint;
    QLineF::IntersectType leftType = leftNormal.intersect(leftLine, &leftIntersectionPoint);

    //Right intersection
    QPointF rightIntersectionPoint;
    QLineF::IntersectType rightType = rightNormal.intersect(rightLine, &rightIntersectionPoint);

    if(leftType != QLineF::NoIntersection && pointOnLine(leftLine, leftIntersectionPoint)) {
        //Point is on the left line
        return leftIntersectionPoint;
    } else if(rightType != QLineF::NoIntersection && pointOnLine(rightLine, rightIntersectionPoint)) {
        //Point on the right line
        return rightIntersectionPoint;
    } else {
        //Point on neither
        return OutlinePoints[bestIndex];
    }
}

/**
  This true if the point is in the bounded line. and false if it outside of the bounded
  line.

  */
bool cwScrap::pointOnLine(QLineF line, QPointF point) {
    double maxX = qMax(line.p1().x(), line.p2().x());
    double minX = qMin(line.p1().x(), line.p2().x());
    double maxY = qMax(line.p1().y(), line.p2().y());
    double minY = qMin(line.p1().y(), line.p2().y());

    return point.x() >= minX && point.x() <= maxX &&
            point.y() >= minY && point.y() <= maxY;
}

const cwScrap & cwScrap::operator =(const cwScrap &other) {
    return copy(other);
}

/**
  \brief The copy constructor for the scrap
  */
const cwScrap & cwScrap::copy(const cwScrap &other) {
    if(&other == this) {
        return *this;
    }

    OutlinePoints = other.OutlinePoints;
    Stations = other.Stations;
    *NoteTransformation = *(other.NoteTransformation);
    setCalculateNoteTransform(other.CalculateNoteTransform);

    emit stationsReset();

    return *this;
}

/**
    \brief Set the parent cave for the scrap
  */
void cwScrap::setParentCave(cwCave *cave) {
    if(cave != ParentCave) {
        ParentCave = cave;
    }
}
