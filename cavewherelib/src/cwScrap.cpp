/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwScrap.h"
#include "cwTrip.h"
#include "cwCave.h"
#include "cwNote.h"
#include "cwDebug.h"
#include "cwLength.h"
#include "cwGlobals.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"
#include "cwPlanScrapViewMatrix.h"
#include "cwRunningProfileScrapViewMatrix.h"
#include "cwProjectedProfileScrapViewMatrix.h"
#include "cwMinimizer.h"

//Qt includes
#include <QDebug>
#include <QQuaternion>

//Std includes
#include <limits>
#include <cmath>
#include <functional>

cwScrap::cwScrap(QObject *parent) :
    QObject(parent),
    NoteTransformation(new cwNoteTranformation(this)),
    CalculateNoteTransform(false),
    ViewMatrix(nullptr),
    ParentNote(nullptr),
    ParentCave(nullptr),
    TriangulationDataDirty(false)
{
    setCalculateNoteTransform(true);
    setViewMatrix(new cwPlanScrapViewMatrix(this));

}

// cwScrap::cwScrap(const cwScrap& other)
//     : QObject(nullptr),
//       NoteTransformation(new cwNoteTranformation(this)),
//       CalculateNoteTransform(false),
//       ViewMatrix(nullptr),
//       ParentNote(nullptr),
//       ParentCave(nullptr),
//       TriangulationDataDirty(false)
// {
//     setCalculateNoteTransform(true);
//     copy(other);
// }

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
 * @brief cwScrap::hasStation
 * @param name - name of the station
 * @return True if the scrap has a reference to the station with name
 *
 * This does a linear search through the scrap and check if it has a station with name
 */
bool cwScrap::hasStation(QString name) const
{
    foreach(cwNoteStation station, stations()) {
        if(station.name().compare(name, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }

    return false;
}

/**
 * @brief cwScrap::addLead
 * @param lead
 */
void cwScrap::addLead(cwLead lead)
{
    int begin = Leads.size();
    int end = begin;
    emit leadsBeginInserted(begin, end);
    Leads.append(lead);
    emit leadsInserted(begin, end);
}

/**
 * @brief cwScrap::removeLead
 * @param leadId
 */
void cwScrap::removeLead(int leadId)
{
    Q_ASSERT(leadId >= 0);
    Q_ASSERT(leadId < Leads.size());
    emit leadsBeginRemoved(leadId, leadId);
    Leads.removeAt(leadId);
    emit leadsRemoved(leadId, leadId);
}

/**
 * @brief cwScrap::setLeads
 * @param leads
 */
void cwScrap::setLeads(QList<cwLead> leads)
{
    Leads = leads;
    emit leadsReset();
}

/**
 * @brief cwScrap::leads
 * @return
 */
QList<cwLead> cwScrap::leads() const
{
    return Leads;
}

/**
 * @brief cwScrap::leadData
 * @param role
 * @param leadIndex
 * @return
 */
QVariant cwScrap::leadData(cwScrap::LeadDataRole role, int leadIndex) const
{
    if(leadIndex < 0 || leadIndex > Leads.size()) {
        return QVariant();
    }

    const cwLead& lead = Leads.at(leadIndex);

    switch(role) {
    case LeadPosition:
        if(leadIndex < triangulationData().leadPoints().size()) {
            return triangulationData().leadPoints().at(leadIndex);
        }
        return QVector3D();
    case LeadPositionOnNote:
        return lead.positionOnNote();
    case LeadDesciption:
        return lead.desciption();
    case LeadSize:
        return lead.size();
    case LeadUnits: {
        auto calibration = parentNote()->parentTrip()->calibrations();
        return calibration->mapToSupportUnit(calibration->distanceUnit());
    }
    case LeadSupportedUnits:
        return parentNote()->parentTrip()->calibrations()->supportedUnits();
    case LeadCompleted:
        return lead.completed();
    default:
        return QVariant();
    }

    return QVariant();
}

/**
 * @brief cwScrap::setLeadData
 * @param role
 * @param leadIndex
 * @param value
 */
void cwScrap::setLeadData(cwScrap::LeadDataRole role, int leadIndex, QVariant value)
{
    if(leadIndex < 0 || leadIndex >= Leads.size()) {
        return;
    }

    if(!value.isValid()) {
        return;
    }

    QList<int> roleChanged;
    roleChanged.append(role);

    cwLead& lead = Leads[leadIndex];
    switch(role) {
    case LeadPositionOnNote:
        if(lead.positionOnNote() == value.toPointF()) { return; }
        lead.setPositionOnNote(value.toPointF());
        break;
    case LeadDesciption:
        if(lead.desciption() == value.toString()) { return; }
        lead.setDescription(value.toString());
        break;
    case LeadSize:
        if(lead.size() == value.toSizeF()) { return; }
        lead.setSize(value.toSizeF());
        break;
    case LeadCompleted:
        if(lead.completed() == value.toBool()) { return; }
        lead.setCompleted(value.toBool());
        break;
    default:
        return;
    }

    emit leadsDataChanged(leadIndex, leadIndex, roleChanged);
}

/**
 * @brief cwScrap::numberOfLeads
 * @return Returns the number of leads in a scrap
 */
int cwScrap::numberOfLeads() const
{
    return Leads.size();
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
    if(parentNote() == nullptr) {
        return;
    }

    if(!calculateNoteTransform()) {
        //User is entering note transform manually
        return;
    }

    //Get all the stations that make shots on the page of notes
    QList< QPair<cwNoteStation, cwNoteStation> > shotStations = noteShots();

    //Choose the averaging function
    std::function<cwNoteTransformationData (QList< QPair<cwNoteStation, cwNoteStation> > )> averageFunc;
    switch(type()) {
    case RunningProfile:
        averageFunc = [this](auto list)
        {
            return runningProfileAverageTransform(list);
        };
        break;
    case Plan:
        averageFunc = [this](auto list)
        {
            return projectedAverageTransform(list);
        };
        break;
    case ProjectedProfile:
        averageFunc = [this](auto list)
        {
            auto angleDiff = [](const ScrapShotTransform& t1, const ScrapShotTransform& t2) {
                auto diffError = t1.ErrorVector - t2.ErrorVector;
                return diffError.length();
            };

            auto scaleDiff = [](const ScrapShotTransform& t1, const ScrapShotTransform& t2) {
                return t1.Scale - t2.Scale;
            };

            auto stdDev = [this](auto list, auto diffFunc) {
                QList<ScrapShotTransform> transformations = calculateShotTransformations(list);
                ScrapShotTransform averageTransform = averageTransformations(transformations);

                auto stdOfValue = [=](auto diffFunc) {
                    double sum = 0.0;
                    for(auto transformation : transformations) {
                        auto diff = diffFunc(transformation, averageTransform);
                        sum += diff * diff;
                    }
                    return sqrt(sum / static_cast<double>(transformations.size()));
                };

                return stdOfValue(diffFunc);
            };

            auto matrix = static_cast<cwProjectedProfileScrapViewMatrix*>(viewMatrix());

            cwMinimizer minimzer;
            minimzer.setIncrement({10, 2, 0.4, 0.1}); //62 iteration to find best 0.1 azimuth
            minimzer.setInitialRange(0.0, 360.0);
            minimzer.setFunction([&](double azimuth) {
                    matrix->setAzimuth(azimuth);
                    return stdDev(list, angleDiff); //Min the min std devation in the angle
            });

            double bestAzimuth = minimzer.findMin();
            matrix->setAzimuth(bestAzimuth);

            return projectedAverageTransform(list);
        };
        break;

    }

    //Do the calculations
    cwNoteTransformationData averageTransformation = averageFunc(shotStations);
    NoteTransformation->setData(averageTransformation);
}

/**
  \brief Returns all the shots that are located on the page of notes

  Currently this only works for plan shots.

  This is a helper to updateNoteTransformation
  */
QList< QPair <cwNoteStation, cwNoteStation> > cwScrap::noteShots() const {

    if(Stations.size() <= 1) { return QList< QPair<cwNoteStation, cwNoteStation> >(); } //Need more than 1 station to update.
    if(parentNote() == nullptr || parentNote()->parentTrip() == nullptr) { return QList< QPair<cwNoteStation, cwNoteStation> >(); }
    if(parentCave() == nullptr) { return QList< QPair<cwNoteStation, cwNoteStation> >(); }

    //Go through all the valid stations get the
    QList<cwNoteStation> validStationList = stations(); //validStationsSet.toList();

    //This makes the assumption that station are surveyed in assending order
    //This usually helps prevent station flipping in profile mode
    std::sort(validStationList.begin(), validStationList.end(),
              [](const cwNoteStation& s1, const cwNoteStation& s2)
    {
        return s1.name().toLower() < s2.name().toLower();
    });

    //Generate all the neighbor list for each station
    QList< QStringList > stationNeighbors;
    foreach(cwNoteStation station, validStationList) {
        stationNeighbors.append(allNeighborStations(station.name()));
    }

    QList< QPair<cwNoteStation, cwNoteStation> > shotList;
    for(int i = 0; i < validStationList.size(); i++) {
        for(int j = i; j < validStationList.size(); j++) {
            cwNoteStation station1 = validStationList[i];
            cwNoteStation station2 = validStationList[j];

            //Get neigbor lookup
            auto neighborsStation1 = stationNeighbors[i];
            auto neighborsStation2 = stationNeighbors[j];

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

  The zeroVector is only for running profile calculations. The zeroVector provides a reference, where up is zero.
  */
QList< cwScrap::ScrapShotTransform > cwScrap::calculateShotTransformations(QList< QPair <cwNoteStation, cwNoteStation> > shots,
                                                                           const ProfileTransform &profileTransform) const {
    QList<ScrapShotTransform> transformations;
    for(int i = 0; i < shots.size(); i++) {
        QPair< cwNoteStation, cwNoteStation >& shot = shots[i];
        ScrapShotTransform transformation = calculateShotTransformation(shot.first, shot.second, profileTransform);
        transformations.append(transformation);
    }

    return transformations;
}

/**
  This will caluclate the transfromation between station1 and station2

  The zeroVector is only for running profile calculations. The zeroVector provides a reference, where up is zero.
  */
cwScrap::ScrapShotTransform cwScrap::calculateShotTransformation(cwNoteStation station1, cwNoteStation station2, const ProfileTransform &profileTransform) const {
    if(parentCave() == nullptr) {
        qDebug() << "Can't calculate shot transformation because parentCave is null" << LOCATION;
        return ScrapShotTransform();
    }

    cwStationPositionLookup positionLookup = parentCave()->stationPositionLookup();

    //Make sure station1 and station2 exist in the lookup
    if(!positionLookup.hasPosition(station1.name()) || !positionLookup.hasPosition(station2.name())) {
        return ScrapShotTransform();
    }

    QVector3D station1RealPos = positionLookup.position(station1.name());
    QVector3D station2RealPos = positionLookup.position(station2.name());

    //Remove the z for plan view
    switch(type()) {
    case ProjectedProfile:
        //Rotate into the correct view
        station1RealPos = viewMatrix()->matrix().map(station1RealPos);
        station2RealPos = viewMatrix()->matrix().map(station2RealPos);
    case Plan:
        station1RealPos.setZ(0.0);
        station2RealPos.setZ(0.0);
        break;
    case RunningProfile:
        //Keep the full point, because running profile keeps the full length
        break;
    }

    QVector3D station1NotePos(station1.positionOnNote()); //In normalized coordinates
    QVector3D station2NotePos(station2.positionOnNote());

    //Scale the normalized points into pixels
    QMatrix4x4 matrix = parentNote()->metersOnPageMatrix();
    station1NotePos = matrix.map(station1NotePos); //Now in meters
    station2NotePos = matrix.map(station2NotePos);

    QVector3D realVector = station2RealPos - station1RealPos; //In meters
    QVector3D noteVector = station2NotePos - station1NotePos; //In meters on page

    double lengthOnPage = noteVector.length(); //Length on page
    double lengthInCave = realVector.length(); //Length in cave


    //calculate the scale
    double scale = lengthInCave / lengthOnPage;

//    qDebug() << "Length on page:" << lengthOnPage << lengthInCave << scale << station1.name() << station2.name();

    realVector.normalize();
    noteVector.normalize();


    /**
      Calculates the shot transform in plan view between station1 and station2
      */
    auto projectedCalcTransformation = [&]()->ScrapShotTransform {
            QVector3D zeroVector(0.0, 1.0, 0.0);
            double angleToZero = acos(QVector3D::dotProduct(zeroVector, realVector)) * cwGlobals::radiansToDegrees();
            QVector3D crossProduct = QVector3D::crossProduct(zeroVector, realVector);

            QMatrix4x4 rotationToNorth;
            rotationToNorth.rotate(-angleToZero, crossProduct);

            QVector3D rotatedNoteVector = rotationToNorth.map(noteVector);
            return ScrapShotTransform(scale, rotatedNoteVector);
};

    /**
      Calculates the shot transform in running profile view between station1 and station2
      */
    auto runningProfileCalcTrasnformation = [&]()->ScrapShotTransform {
            QMatrix4x4 toNoteToWorldProfile = profileTransform.Mirror * profileTransform.Rotation;
            QVector3D afterNoteVector = toNoteToWorldProfile.map(noteVector);

            QMatrix4x4 toProfile = cwRunningProfileScrapViewMatrix::Data(station1RealPos, station2RealPos).matrix();
            QVector3D realProfileVector = toProfile.mapVector(realVector);

            //Clamp the dot product to prevent Nana
            double clinoDotProduct = std::clamp((double)QVector3D::dotProduct(realProfileVector, afterNoteVector), -1.0, 1.0);
            double clinoDiff = acos(clinoDotProduct) * cwGlobals::radiansToDegrees();

            QVector3D xAxis = profileTransform.Rotation.map(QVector3D(1.0, 0.0, 0.0));

            QQuaternion errorQuat = QQuaternion::fromAxisAndAngle(QVector3D(0.0, 0.0, 1.0), clinoDiff);
            QVector3D errorVector = errorQuat.rotatedVector(xAxis);

            // if(std::isnan(clinoDiff)) {
            //     qDebug() << "\tclineDiff is nan:" << clinoDiff << QVector3D::dotProduct(realProfileVector, afterNoteVector);
            // }
            // qDebug() << "real:" << realProfileVector << afterNoteVector << noteVector << clinoDiff << errorVector << xAxis;

            return ScrapShotTransform(scale, errorVector, clinoDiff);
};

    switch(type()) {
    case Plan:
    case ProjectedProfile:
        return projectedCalcTransformation();
    case RunningProfile:
        return runningProfileCalcTrasnformation();
    }
    return ScrapShotTransform();
}

/**
  This will average all the transformatons into one transfromation
  */
cwScrap::ScrapShotTransform cwScrap::averageTransformations(QList< ScrapShotTransform > shotTransforms) const {

    if(shotTransforms.empty()) {
        return ScrapShotTransform();
    }

    //Values to be averaged
    QVector3D errorVectorAverage;
    double scaleAverage = 0.0;

    //Number of valid transformations
    double numberValidTransforms = 0.0;

    //Sum all the values
    foreach(ScrapShotTransform transformation, shotTransforms) {
        //Make sure the note transform scale is valid
        if(transformation.Scale != 0.0) {
            errorVectorAverage += transformation.ErrorVector;
            scaleAverage += transformation.Scale;
            numberValidTransforms += 1.0;
        }
    }

    if(numberValidTransforms == 0.0) {
        qDebug() << "No valid transfroms" << LOCATION;
        return ScrapShotTransform();
    }

    //Do the averaging
    errorVectorAverage = errorVectorAverage / numberValidTransforms;
    scaleAverage = scaleAverage / numberValidTransforms;

    return ScrapShotTransform(scaleAverage, errorVectorAverage);
}

/**
  Finds the average transfrom for the plan, based on all the shots in the scrap
  */
cwNoteTransformationData cwScrap::projectedAverageTransform(QList<QPair<cwNoteStation, cwNoteStation> > shotStations) const
{
    QList<ScrapShotTransform> transformations = calculateShotTransformations(shotStations);
    auto averageTransformation = averageTransformations(transformations).toNoteTransform();
    return averageTransformation;
}

/**
  Finds the average transform for the running profile, based on all the shots in the scrap
  */
cwNoteTransformationData cwScrap::runningProfileAverageTransform(QList<QPair<cwNoteStation, cwNoteStation> > shotStations) const
{
    class ErrorTransforms {
    public:
        ErrorTransforms(double errorLength) :
            ErrorLength(errorLength),
            Rotation(0.0),
            RotationOffset(0.0)
        {}

        ErrorTransforms(double errorLength, double rotation, double rotationOffset, QList<ScrapShotTransform> transform) :
            ErrorLength(errorLength),
            Rotation(rotation),
            RotationOffset(rotationOffset),
            Transformations(transform)
        {}

        double ErrorLength;
        double Rotation;
        double RotationOffset;
        QList<ScrapShotTransform> Transformations;
    };

    /**
      Averages all the error length for a list of scrap shot transforms
      */
    auto upErrorLength = [](const QList<ScrapShotTransform>& transforms)->double {
        //Calculate the clino error
        double sumErrorAngle = 0.0;
        foreach(ScrapShotTransform transform, transforms) {
            sumErrorAngle += transform.RotationDiff * transform.RotationDiff; //Sum of the squares
        }
        return sumErrorAngle / (double)transforms.size();
    };

    auto lookupRotationOffset = [](int i)->double {
        switch(i) {
        case 0:
            return -90.0; //0 rotation
        case 1:
            return -270; //90 rotation
        case 2:
            return -90.0; //180 rotation
        case 3:
            return 90.0; //270 rotation
        default:
            return 0.0;
        }
    };

    //Seach through 0, -90 to figure out what the best orientation by mimimizing the
    //error length.
//    qDebug() << "----------------------";

    ErrorTransforms minError(std::numeric_limits<double>::max());
    for(int s = 0; s < 2; s++) {

        QMatrix4x4 xMirror;
        if(s == 1) {
            //Right to Left
            xMirror.scale(QVector3D(-1.0, 1.0, 1.0));
        }

        for(int i = 0; i < 4; i++) {
            double rotation = i * 90.0; //Rotation will be 0.0, 90, 180, or 270

            QMatrix4x4 rotationMatrix;
            rotationMatrix.rotate(rotation, QVector3D(0.0, 0.0, 1.0)); //Rotate around the z-axis

            ProfileTransform profileTransform(rotationMatrix, xMirror);

            QList<ScrapShotTransform> transformations = calculateShotTransformations(shotStations, profileTransform);
            double errorLength = upErrorLength(transformations);

//            qDebug() << "ErrorLength:" << errorLength << rotation << s;

            if(errorLength < minError.ErrorLength) {
                double rotationOffset = lookupRotationOffset(i);
                minError = ErrorTransforms(errorLength, rotation, rotationOffset, transformations);
            }
        }
    }

//    qDebug() << "Best rotation:" << minError.Rotation;

    //Using the mimimized error, find the average transform
    cwNoteTransformationData averageTransformation = averageTransformations(minError.Transformations).toNoteTransform();

//    qDebug() << "Average Transform:" << averageTransformation.northUp() << averageTransformation.scale();

    averageTransformation.north = averageTransformation.north + minError.RotationOffset;

    return averageTransformation;
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
            disconnect(noteTransformation()->scaleDenominator(), nullptr, this, nullptr);
            disconnect(noteTransformation()->scaleNumerator(), nullptr, this, nullptr);
        }

        emit calculateNoteTransformChanged();
    }
}

/**
  This guess the station based on the position on the Notes
scra
  If it doesn't know the station name.  If a station is within 5%, then it'll automatically
  update the station name.

  \param previousStation - The current station that's select.  This will only look at neighboring
  station of this station
  \param stationNotePosition - The position on this page of notes of the station that needs to be guessed

  \return An empty string if no station is within 5% of the guess
  */
QString cwScrap::guessNeighborStationName(const cwNoteStation& previousStation, QPointF stationNotePosition) {
    if(parentNote() == nullptr) { return QString(); }
    if(parentNote()->parentTrip() == nullptr) { return QString(); }
    if(parentNote()->parentTrip()->parentCave() == nullptr) { return QString(); }

    cwCave* parentCave = parentNote()->parentTrip()->parentCave();
    cwStationPositionLookup stationLookup = parentCave->stationPositionLookup();

    auto neigborStations = allNeighborStations(previousStation.name());

    //Make sure we have neigbors
    if(neigborStations.isEmpty()) {
        return QString();
    }

    //The previous Station position
    QVector3D prevStationPos = stationLookup.position(previousStation.name());

    QMatrix4x4 offsetMatrix;
    offsetMatrix.translate(-prevStationPos);

    //The matrix the maps the note station to the world coordinates
    QMatrix4x4 worldToNoteMatrix = mapWorldToNoteMatrix(previousStation);

    //The best station name
    QString bestStationName;
    double bestError = std::numeric_limits<double>::max();

//    qDebug() << "-------" << previousStation.name() << previousStation.positionOnNote() << "--------";

    /**
      Finds the amount of error for station's note position vs the prediected position. If the
      station has the lowest error, then it is the WINNER! and is set as the best station. If
      it is the lowest error of all the neighboring stations, then it returned from guessNeighborStationName()
      */
    auto updateBestStation = [&](const QString& stationName, QPointF predictedPosition) {
        //Figure out the error between stationNotePosition and predictedPosition
        QPointF errorVector = predictedPosition - stationNotePosition;
        double errorLength = QVector2D(errorVector).lengthSquared();

//        qDebug() << "PredictedPosition:" << stationName << stationNotePosition << predictedPosition << errorVector << errorLength;

        if(errorLength < bestError) {
            //Station is probably the one
            bestStationName = stationName;
            bestError = errorLength;
        }
    };

    /**
      Calculates the error using a matrix that converts the stationPosition into note coordinates
      */
    auto calcErrorUsingMatrix = [&updateBestStation](const QString& stationName, QVector3D stationPosition, QMatrix4x4 matrix) {
        //Figure out the predicited position of the station on the notes
        QPointF predictedPosition = matrix.map(stationPosition).toPointF();
        updateBestStation(stationName, predictedPosition);
    };

    /**
      Calculates the error in plan mode
      */
    auto calcErrorForProjected = [&calcErrorUsingMatrix, worldToNoteMatrix, offsetMatrix](const QString& stationName, QVector3D stationPosition) {
        calcErrorUsingMatrix(stationName, stationPosition, worldToNoteMatrix * offsetMatrix);
    };

    /**
      Calculates the error for running profile mode
      */
    auto calcErrorForRunningProfile = [&calcErrorUsingMatrix, worldToNoteMatrix, prevStationPos, offsetMatrix](const QString& stationName, QVector3D stationPosition) {
        QMatrix4x4 worldToProfileNoteMatrix;

        //Rotate into a profile
        QMatrix4x4 profileRotation = cwRunningProfileScrapViewMatrix::Data(prevStationPos, stationPosition).matrix();

        //Mirror right to left, left to right, running profile can be drawn either way
        for(int i = 0; i < 2; i++) {
            double sign = i ? 1.0 : -1.0;
            QMatrix4x4 mirrorMatrix;
            mirrorMatrix.scale(sign, 1.0, 1.0); //Mirror along the y-axis

            //Rotate into profile, then mirror, then convert to note coordinates
            worldToProfileNoteMatrix = worldToNoteMatrix * mirrorMatrix * profileRotation * offsetMatrix;

            calcErrorUsingMatrix(stationName, stationPosition, worldToProfileNoteMatrix);
        }
    };

    //Figure out how we are going to calculate the error, choose the function pointer
    std::function<void (const QString&, QVector3D)> calcError;
    switch(type()) {
    case ProjectedProfile:
    case Plan:
        calcError = calcErrorForProjected;
        break;
    case RunningProfile:
        calcError = calcErrorForRunningProfile;
        break;
    }

    //Calculate the error each station
    foreach(cwStation station, neigborStations) {
        if(stationLookup.hasPosition(station.name())) {
            QVector3D stationPosition = stationLookup.position(station.name());
            calcError(station.name(), stationPosition);
        }
    }

    //return the best station from the calculation
    return bestStationName;
}

/**
  This creates a QMatrix4x4 that can be used to transform a station's position in
  3D to normalize note coordinates

  This function is only valid in plan and projected profiles
  */
QMatrix4x4 cwScrap::mapWorldToNoteMatrix(const cwNoteStation& referenceStation) const {
    if(parentNote() == nullptr) { return QMatrix4x4(); }
    if(parentNote()->parentTrip() == nullptr) { return QMatrix4x4(); }
    if(parentNote()->parentTrip()->parentCave() == nullptr) { return QMatrix4x4(); }

    //Create the matrix to covert global position into note position
    QMatrix4x4 noteTransformMatrix = noteTransformation()->matrix(); //Matrix from page coordinates to cave coordinates
    noteTransformMatrix = noteTransformMatrix.inverted(); //From cave coordinates to page coordinates

    QMatrix4x4 dotsOnPageMatrix = parentNote()->metersOnPageMatrix().inverted();

    QMatrix4x4 noteStationOffset;
    noteStationOffset.translate(QVector3D(referenceStation.positionOnNote()));

    QMatrix4x4 toNormalizedNote = noteStationOffset
            * dotsOnPageMatrix
            * noteTransformMatrix
            * viewMatrix()->matrix(); //Change the view passed on the projection

    return toNormalizedNote;
}

/**
  \brief Sets the parent trip
  */
void cwScrap::setParentNote(cwNote* note) {
    if(ParentNote != note) {
        if(ParentNote != nullptr) {
            disconnect(ParentNote, nullptr, this, nullptr);
        }

        ParentNote = note;

        if(ParentNote != nullptr) {
            ParentCave = parentNote()->parentCave();
        }
    }
}



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
    QLineF::IntersectType leftType = leftNormal.intersects(leftLine, &leftIntersectionPoint);

    //Right intersection
    QPointF rightIntersectionPoint;
    QLineF::IntersectType rightType = rightNormal.intersects(rightLine, &rightIntersectionPoint);

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

// const cwScrap & cwScrap::operator =(const cwScrap &other) {
//     return copy(other);
// }

// /**
//   \brief The copy constructor for the scrap
//   */
// const cwScrap & cwScrap::copy(const cwScrap &other) {
//     if(&other == this) {
//         return *this;
//     }

//     OutlinePoints = other.OutlinePoints;
//     Stations = other.Stations;
//     Leads = other.Leads;
//     *NoteTransformation = *(other.NoteTransformation);
//     setCalculateNoteTransform(other.CalculateNoteTransform);
//     TriangulationData = other.TriangulationData;

//     setViewMatrix(other.ViewMatrix->clone());

//     emit stationsReset();

//     return *this;
// }

/**
 * Returns all the neighbor stations for station name in the scrap, by iterating through the
 * all the trips in the parent cave.
 */
QStringList cwScrap::allNeighborStations(const QString &stationName) const
{
    //If this is slow, could replace this with cwSurveyNetwork found in the parent cave
    //The survey network is calculate in a seperate th
    return parentCave()->network().neighbors(stationName);

//Below is a slower but doesn't require the threading to run
//    QSet<cwStation> neigborStations;
//    foreach(cwTrip* trip, parentCave()->trips()) {
//        QSet<cwStation> tripStations = trip->neighboringStations(stationName);
//        neigborStations.unite(tripStations);
//    }
    //    return neigborStations;
}

/**
 * Connects the view matrix to update the note transform for the scrap
 */
void cwScrap::setViewMatrix(cwAbstractScrapViewMatrix *viewMatrix)
{
    if(ViewMatrix == viewMatrix) {
        return;
    }

    ViewMatrix = viewMatrix;

    if(ViewMatrix) {
        ViewMatrix->setParent(this);

        updateNoteTransformation();

        emit typeChanged();
        emit viewMatrixChanged();
    }
}

/**
    \brief Set the parent cave for the scrap
  */
void cwScrap::setParentCave(cwCave *cave) {
    if(cave != ParentCave) {
        ParentCave = cave;
    }
}

/**
  \brief Sets the triangulation data
  */
void cwScrap::setTriangulationData(cwTriangulatedData data) {
    TriangulationData = data;

    QList<int> roles;
    roles.append(cwScrap::LeadPosition);

    emit leadsDataChanged(0, leads().size() - 1, roles);
}

void cwScrap::updateImage()
{
    emit pointsReset();
}

void cwScrap::setData(const cwScrapData &data)
{
    setPoints(data.outlinePoints);
    setStations(data.stations);
    setLeads(data.leads);
    NoteTransformation->setData(data.noteTransformation);
    setCalculateNoteTransform(CalculateNoteTransform);

    setType((ScrapType)data.viewMatrix->type());
    ViewMatrix->setData(data.viewMatrix->clone());
}

cwScrapData cwScrap::data() const
{
    return {
        OutlinePoints,
        Stations,
        Leads,
        NoteTransformation->data(),
        CalculateNoteTransform,
        std::unique_ptr<cwAbstractScrapViewMatrix::Data>(ViewMatrix->data()->clone())
    };
}

/**
* Sets the scrap type on how it's going to be projected
*/
void cwScrap::setType(ScrapType type) {
    if(this->type() != type) {
        cwAbstractScrapViewMatrix* newViewMatrix;

        auto createOrFromCache = [](auto& cacheViewMatrix, auto create) {
            if(!cacheViewMatrix) {
                cacheViewMatrix = create();
            }
            return cacheViewMatrix;
        };

        switch(type) {
        case Plan:
            newViewMatrix = createOrFromCache(CachedPlanViewMatrix, [this](){ return new cwPlanScrapViewMatrix(this);});
            break;
        case RunningProfile:
            newViewMatrix = createOrFromCache(CachedRunningProfileViewMatrix, [this](){ return new cwRunningProfileScrapViewMatrix(this); });
            break;
        case ProjectedProfile:
            newViewMatrix = createOrFromCache(CachedProjectedProfileViewMatrix, [this](){ return new cwProjectedProfileScrapViewMatrix(this); });
            break;
        }

        setViewMatrix(newViewMatrix);
    }
}

QStringList cwScrap::types() const {
    return {"Plan", "Running Profile", "Project Profile"};
}

cwScrap::ScrapType cwScrap::type() const {
    return (ScrapType)viewMatrix()->type();
}

cwNoteTransformationData cwScrap::ScrapShotTransform::toNoteTransform() const
{
    cwNoteTranformation transformation;
    double northAngle = transformation.calculateNorth(QPointF(0.0, 0.0), ErrorVector.toPointF());

//    qDebug() << "ErrorVectorAverage:" << errorVectorAverage;

    transformation.setNorthUp(northAngle);
    transformation.scaleNumerator()->setValue(1);
    transformation.scaleDenominator()->setValue(Scale);

    return transformation.data();
}
