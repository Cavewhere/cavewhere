/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSERIALIZATION_H
#define CWSERIALIZATION_H

//Our includes
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyNoteModel.h"
#include "cwNote.h"
#include "cwImage.h"
#include "cwTripCalibration.h"
#include "cwSurveyChunk.h"
#include "cwShot.h"
#include "cwScrap.h"
#include "cwNoteStation.h"
#include "cwNoteTranformation.h"
#include "cwLength.h"
#include "cwImageResolution.h"
#include "cwTeam.h"
#include "cwTeamMember.h"
#include "cwReadingStates.h"
#include "cwQtSerialization.h"
#include "cwTriangulatedData.h"

//Boost xml includes
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/serialization/split_free.hpp>
#include <boost/serialization/collection_traits.hpp>
#include <boost/serialization/collections_load_imp.hpp>
#include <boost/serialization/collections_save_imp.hpp>
#include <boost/serialization/binary_object.hpp>

BOOST_CLASS_VERSION(cwCavingRegion, 1)
BOOST_CLASS_VERSION(cwCave, 2)
BOOST_CLASS_VERSION(cwNote, 2)
BOOST_CLASS_VERSION(cwStation, 1)
BOOST_CLASS_VERSION(cwTrip, 1)
BOOST_CLASS_VERSION(cwSurveyNoteModel, 1)
BOOST_CLASS_VERSION(cwImage, 1)
BOOST_CLASS_VERSION(cwTripCalibration, 1)
BOOST_CLASS_VERSION(cwSurveyChunk, 1)
BOOST_CLASS_VERSION(cwStationReference, 1)
BOOST_CLASS_VERSION(cwShot, 2)
BOOST_CLASS_VERSION(cwScrap, 2)
BOOST_CLASS_VERSION(cwNoteStation, 1)
BOOST_CLASS_VERSION(cwNoteTranformation, 1)
BOOST_CLASS_VERSION(cwLength, 1)
BOOST_CLASS_VERSION(cwImageResolution, 1)
BOOST_CLASS_VERSION(cwTeam, 1)
BOOST_CLASS_VERSION(cwTeamMember, 1)
BOOST_CLASS_VERSION(cwTriangulatedData, 1)

BOOST_SERIALIZATION_SPLIT_FREE(cwCave)
BOOST_SERIALIZATION_SPLIT_FREE(cwCavingRegion)
BOOST_SERIALIZATION_SPLIT_FREE(cwStation)
BOOST_SERIALIZATION_SPLIT_FREE(cwTrip)
BOOST_SERIALIZATION_SPLIT_FREE(cwSurveyNoteModel)
BOOST_SERIALIZATION_SPLIT_FREE(cwNote)
BOOST_SERIALIZATION_SPLIT_FREE(cwImage)
BOOST_SERIALIZATION_SPLIT_FREE(cwTripCalibration)
BOOST_SERIALIZATION_SPLIT_FREE(cwSurveyChunk)
BOOST_SERIALIZATION_SPLIT_FREE(cwStationReference)
BOOST_SERIALIZATION_SPLIT_FREE(cwShot)
BOOST_SERIALIZATION_SPLIT_FREE(cwScrap)
BOOST_SERIALIZATION_SPLIT_FREE(cwNoteStation)
BOOST_SERIALIZATION_SPLIT_FREE(cwNoteTranformation)
BOOST_SERIALIZATION_SPLIT_FREE(cwLength)
BOOST_SERIALIZATION_SPLIT_FREE(cwImageResolution)
BOOST_SERIALIZATION_SPLIT_FREE(cwTeam)
BOOST_SERIALIZATION_SPLIT_FREE(cwTeamMember)
BOOST_SERIALIZATION_SPLIT_FREE(cwTriangulatedData)

/**
  All the saving and loading for all the objects
  */
namespace boost {
    namespace  serialization {

    /**
  Save and load a region
  */
    template<class Archive>
    void save(Archive& archive, const cwCavingRegion & region, const unsigned int /*version*/) {
        //Do the saving
        QList<cwCave*> caves = region.caves();
        archive << BOOST_SERIALIZATION_NVP(caves);
    }

    /**
      Load a region
      */
    template<class Archive>
    void load(Archive & archive, cwCavingRegion & region, const unsigned int /*version*/) {
        QList<cwCave*> caves;

        //Do the loading or the saving
        archive >> BOOST_SERIALIZATION_NVP(caves);

        region.clearCaves();
        region.addCaves(caves);
    }


    ////////////////////////////////// cwCave /////////////////////////////////
    template<class Archive>
    void save(Archive & archive, const cwCave& cave, const unsigned int /*version*/) {

        //Save the name
        QString name = cave.name();
        archive << BOOST_SERIALIZATION_NVP(name);

        //Save all the trips
        QList<cwTrip*> trips = cave.trips();
        archive << BOOST_SERIALIZATION_NVP(trips);

        int lengthUnit = cave.length()->unit();
        int depthUnit = cave.depth()->unit();

        archive << BOOST_SERIALIZATION_NVP(lengthUnit);
        archive << BOOST_SERIALIZATION_NVP(depthUnit);

    }

    template<class Archive>
    void load(Archive & archive, cwCave& cave, const unsigned int version) {

        //Load the name
        QString name;
        archive >> BOOST_SERIALIZATION_NVP(name);
        cave.setName(name);

        //Load all the trips
        QList<cwTrip*> trips;
        archive >> BOOST_SERIALIZATION_NVP(trips);
        foreach(cwTrip* trip, trips) {
            cave.addTrip(trip);
        }

        if(version >= 2) {
            int lengthUnit;
            int depthUnit;

            archive >> BOOST_SERIALIZATION_NVP(lengthUnit);
            archive >> BOOST_SERIALIZATION_NVP(depthUnit);

            cave.length()->setUnit(lengthUnit);
            cave.depth()->setUnit(depthUnit);
        }
    }



    /////////////////////////////// cwStation //////////////////////////////////////////////
    template<class Archive>
    void save(Archive &archive, const cwStation &station, const unsigned int) {
        QString name = station.name();
        double left = station.left();
        double right = station.right();
        double up = station.up();
        double down = station.down();
        int leftState = station.leftInputState();
        int rightState = station.rightInputState();
        int upState = station.upInputState();
        int downState = station.downInputState();

//        QVector3D position = station.position();

        archive << BOOST_SERIALIZATION_NVP(name);
        archive << BOOST_SERIALIZATION_NVP(left);
        archive << BOOST_SERIALIZATION_NVP(right);
        archive << BOOST_SERIALIZATION_NVP(up);
        archive << BOOST_SERIALIZATION_NVP(down);
        archive << BOOST_SERIALIZATION_NVP(leftState);
        archive << BOOST_SERIALIZATION_NVP(rightState);
        archive << BOOST_SERIALIZATION_NVP(upState);
        archive << BOOST_SERIALIZATION_NVP(downState);
//        archive << BOOST_SERIALIZATION_NVP(position);
    }

    template<class Archive>
    void load(Archive &archive, cwStation &station, const unsigned int) {
        QString name;
        double left;
        double right;
        double up;
        double down;
        int leftState;
        int rightState;
        int upState;
        int downState;

//        QVector3D position;

        archive >> BOOST_SERIALIZATION_NVP(name);
        archive >> BOOST_SERIALIZATION_NVP(left);
        archive >> BOOST_SERIALIZATION_NVP(right);
        archive >> BOOST_SERIALIZATION_NVP(up);
        archive >> BOOST_SERIALIZATION_NVP(down);
        archive >> BOOST_SERIALIZATION_NVP(leftState);
        archive >> BOOST_SERIALIZATION_NVP(rightState);
        archive >> BOOST_SERIALIZATION_NVP(upState);
        archive >> BOOST_SERIALIZATION_NVP(downState);
//        archive >> BOOST_SERIALIZATION_NVP(position);

        station = cwStation(name);
        station.setLeft(left);
        station.setRight(right);
        station.setUp(up);
        station.setDown(down);
        station.setLeftInputState((cwDistanceStates::State)leftState);
        station.setRightInputState((cwDistanceStates::State)rightState);
        station.setUpInputState((cwDistanceStates::State)upState);
        station.setDownInputState((cwDistanceStates::State)downState);

//        station.setPosition(position);
    }

    ///////////////////////////// cwTrip ///////////////////////////////
    template<class Archive>
    void save(Archive &archive, const cwTrip &trip, const unsigned int version) {
        Q_UNUSED(version);

        //Save the trip name
        QString name = trip.name();
        archive << BOOST_SERIALIZATION_NVP(name);

        //Save the trip date
        QDate date = trip.date();
        archive << BOOST_SERIALIZATION_NVP(date);

        //Save the trip's notes
        cwSurveyNoteModel* noteModel = trip.notes();
        archive << BOOST_SERIALIZATION_NVP(noteModel);

        //Save the trip's calibration
        cwTripCalibration* tripCalibration = trip.calibrations();
        archive << BOOST_SERIALIZATION_NVP(tripCalibration);

        //Copy the trip chunks
        QList<cwSurveyChunk*> chunks = trip.chunks();
        archive << BOOST_SERIALIZATION_NVP(chunks);

        //Save the trip's team
        cwTeam* team = trip.team();
        archive << BOOST_SERIALIZATION_NVP(team);

    }

    template<class Archive>
    void load(Archive &archive, cwTrip &trip, const unsigned int version) {
        Q_UNUSED(version);

        //Load the trip's name
        QString name;
        archive >> BOOST_SERIALIZATION_NVP(name);
        trip.setName(name);

        //Load the trip date
        QDate date;
        archive >> BOOST_SERIALIZATION_NVP(date);
        trip.setDate(date);

        //Load the trip's notes
        cwSurveyNoteModel noteModel;
        archive >> BOOST_SERIALIZATION_NVP(noteModel);
        cwSurveyNoteModel* tripNoteModel = trip.notes();
        *tripNoteModel = noteModel;
        tripNoteModel->setParentTrip(&trip);

        //Load the calibration for the notes
        cwTripCalibration tripCalibration;
        archive >> BOOST_SERIALIZATION_NVP(tripCalibration);
        cwTripCalibration* calibration = trip.calibrations();
        *calibration = tripCalibration;

        //Load all the chunks
        QList<cwSurveyChunk*> chunks;
        archive >> BOOST_SERIALIZATION_NVP(chunks);
        foreach(cwSurveyChunk* chunk, chunks) {
            trip.addChunk(chunk);
        }

        //Save the trip's team
        cwTeam* team;
        archive >> BOOST_SERIALIZATION_NVP(team);
        trip.setTeam(team);
    }

    ///////////////////////////// cwSurveyNoteModel ///////////////////////////////
    template<class Archive>
    void save(Archive &archive, const cwSurveyNoteModel &noteModel, const unsigned int) {
        //Save the notes
        QList<cwNote*> notes = noteModel.notes();
        archive << BOOST_SERIALIZATION_NVP(notes);
    }

    template<class Archive>
    void load(Archive &archive, cwSurveyNoteModel &noteModel, const unsigned int) {
        //Load the notes
        QList<cwNote*> notes;
        archive >> BOOST_SERIALIZATION_NVP(notes);
        noteModel.addNotes(notes);
    }


    ///////////////////////////// cwNote ///////////////////////////////
    template<class Archive>
    void save(Archive &archive, const cwNote &note, const unsigned int version) {
        Q_UNUSED(version)

        //Save the notes
        cwImage image = note.image();
        float rotation = note.rotate();
        QList<cwScrap*> scraps = note.scraps();
        cwImageResolution* imageResolution = note.imageResolution();

        archive << BOOST_SERIALIZATION_NVP(image);
        archive << BOOST_SERIALIZATION_NVP(rotation);
        archive << BOOST_SERIALIZATION_NVP(scraps);
        archive << BOOST_SERIALIZATION_NVP(imageResolution);
    }

    template<class Archive>
    void load(Archive &archive, cwNote &note, const unsigned int version) {
        Q_UNUSED(version);

        //Load the notes
        cwImage image;
        float rotation;
        QList<cwScrap*> scraps;

        archive >> BOOST_SERIALIZATION_NVP(image);
        archive >> BOOST_SERIALIZATION_NVP(rotation);
        archive >> BOOST_SERIALIZATION_NVP(scraps);

        note.setImage(image);
        note.setRotate(rotation);
        note.setScraps(scraps);

        //Added support for resolution
        if(version >= 2) {
            cwImageResolution* imageResolution;
            archive >> BOOST_SERIALIZATION_NVP(imageResolution);
            *(note.imageResolution()) = *imageResolution; //Copy the image resolution into the note
            delete imageResolution; //Delete the loaded image resolution
        }

    }

    ///////////////////////////// cwImage ///////////////////////////////
    template<class Archive>
    void save(Archive &archive, const cwImage &image, const unsigned int) {
        int originalId = image.original();
        int iconId = image.icon();
        QList<int> mipmapIds = image.mipmaps();
        QSize size = image.origianlSize();
        int dotPerMeter = image.originalDotsPerMeter();

        archive << BOOST_SERIALIZATION_NVP(originalId);
        archive << BOOST_SERIALIZATION_NVP(iconId);
        archive << BOOST_SERIALIZATION_NVP(mipmapIds);
        archive << BOOST_SERIALIZATION_NVP(size);
        archive << BOOST_SERIALIZATION_NVP(dotPerMeter);
    }

    template<class Archive>
    void load(Archive &archive, cwImage &image, const unsigned int) {
        int originalId;
        int iconId;
        QList<int> mipmapIds;
        QSize size;
        int dotPerMeter;

        archive >> BOOST_SERIALIZATION_NVP(originalId);
        archive >> BOOST_SERIALIZATION_NVP(iconId);
        archive >> BOOST_SERIALIZATION_NVP(mipmapIds);
        archive >> BOOST_SERIALIZATION_NVP(size);
        archive >> BOOST_SERIALIZATION_NVP(dotPerMeter);

        image.setOriginal(originalId);
        image.setOriginalSize(size);
        image.setIcon(iconId);
        image.setMipmaps(mipmapIds);
        image.setOriginalDotsPerMeter(dotPerMeter);
    }

    ////////////////////////// cwTripCalibration //////////////////////////
    template<class Archive>
    void save(Archive &archive, const cwTripCalibration &calibration, const unsigned int version) {
        Q_UNUSED(version)

        bool CorrectedCompassBacksight = calibration.hasCorrectedCompassBacksight();
        bool CorrectedClinoBacksight = calibration.hasCorrectedClinoBacksight();
        float TapeCalibration = calibration.tapeCalibration();
        float FrontCompassCalibration = calibration.frontCompassCalibration();
        float FrontClinoCalibration = calibration.frontClinoCalibration();
        float BackCompasssCalibration = calibration.backCompassCalibration();
        float BackClinoCalibration= calibration.backClinoCalibration();
        float Declination = calibration.declination();
        int DistanceUnit = (int)calibration.distanceUnit();
        bool hasFrontSights = calibration.hasFrontSights();
        bool hasBackSights = calibration.hasBackSights();

        archive << BOOST_SERIALIZATION_NVP(CorrectedCompassBacksight);
        archive << BOOST_SERIALIZATION_NVP(CorrectedClinoBacksight);
        archive << BOOST_SERIALIZATION_NVP(TapeCalibration);
        archive << BOOST_SERIALIZATION_NVP(FrontCompassCalibration);
        archive << BOOST_SERIALIZATION_NVP(FrontClinoCalibration);
        archive << BOOST_SERIALIZATION_NVP(BackCompasssCalibration);
        archive << BOOST_SERIALIZATION_NVP(BackClinoCalibration);
        archive << BOOST_SERIALIZATION_NVP(Declination);
        archive << BOOST_SERIALIZATION_NVP(DistanceUnit);
        archive << BOOST_SERIALIZATION_NVP(hasFrontSights);
        archive << BOOST_SERIALIZATION_NVP(hasBackSights);
    }

    template<class Archive>
    void load(Archive &archive, cwTripCalibration &calibration, const unsigned int version) {
        Q_UNUSED(version)

        bool CorrectedCompassBacksight = false;
        bool CorrectedClinoBacksight = false;
        float TapeCalibration = 0.0;
        float FrontCompassCalibration = 0.0;
        float FrontClinoCalibration = 0.0;
        float BackCompasssCalibration = 0.0;
        float BackClinoCalibration = 0.0;
        float Declination = 0.0;
        int DistanceUnit = cwUnits::LengthUnitless;
        bool hasFrontSights;
        bool hasBackSights;

        archive >> BOOST_SERIALIZATION_NVP(CorrectedCompassBacksight);
        archive >> BOOST_SERIALIZATION_NVP(CorrectedClinoBacksight);
        archive >> BOOST_SERIALIZATION_NVP(TapeCalibration);
        archive >> BOOST_SERIALIZATION_NVP(FrontCompassCalibration);
        archive >> BOOST_SERIALIZATION_NVP(FrontClinoCalibration);
        archive >> BOOST_SERIALIZATION_NVP(BackCompasssCalibration);
        archive >> BOOST_SERIALIZATION_NVP(BackClinoCalibration);
        archive >> BOOST_SERIALIZATION_NVP(Declination);
        archive >> BOOST_SERIALIZATION_NVP(DistanceUnit);
        archive >> BOOST_SERIALIZATION_NVP(hasFrontSights);
        archive >> BOOST_SERIALIZATION_NVP(hasBackSights);

        calibration.setCorrectedCompassBacksight(CorrectedCompassBacksight);
        calibration.setCorrectedClinoBacksight(CorrectedClinoBacksight);
        calibration.setTapeCalibration(TapeCalibration);
        calibration.setFrontCompassCalibration(FrontCompassCalibration);
        calibration.setFrontClinoCalibration(FrontClinoCalibration);
        calibration.setBackCompassCalibration(BackCompasssCalibration);
        calibration.setBackClinoCalibration(BackClinoCalibration);
        calibration.setDeclination(Declination);
        calibration.setDistanceUnit((cwUnits::LengthUnit)DistanceUnit);
        calibration.setFrontSights(hasFrontSights);
        calibration.setBackSights(hasBackSights);
    }

    //////////////////////// cwSurveyChunk ////////////////////////////////
    template<class Archive>
    void save(Archive &archive, const cwSurveyChunk &chunk, const unsigned int) {
        QList<cwStation> stations = chunk.stations();
        QList<cwShot> shots = chunk.shots();

        archive << BOOST_SERIALIZATION_NVP(stations);
        archive << BOOST_SERIALIZATION_NVP(shots);

    }

    template<class Archive>
    void load(Archive &archive, cwSurveyChunk &chunk, const unsigned int) {

        QList<cwStation> stations;
        QList<cwShot> shots;;

        archive >> BOOST_SERIALIZATION_NVP(stations);
        archive >> BOOST_SERIALIZATION_NVP(shots);

        if(stations.count() - 1 != shots.count()) {
            qDebug() << "Shot, station count mismatch, survey chunk invalid:" << stations.count() << shots.count();
            return;
        }

        for(int i = 0; i < stations.count() - 1; i++) {
            cwStation fromStation = stations[i];
            cwStation toStation = stations[i + 1];
            cwShot shot = shots[i];

            chunk.appendShot(fromStation, toStation, shot);
        }
    }

    ////////////////////////// cwShot ////////////////////////////////////////
    template<class Archive>
    void save(Archive &archive, const cwShot &shot, const unsigned int) {

        double Distance = shot.distance();
        double Compass = shot.compass();
        double BackCompass = shot.backCompass();
        double Clino = shot.clino();
        double BackClino = shot.backClino();
        int distanceState = shot.distanceState();
        int compassState = shot.compassState();
        int backCompassState = shot.backCompassState();
        int clinoState = shot.clinoState();
        int backClinoState = shot.backClinoState();
        bool includeDistance = shot.isDistanceIncluded();

        archive << BOOST_SERIALIZATION_NVP(Distance);
        archive << BOOST_SERIALIZATION_NVP(Compass);
        archive << BOOST_SERIALIZATION_NVP(BackCompass);
        archive << BOOST_SERIALIZATION_NVP(Clino);
        archive << BOOST_SERIALIZATION_NVP(BackClino);
        archive << BOOST_SERIALIZATION_NVP(distanceState);
        archive << BOOST_SERIALIZATION_NVP(compassState);
        archive << BOOST_SERIALIZATION_NVP(backCompassState);
        archive << BOOST_SERIALIZATION_NVP(clinoState);
        archive << BOOST_SERIALIZATION_NVP(backClinoState);
        archive << BOOST_SERIALIZATION_NVP(includeDistance);
    }

    template<class Archive>
    void load(Archive &archive, cwShot &shot, const unsigned int version) {

        double Distance;
        double Compass;
        double BackCompass;
        double Clino;
        double BackClino;

        int distanceState;
        int compassState;
        int backCompassState;
        int clinoState;
        int backClinoState;

        bool includeDistance;

        archive >> BOOST_SERIALIZATION_NVP(Distance);
        archive >> BOOST_SERIALIZATION_NVP(Compass);
        archive >> BOOST_SERIALIZATION_NVP(BackCompass);
        archive >> BOOST_SERIALIZATION_NVP(Clino);
        archive >> BOOST_SERIALIZATION_NVP(BackClino);
        archive >> BOOST_SERIALIZATION_NVP(distanceState);
        archive >> BOOST_SERIALIZATION_NVP(compassState);
        archive >> BOOST_SERIALIZATION_NVP(backCompassState);
        archive >> BOOST_SERIALIZATION_NVP(clinoState);
        archive >> BOOST_SERIALIZATION_NVP(backClinoState);

        shot.setDistance(Distance);
        shot.setCompass(Compass);
        shot.setBackCompass(BackCompass);
        shot.setClino(Clino);
        shot.setBackClino(BackClino);

        shot.setDistanceState((cwDistanceStates::State)distanceState);
        shot.setCompassState((cwCompassStates::State)compassState);
        shot.setBackCompassState((cwCompassStates::State)backCompassState);
        shot.setClinoState((cwClinoStates::State)clinoState);
        shot.setBackClinoState((cwClinoStates::State)backClinoState);

        if(version >= 2) {
            archive >> BOOST_SERIALIZATION_NVP(includeDistance);
            shot.setDistanceIncluded(includeDistance);
        }
    }

    ////////////////////////// cwScrap ////////////////////////////////////////
    template<class Archive>
    void save(Archive &archive, const cwScrap &scrap, const unsigned int version) {
        Q_UNUSED(version);

        QVector<QPointF> outlinePoints = scrap.points();
        QList<cwNoteStation> stations = scrap.stations();
        cwNoteTranformation* noteTransformion = scrap.noteTransformation();
        bool calulateNoteTransform = scrap.calculateNoteTransform();
        cwTriangulatedData triangleData = scrap.triangulationData();

        archive << BOOST_SERIALIZATION_NVP(outlinePoints);
        archive << BOOST_SERIALIZATION_NVP(stations);
        archive << BOOST_SERIALIZATION_NVP(noteTransformion);
        archive << BOOST_SERIALIZATION_NVP(calulateNoteTransform);

        //This can be saved optionally, data can be regenerated
        archive << BOOST_SERIALIZATION_NVP(triangleData);
    }

    template<class Archive>
    void load(Archive &archive, cwScrap &scrap, const unsigned int version) {
        QVector<QPointF> outlinePoints;
        QList<cwNoteStation> stations;
        cwNoteTranformation* noteTransformion;
        bool calulateNoteTransform;

        archive >> BOOST_SERIALIZATION_NVP(outlinePoints);
        archive >> BOOST_SERIALIZATION_NVP(stations);
        archive >> BOOST_SERIALIZATION_NVP(noteTransformion);
        archive >> BOOST_SERIALIZATION_NVP(calulateNoteTransform);

        scrap.setPoints(outlinePoints);
        scrap.setStations(stations);

        //Make a copy of the noteTransform's information
        *(scrap.noteTransformation()) = *noteTransformion;
        delete noteTransformion; //Delete it

        scrap.setCalculateNoteTransform(calulateNoteTransform);

        if(version >= 2) {
            cwTriangulatedData triangleData;
            archive >> BOOST_SERIALIZATION_NVP(triangleData);
            scrap.setTriangulationData(triangleData);
        }

    }

    ////////////////////////// cwTriangulatedData ////////////////////////////////////////
    template<class Archive>
    void save(Archive &archive, const cwTriangulatedData &triangluatedData, const unsigned int version) {
        Q_UNUSED(version);

        cwImage croppedImage = triangluatedData.croppedImage();

        QByteArray binaryData;

        {
            QDataStream dataStream(&binaryData, QIODevice::WriteOnly);
            dataStream << triangluatedData.points();
            dataStream << triangluatedData.texCoords();
            dataStream << triangluatedData.indices();
        }

        int binaryDataSize = binaryData.size();
        qDebug() << "BinaryData Save:" << binaryDataSize << triangluatedData.points().size();

        archive << BOOST_SERIALIZATION_NVP(croppedImage);
        archive << BOOST_SERIALIZATION_NVP(binaryDataSize);
        archive << make_nvp("triangleGeometryData", make_binary_object(binaryData.data(), binaryData.size()));
    }

    template<class Archive>
    void load(Archive &archive, cwTriangulatedData &triangluatedData, const unsigned int version) {
        Q_UNUSED(version);

        cwImage croppedImage;
        int binaryDataSize;
        QByteArray binaryData;

        archive >> BOOST_SERIALIZATION_NVP(croppedImage);
        archive >> BOOST_SERIALIZATION_NVP(binaryDataSize);

//        qDebug() << "Binary size:" << binaryDataSize;

        //Resize the binary data
        binaryData.resize(binaryDataSize);

        //Read in the binary data
        archive >> make_nvp("triangleGeometryData", make_binary_object(binaryData.data(), binaryData.size()));

        //Extra the binary data
        QDataStream dataStream(binaryData);

        QVector<QVector3D> points;
        QVector<QVector2D> texCoords;
        QVector<uint> indices;

        dataStream >> points;
        dataStream >> texCoords;
        dataStream >> indices;

        triangluatedData.setCroppedImage(croppedImage);
        triangluatedData.setPoints(points);
        triangluatedData.setTexCoords(texCoords);
        triangluatedData.setIndices(indices);
    }

    ////////////////////////// cwNoteStation ////////////////////////////////////////
    template<class Archive>
    void save(Archive &archive, const cwNoteStation &noteStation, const unsigned int version) {
        Q_UNUSED(version);

        QString stationName = noteStation.name();
        QPointF positionOnNote = noteStation.positionOnNote();

        archive << BOOST_SERIALIZATION_NVP(stationName);
        archive << BOOST_SERIALIZATION_NVP(positionOnNote);
    }

    template<class Archive>
    void load(Archive &archive, cwNoteStation &noteStation, const unsigned int version) {
        Q_UNUSED(version)

        QString stationName;
        QPointF positionOnNote;

        archive >> BOOST_SERIALIZATION_NVP(stationName);
        archive >> BOOST_SERIALIZATION_NVP(positionOnNote);

        noteStation.setName(stationName);
        noteStation.setPositionOnNote(positionOnNote);
    }

    ////////////////////////// cwNoteTranformation ////////////////////////////////////////
    template<class Archive>
    void save(Archive &archive, const cwNoteTranformation &transform, const unsigned int version) {
        Q_UNUSED(version);

        double northUp = transform.northUp();
        cwLength* scaleNumerator = transform.scaleNumerator();
        cwLength* scaleDenominator = transform.scaleDenominator();

        archive << BOOST_SERIALIZATION_NVP(northUp);
        archive << BOOST_SERIALIZATION_NVP(scaleNumerator);
        archive << BOOST_SERIALIZATION_NVP(scaleDenominator);
    }

    template<class Archive>
    void load(Archive &archive, cwNoteTranformation &transform, const unsigned int version) {
        Q_UNUSED(version)

        double northUp;
        cwLength* scaleNumerator;
        cwLength* scaleDenominator;

        archive >> BOOST_SERIALIZATION_NVP(northUp);
        archive >> BOOST_SERIALIZATION_NVP(scaleNumerator);
        archive >> BOOST_SERIALIZATION_NVP(scaleDenominator);

        transform.setNorthUp(northUp);
        transform.scaleNumerator()->setValue(scaleNumerator->value());
        transform.scaleNumerator()->setUnit(scaleNumerator->unit());
        transform.scaleDenominator()->setValue(scaleDenominator->value());
        transform.scaleDenominator()->setUnit(scaleDenominator->unit());
    }

    ////////////////////////// cwLength ////////////////////////////////////////
    template<class Archive>
    void save(Archive &archive, const cwLength &length, const unsigned int version) {
        Q_UNUSED(version);

        double value = length.value();
        int unit = length.unit();

        archive << BOOST_SERIALIZATION_NVP(value);
        archive << BOOST_SERIALIZATION_NVP(unit);
    }

    template<class Archive>
    void load(Archive &archive, cwLength &length, const unsigned int version) {
        Q_UNUSED(version)

        double value;
        int unit;

        archive >> BOOST_SERIALIZATION_NVP(value);
        archive >> BOOST_SERIALIZATION_NVP(unit);

        length.setValue(value);
        length.setUnit((cwUnits::LengthUnit)unit);
    }

    ////////////////////////// cwImageResolution ////////////////////////////////////////
    template<class Archive>
    void save(Archive &archive, const cwImageResolution &resolution, const unsigned int version) {
        Q_UNUSED(version);

        double value = resolution.value();
        int unit = resolution.unit();

        archive << BOOST_SERIALIZATION_NVP(value);
        archive << BOOST_SERIALIZATION_NVP(unit);
    }

    template<class Archive>
    void load(Archive &archive, cwImageResolution &resolution, const unsigned int version) {
        Q_UNUSED(version)

        double value;
        int unit;

        archive >> BOOST_SERIALIZATION_NVP(value);
        archive >> BOOST_SERIALIZATION_NVP(unit);

        resolution.setValue(value);
        resolution.setUnit((cwUnits::LengthUnit)unit);
    }

    ////////////////////////// cwTeam ////////////////////////////////////////
    template<class Archive>
    void save(Archive &archive, const cwTeam &team, const unsigned int version) {
        Q_UNUSED(version);

        QList<cwTeamMember> teamMembers = team.teamMembers();
        archive << BOOST_SERIALIZATION_NVP(teamMembers);
    }

    template<class Archive>
    void load(Archive &archive, cwTeam &team, const unsigned int version) {
        Q_UNUSED(version)

        QList<cwTeamMember> teamMembers;
        archive >> BOOST_SERIALIZATION_NVP(teamMembers);
        team.setTeamMembers(teamMembers);
    }

    ////////////////////////// cwTeamMember ////////////////////////////////////////
    template<class Archive>
    void save(Archive &archive, const cwTeamMember &member, const unsigned int version) {
        Q_UNUSED(version);

        QString name = member.name();
        QStringList jobs = member.jobs();

        archive << BOOST_SERIALIZATION_NVP(name);
        archive << BOOST_SERIALIZATION_NVP(jobs);
    }

    template<class Archive>
    void load(Archive &archive, cwTeamMember &member, const unsigned int version) {
        Q_UNUSED(version)

        QString name;
        QStringList jobs;

        archive >> BOOST_SERIALIZATION_NVP(name);
        archive >> BOOST_SERIALIZATION_NVP(jobs);

        member.setName(name);
        member.setJobs(jobs);
    }


    }
}

#endif // CWQTSERIALIZATION_H



