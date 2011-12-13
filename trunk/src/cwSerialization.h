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
#include "cwStationReference.h"
#include "cwShot.h"
#include "cwScrap.h"
#include "cwNoteStation.h"
#include "cwNoteTranformation.h"
#include "cwLength.h"

//Boost xml includes
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/serialization/split_free.hpp>
#include <boost/serialization/collection_traits.hpp>
#include <boost/serialization/collections_load_imp.hpp>
#include <boost/serialization/collections_save_imp.hpp>

BOOST_CLASS_VERSION(cwCavingRegion, 1)
BOOST_CLASS_VERSION(cwCave, 1)
BOOST_CLASS_VERSION(cwNote, 1)

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

/**
  All the saving and loading for all the objects
  */
namespace boost {
namespace  serialization {

/**
  Save and load a region
  */
template<class Archive>
void save(Archive & archive, const cwCavingRegion & region, const unsigned int /*version*/) {
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

    //Save all the stations
    QList< QWeakPointer<cwStation> > weakStations = cave.stations();
    QList< cwStation* > stations;
    foreach(QWeakPointer<cwStation> weakStation, weakStations) {
        if(!weakStation.isNull()) {
            stations.append(weakStation.data());
        }
    }
    archive << BOOST_SERIALIZATION_NVP(stations);

    //Save all the trips
    QList<cwTrip*> trips = cave.trips();
    qDebug() << "Number of trips:" << trips.size();
    archive << BOOST_SERIALIZATION_NVP(trips);

}

template<class Archive>
void load(Archive & archive, cwCave& cave, const unsigned int /*version*/) {

    //Load the name
    QString name;
    archive >> BOOST_SERIALIZATION_NVP(name);
    cave.setName(name);

    //Load the stations
    QList<cwStation*> stations;
    archive >> BOOST_SERIALIZATION_NVP(stations);
    foreach(cwStation* station, stations) {
        cave.addStation(QSharedPointer<cwStation>(station));
    }

    //Load all the trips
    QList<cwTrip*> trips;
    archive >> BOOST_SERIALIZATION_NVP(trips);
    foreach(cwTrip* trip, trips) {
        cave.addTrip(trip);
    }
}



/////////////////////////////// cwStation //////////////////////////////////////////////
template<class Archive>
void save(Archive &archive, const cwStation &station, const unsigned int) {
    QString name = station.name();
    QString left = station.left();
    QString right = station.right();
    QString up = station.up();
    QString down = station.down();
    QVector3D position = station.position();

    archive << BOOST_SERIALIZATION_NVP(name);
    archive << BOOST_SERIALIZATION_NVP(left);
    archive << BOOST_SERIALIZATION_NVP(right);
    archive << BOOST_SERIALIZATION_NVP(up);
    archive << BOOST_SERIALIZATION_NVP(down);
    archive << BOOST_SERIALIZATION_NVP(position);
}

template<class Archive>
void load(Archive &archive, cwStation &station, const unsigned int) {
    QString name;
    QString left;
    QString right;
    QString up;
    QString down;
    QVector3D position;

    archive >> BOOST_SERIALIZATION_NVP(name);
    archive >> BOOST_SERIALIZATION_NVP(left);
    archive >> BOOST_SERIALIZATION_NVP(right);
    archive >> BOOST_SERIALIZATION_NVP(up);
    archive >> BOOST_SERIALIZATION_NVP(down);
    archive >> BOOST_SERIALIZATION_NVP(position);

    station = cwStation(name);
    station.setLeft(left);
    station.setRight(right);
    station.setUp(up);
    station.setDown(down);
    station.setPosition(position);
}



///////////////////////////// cwTrip ///////////////////////////////
template<class Archive>
void save(Archive &archive, const cwTrip &trip, const unsigned int) {

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
}

template<class Archive>
void load(Archive &archive, cwTrip &trip, const unsigned int) {

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
void save(Archive &archive, const cwNote &note, const unsigned int) {
    //Save the notes
    cwImage image = note.image();
    float rotation = note.rotate();
    QList<cwScrap*> scraps = note.scraps();

    archive << BOOST_SERIALIZATION_NVP(image);
    archive << BOOST_SERIALIZATION_NVP(rotation);
    archive << BOOST_SERIALIZATION_NVP(scraps);
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
void save(Archive &archive, const cwTripCalibration &calibration, const unsigned int) {

    bool CorrectedCompassBacksight = calibration.hasCorrectedCompassBacksight();
    bool CorrectedClinoBacksight = calibration.hasCorrectedClinoBacksight();
    float TapeCalibration = calibration.tapeCalibration();
    float FrontCompassCalibration = calibration.frontCompassCalibration();
    float FrontClinoCalibration = calibration.frontClinoCalibration();
    float BackCompasssCalibration = calibration.backCompassCalibration();
    float BackClinoCalibration= calibration.backClinoCalibration();
    float Declination = calibration.declination();
    int DistanceUnit = (int)calibration.distanceUnit();

    archive << BOOST_SERIALIZATION_NVP(CorrectedCompassBacksight);
    archive << BOOST_SERIALIZATION_NVP(CorrectedClinoBacksight);
    archive << BOOST_SERIALIZATION_NVP(TapeCalibration);
    archive << BOOST_SERIALIZATION_NVP(FrontCompassCalibration);
    archive << BOOST_SERIALIZATION_NVP(FrontClinoCalibration);
    archive << BOOST_SERIALIZATION_NVP(BackCompasssCalibration);
    archive << BOOST_SERIALIZATION_NVP(BackClinoCalibration);
    archive << BOOST_SERIALIZATION_NVP(Declination);
    archive << BOOST_SERIALIZATION_NVP(DistanceUnit);
}

template<class Archive>
void load(Archive &archive, cwTripCalibration &calibration, const unsigned int) {
    bool CorrectedCompassBacksight = false;
    bool CorrectedClinoBacksight = false;
    float TapeCalibration = 0.0;
    float FrontCompassCalibration = 0.0;
    float FrontClinoCalibration = 0.0;
    float BackCompasssCalibration = 0.0;
    float BackClinoCalibration = 0.0;
    float Declination = 0.0;
    int DistanceUnit = cwUnits::Unitless;

    archive >> BOOST_SERIALIZATION_NVP(CorrectedCompassBacksight);
    archive >> BOOST_SERIALIZATION_NVP(CorrectedClinoBacksight);
    archive >> BOOST_SERIALIZATION_NVP(TapeCalibration);
    archive >> BOOST_SERIALIZATION_NVP(FrontCompassCalibration);
    archive >> BOOST_SERIALIZATION_NVP(FrontClinoCalibration);
    archive >> BOOST_SERIALIZATION_NVP(BackCompasssCalibration);
    archive >> BOOST_SERIALIZATION_NVP(BackClinoCalibration);
    archive >> BOOST_SERIALIZATION_NVP(Declination);
    archive >> BOOST_SERIALIZATION_NVP(DistanceUnit);

    calibration.setCorrectedCompassBacksight(CorrectedCompassBacksight);
    calibration.setCorrectedClinoBacksight(CorrectedClinoBacksight);
    calibration.setTapeCalibration(TapeCalibration);
    calibration.setFrontCompassCalibration(FrontCompassCalibration);
    calibration.setFrontClinoCalibration(FrontClinoCalibration);
    calibration.setBackCompassCalibration(BackCompasssCalibration);
    calibration.setBackClinoCalibration(BackClinoCalibration);
    calibration.setDeclination(Declination);
    calibration.setDistanceUnit((cwUnits::LengthUnit)DistanceUnit);
}

//////////////////////// cwSurveyChunk ////////////////////////////////
template<class Archive>
void save(Archive &archive, const cwSurveyChunk &chunk, const unsigned int) {
    QList<cwStationReference> stations = chunk.stations();
    QList<cwShot*> shots = chunk.shots();

    archive << BOOST_SERIALIZATION_NVP(stations);
    archive << BOOST_SERIALIZATION_NVP(shots);

}

template<class Archive>
void load(Archive &archive, cwSurveyChunk &chunk, const unsigned int) {

    QList<cwStationReference> stations;
    QList<cwShot*> shots;;

    archive >> BOOST_SERIALIZATION_NVP(stations);
    archive >> BOOST_SERIALIZATION_NVP(shots);

    if(stations.count() - 1 != shots.count()) {
        qDebug() << "Shot, station count mismatch, survey chunk invalid:" << stations.count() << shots.count();
        return;
    }

    for(int i = 0; i < stations.count() - 1; i++) {
        cwStationReference fromStation = stations[i];
        cwStationReference toStation = stations[i + 1];
        cwShot* shot = shots[i];

        chunk.appendShot(fromStation, toStation, shot);
    }
}

////////////////////////// cwStationReferance ////////////////////////////////////////
template<class Archive>
void save(Archive &archive, const cwStationReference &station, const unsigned int) {
    QString stationName = station.name();
    cwCave* cave = station.cave();

    archive << BOOST_SERIALIZATION_NVP(stationName);
    archive << BOOST_SERIALIZATION_NVP(cave);
}

template<class Archive>
void load(Archive &archive, cwStationReference &station, const unsigned int) {
    QString stationName;
    cwCave* cave = NULL;

    archive >> BOOST_SERIALIZATION_NVP(stationName);
    archive >> BOOST_SERIALIZATION_NVP(cave);

    station.setCave(cave);
    station.setName(stationName);
}

////////////////////////// cwShot ////////////////////////////////////////
template<class Archive>
void save(Archive &archive, const cwShot &shot, const unsigned int) {

    QString Distance = shot.distance();
    QString Compass = shot.compass();
    QString BackCompass = shot.backCompass();
    QString Clino = shot.clino();
    QString BackClino = shot.backClino();

    archive << BOOST_SERIALIZATION_NVP(Distance);
    archive << BOOST_SERIALIZATION_NVP(Compass);
    archive << BOOST_SERIALIZATION_NVP(BackCompass);
    archive << BOOST_SERIALIZATION_NVP(Clino);
    archive << BOOST_SERIALIZATION_NVP(BackClino);
}

template<class Archive>
void load(Archive &archive, cwShot &shot, const unsigned int) {

    QString Distance;
    QString Compass;
    QString BackCompass;
    QString Clino;
    QString BackClino;

    archive >> BOOST_SERIALIZATION_NVP(Distance);
    archive >> BOOST_SERIALIZATION_NVP(Compass);
    archive >> BOOST_SERIALIZATION_NVP(BackCompass);
    archive >> BOOST_SERIALIZATION_NVP(Clino);
    archive >> BOOST_SERIALIZATION_NVP(BackClino);

    shot.setDistance(Distance);
    shot.setCompass(Compass);
    shot.setBackCompass(BackCompass);
    shot.setClino(Clino);
    shot.setBackClino(BackClino);
}

////////////////////////// cwScrap ////////////////////////////////////////
template<class Archive>
void save(Archive &archive, const cwScrap &scrap, const unsigned int version) {
    Q_UNUSED(version);

    QVector<QPointF> outlinePoints = scrap.points();
    QList<cwNoteStation> stations = scrap.stations();
    cwNoteTranformation* noteTransformion = scrap.noteTransformation();
    bool calulateNoteTransform = scrap.calculateNoteTransform();

    archive << BOOST_SERIALIZATION_NVP(outlinePoints);
    archive << BOOST_SERIALIZATION_NVP(stations);
    archive << BOOST_SERIALIZATION_NVP(noteTransformion);
    archive << BOOST_SERIALIZATION_NVP(calulateNoteTransform);
}

template<class Archive>
void load(Archive &archive, cwScrap &scrap, const unsigned int version) {
    Q_UNUSED(version)

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
}

////////////////////////// cwNoteStation ////////////////////////////////////////
template<class Archive>
void save(Archive &archive, const cwNoteStation &noteStation, const unsigned int version) {
    Q_UNUSED(version);

    cwStationReference stationReference = noteStation.station();
    QPointF positionOnNote = noteStation.positionOnNote();

    archive << BOOST_SERIALIZATION_NVP(stationReference);
    archive << BOOST_SERIALIZATION_NVP(positionOnNote);
}

template<class Archive>
void load(Archive &archive, cwNoteStation &noteStation, const unsigned int version) {
    Q_UNUSED(version)

    cwStationReference stationReference;
    QPointF positionOnNote;

    archive >> BOOST_SERIALIZATION_NVP(stationReference);
    archive >> BOOST_SERIALIZATION_NVP(positionOnNote);

    noteStation.setStation(stationReference);
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
void load(Archive &archive, cwNoteTranformation &noteStation, const unsigned int version) {
    Q_UNUSED(version)

    double northUp;
    cwLength* scaleNumerator;
    cwLength* scaleDenominator;

    archive >> BOOST_SERIALIZATION_NVP(northUp);
    archive >> BOOST_SERIALIZATION_NVP(scaleNumerator);
    archive >> BOOST_SERIALIZATION_NVP(scaleDenominator);

    noteStation.setNorthUp(northUp);
    noteStation.scaleNumerator()->setValue(scaleNumerator->value());
    noteStation.scaleNumerator()->setUnit(scaleNumerator->unit());
    noteStation.scaleDenominator()->setValue(scaleDenominator->value());
    noteStation.scaleDenominator()->setUnit(scaleDenominator->unit());
}

////////////////////////// cwNoteTranformation ////////////////////////////////////////
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


}
}

#endif // CWQTSERIALIZATION_H



