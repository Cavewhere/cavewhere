#ifndef CWNOTETRANSFORMCALCULATOR_H
#define CWNOTETRANSFORMCALCULATOR_H

//Qt includes
#include <QList>
#include <QPair>
#include <QVector3D>

//Our includes
#include "cwNoteTranformation.h"
#include "cwScrapType.h"
#include "cwStationPositionLookup.h"
#include "cwRunningProfileScrapViewMatrix.h"
#include "cwSurveyNetwork.h"

class cwNoteTransformCalculator
{
public:
    cwNoteTransformCalculator() = delete;


    /**
     * @brief The ScrapShot class
     *
     * This is for averaging shot transform.  This class has the scale of the shot and the
     * QVector2D of how far off shot drawn on paper compared the shot's data.  The QVector3D
     * has no error when it's equal to (0.0, 1.0, 0.0)
     */
    struct ShotTransform {
    public:
        ShotTransform() : Scale(0.0), RotationDiff(0.0) { }
        ShotTransform(double scale, QVector3D errorVector) : Scale(scale), ErrorVector(errorVector), RotationDiff(0.0) {}
        ShotTransform(double scale, QVector3D errorVector, double rotationDiff) : Scale(scale), ErrorVector(errorVector), RotationDiff(rotationDiff) {}

        double Scale;
        QVector3D ErrorVector;
        double RotationDiff;

        cwNoteTransformationData toNoteTransform() const;
    };

    /**
     * @brief The ProfileTransform class
     *
     * This class is used to re-project the x-axis for drawing running profile. Rotation is what
     * direction up is, and mirror is left to right, or right to left (mirror on x-axis)
     */
    struct ProfileTransform {
    public:
        // ProfileTransform() {}
        // ProfileTransform(QMatrix4x4 rotation, QMatrix4x4 mirror) : Rotation(rotation), Mirror(mirror) {}

        //The type of profile transformation
        cwScrapType::Type type;

        //For running profile calculations
        QMatrix4x4 rotation;
        QMatrix4x4 mirror;

        //For rotating to the correct view, for projected profiles
        QMatrix4x4 viewMatrix;

        //Scaling model or paper to physical units
        QMatrix4x4 scalingMatrix;

        //Station locations
        cwStationPositionLookup stationPositionLookup;


    };


    template <typename NoteStation>
    static QList< QPair <NoteStation, NoteStation> > noteShots(QList<NoteStation> validStationList,
                                                             const cwSurveyNetwork& network) {
        //This makes the assumption that station are surveyed in assending order
        //This usually helps prevent station flipping in profile mode
        std::sort(validStationList.begin(), validStationList.end(),
                  [](const NoteStation& s1, const NoteStation& s2)
                  {
                      return s1.name().toLower() < s2.name().toLower();
                  });

        //Generate all the neighbor list for each station
        QList< QStringList > stationNeighbors;
        foreach(NoteStation station, validStationList) {
            stationNeighbors.append(network.neighbors(station.name()));
        }

        QList< QPair<NoteStation, NoteStation> > shotList;
        for(int i = 0; i < validStationList.size(); i++) {
            for(int j = i; j < validStationList.size(); j++) {
                NoteStation station1 = validStationList[i];
                NoteStation station2 = validStationList[j];

                //Get neigbor lookup
                auto neighborsStation1 = stationNeighbors[i];
                auto neighborsStation2 = stationNeighbors[j];

                //See if they make up a shot
                if(neighborsStation1.contains(station2.name().toLower()) && neighborsStation2.contains(station1.name().toLower())) {
                    shotList.append(QPair<NoteStation, NoteStation>(station1, station2));
                }
            }
        }

        return shotList;
    }

    /**
  Finds the average transfrom for the plan, based on all the shots in the scrap
  */
    template <typename NoteStation>
    static cwNoteTransformationData projectedAverageTransform(QList<QPair<NoteStation, NoteStation> > shotStations, const ProfileTransform &profileTransform)
    {
        QList<ShotTransform> transformations = calculateShotTransformations<NoteStation>(shotStations, profileTransform);
        auto averageTransformation = averageTransformations(transformations).toNoteTransform();
        return averageTransformation;
    }



    /**
  This will caluclate the transfromation between station1 and station2

  The zeroVector is only for running profile calculations. The zeroVector provides a reference, where up is zero.
  */
    template <typename NoteStation>
    static ShotTransform calculateShotTransformation(NoteStation station1, NoteStation station2, const ProfileTransform &profileTransform) {
        const cwStationPositionLookup& positionLookup = profileTransform.stationPositionLookup;

        //Make sure station1 and station2 exist in the lookup
        if(!positionLookup.hasPosition(station1.name()) || !positionLookup.hasPosition(station2.name())) {
            return ShotTransform();
        }

        QVector3D station1RealPos = positionLookup.position(station1.name());
        QVector3D station2RealPos = positionLookup.position(station2.name());

        QVector3D station1NotePos(station1.positionOnNote()); //In normalized coordinates
        QVector3D station2NotePos(station2.positionOnNote());

        //Scale the normalized model coordinates to meters
        station1NotePos = profileTransform.scalingMatrix.map(station1NotePos); //Now in meters
        station2NotePos = profileTransform.scalingMatrix.map(station2NotePos);

        //Remove the z for plan view
        switch(profileTransform.type) {
        case cwScrapType::ProjectedProfile:
            //Rotate into the correct view
            station1RealPos = profileTransform.viewMatrix.map(station1RealPos);
            station2RealPos = profileTransform.viewMatrix.map(station2RealPos);
        case cwScrapType::Plan:
            station1RealPos.setZ(0.0);
            station2RealPos.setZ(0.0);
            break;
        case cwScrapType::LiDAR:
            //Look down, this constrains the angle calculation to plan view
            station1RealPos.setZ(0.0);
            station2RealPos.setZ(0.0);
            station1NotePos.setZ(0.0);
            station2NotePos.setZ(0.0);
            break;
        case cwScrapType::RunningProfile:
            //Keep the full point, because running profile keeps the full length
            break;
        }

        QVector3D realVector = station2RealPos - station1RealPos; //In meters
        QVector3D noteVector = station2NotePos - station1NotePos; //In meters on page

        double lengthOnPage = noteVector.length(); //Length on page
        double lengthInCave = realVector.length(); //Length in cave

        //calculate the scale
        double scale = lengthInCave / lengthOnPage;

        realVector.normalize();
        noteVector.normalize();


        /**
      Calculates the shot transform in plan view between station1 and station2
      */
        auto projectedCalcTransformation = [=]()->ShotTransform {
            QVector3D zeroVector(0.0, 1.0, 0.0);
            double angleToZero = acos(QVector3D::dotProduct(zeroVector, realVector)) * cwGlobals::radiansToDegrees();
            QVector3D crossProduct = QVector3D::crossProduct(zeroVector, realVector);

            QMatrix4x4 rotationToNorth;
            rotationToNorth.rotate(-angleToZero, crossProduct);

            QVector3D rotatedNoteVector = rotationToNorth.map(noteVector);
            return ShotTransform(scale, rotatedNoteVector);
        };

        /**
      Calculates the shot transform in running profile view between station1 and station2
      */
        auto runningProfileCalcTrasnformation = [&]()->ShotTransform {
            QMatrix4x4 toNoteToWorldProfile = profileTransform.mirror * profileTransform.rotation;
            QVector3D afterNoteVector = toNoteToWorldProfile.map(noteVector);

            QMatrix4x4 toProfile = cwRunningProfileScrapViewMatrix::Data(station1RealPos, station2RealPos).matrix();
            QVector3D realProfileVector = toProfile.mapVector(realVector);

            //Clamp the dot product to prevent Nana
            double clinoDotProduct = std::clamp((double)QVector3D::dotProduct(realProfileVector, afterNoteVector), -1.0, 1.0);
            double clinoDiff = acos(clinoDotProduct) * cwGlobals::radiansToDegrees();

            QVector3D xAxis = profileTransform.rotation.map(QVector3D(1.0, 0.0, 0.0));

            QQuaternion errorQuat = QQuaternion::fromAxisAndAngle(QVector3D(0.0, 0.0, 1.0), clinoDiff);
            QVector3D errorVector = errorQuat.rotatedVector(xAxis);

            // if(std::isnan(clinoDiff)) {
            //     qDebug() << "\tclineDiff is nan:" << clinoDiff << QVector3D::dotProduct(realProfileVector, afterNoteVector);
            // }
            // qDebug() << "real:" << realProfileVector << afterNoteVector << noteVector << clinoDiff << errorVector << xAxis;

            return ShotTransform(scale, errorVector, clinoDiff);
        };

        switch(profileTransform.type) {
        case cwScrapType::Plan:
        case cwScrapType::ProjectedProfile:
        case cwScrapType::LiDAR:
            return projectedCalcTransformation();
        case cwScrapType::RunningProfile:
            return runningProfileCalcTrasnformation();
        }
        return ShotTransform();
    }

    /**
  This will create cwNoteTransformation for each shot in the list

  The zeroVector is only for running profile calculations. The zeroVector provides a reference, where up is zero.
  */
    template <typename NoteStation>
    static QList<ShotTransform > calculateShotTransformations(QList< QPair <NoteStation, NoteStation> > shots,
                                                             const ProfileTransform &profileTransform) {
        QList<ShotTransform> transformations;
        for(int i = 0; i < shots.size(); i++) {
            QPair< NoteStation, NoteStation >& shot = shots[i];
            ShotTransform transformation = calculateShotTransformation<NoteStation>(shot.first, shot.second, profileTransform);
            transformations.append(transformation);
        }

        return transformations;
    }

    /**
  This will average all the transformatons into one transfromation
  */
    static ShotTransform averageTransformations(const QList< ShotTransform >& shotTransforms);



};

#endif // CWNOTETRANSFORMCALCULATOR_H
