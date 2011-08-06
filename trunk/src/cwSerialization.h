#ifndef CWSERIALIZATION_H
#define CWSERIALIZATION_H

//Our includes
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyNoteModel.h"
#include "cwNote.h"
#include "cwImage.h"

//Boost xml includes
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/serialization/split_free.hpp>
#include <boost/serialization/collection_traits.hpp>
#include <boost/serialization/collections_load_imp.hpp>
#include <boost/serialization/collections_save_imp.hpp>

BOOST_CLASS_VERSION(cwCavingRegion, 1)
BOOST_CLASS_VERSION(cwCave, 1)

BOOST_SERIALIZATION_SPLIT_FREE(cwCave)
BOOST_SERIALIZATION_SPLIT_FREE(cwCavingRegion)
BOOST_SERIALIZATION_SPLIT_FREE(cwStation)
BOOST_SERIALIZATION_SPLIT_FREE(cwTrip)
BOOST_SERIALIZATION_SPLIT_FREE(cwSurveyNoteModel)
BOOST_SERIALIZATION_SPLIT_FREE(cwNote)
BOOST_SERIALIZATION_SPLIT_FREE(cwImage)


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
    archive << BOOST_SERIALIZATION_NVP(image);
}

template<class Archive>
void load(Archive &archive, cwNote &note, const unsigned int) {
    //Load the notes
    cwImage image;
    archive >> BOOST_SERIALIZATION_NVP(image);
    note.setImage(image);
}

///////////////////////////// cwImage ///////////////////////////////
template<class Archive>
void save(Archive &archive, const cwImage &image, const unsigned int) {
    int originalId = image.original();
    int iconId = image.icon();
    QList<int> mipmapIds = image.mipmaps();

    archive << BOOST_SERIALIZATION_NVP(originalId);
    archive << BOOST_SERIALIZATION_NVP(iconId);
    archive << BOOST_SERIALIZATION_NVP(mipmapIds);
}

template<class Archive>
void load(Archive &archive, cwImage &image, const unsigned int) {

    int originalId;
    int iconId;
    QList<int> mipmapIds;

    archive >> BOOST_SERIALIZATION_NVP(originalId);
    archive >> BOOST_SERIALIZATION_NVP(iconId);
    archive >> BOOST_SERIALIZATION_NVP(mipmapIds);

    image.setOriginal(originalId);
    image.setIcon(iconId);
    image.setMipmaps(mipmapIds);
}

}
}

#endif // CWQTSERIALIZATION_H



