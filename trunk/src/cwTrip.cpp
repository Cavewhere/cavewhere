//Our includes
#include "cwTrip.h"
#include "cwCave.h"
#include "cwSurveyChunk.h"
#include "cwStationReference.h"

//Qt includes
#include <QMap>

cwTrip::cwTrip(QObject *parent) :
    QObject(parent),
    ParentCave(NULL)
{
}

void cwTrip::Copy(const cwTrip& object) {
    //Copy the name of the trip
    setName(object.Name);

    //Remove all the originals
    int lastChunkIndex = Chunks.size() - 1;
    Chunks.clear();
    emit chunksRemoved(0, lastChunkIndex);

    //Copy the chunks
    Chunks.reserve(object.Chunks.size());
    for(int i = 0; i < object.Chunks.size(); i++) {
        cwSurveyChunk* objectsChunk = object.Chunks[i];
        cwSurveyChunk* newChunk = new cwSurveyChunk(*objectsChunk);
        newChunk->setParent(this);
        Chunks.append(newChunk);
    }
    emit chunksInserted(0, object.Chunks.size() - 1);

}

/**
  \brief Copy constructor
  */
cwTrip::cwTrip(const cwTrip& object)
    : QObject(NULL)
{
    Copy(object);
}

/**
  \brief Assignment operator
  */
cwTrip& cwTrip::operator=(const cwTrip& object) {
    if(&object == this) { return *this; }
    Copy(object);
    return *this;
}

/**
  \brief Set's the name of the survey trip
  */
void cwTrip::setName(QString name) {
    if(name != Name) {
        Name = name;
        emit nameChanged(Name);
    }
}

/**
  \brief Gets the number of chunks in a trip
  */
int cwTrip::numberOfChunks() const {
    return Chunks.size();
}

//int cwTrip::rowCount(const QModelIndex &/*parent*/) const {
//    return Chunks.size();
//}

//QVariant cwTrip::data(const QModelIndex &index, int role) const {
//    if(!index.isValid()) { return QVariant(); }
//    if(role == ChunkRole) {
//        return QVariant::fromValue(qobject_cast<QObject*>(Chunks[index.row()]));
//    }
//    return QVariant();
//}

///**
//  \brief Insert rows to a chunk group

//  This is reimplimentation of the model in the model view frame work

//  Rows must always be added to the root item
//  */
//bool cwTrip::insertRows ( int row, int count, const QModelIndex & parent) {
//    if(parent != QModelIndex()) { return false; }
//    if(row < 0 || row > rowCount()) { return false; }

//    beginInsertRows(parent, row, row + count - 1);
//    for(int i = 0; i < count; i++) {
//        cwSurveyChunk* newChunk = new cwSurveyChunk(this);
//        newChunk->AppendNewShot();

//        Chunks.insert(row, newChunk);
//    }
//    endInsertRows();
//    return true;
//}

/**
  \brief Remove the chunks
  */
void cwTrip::removeChunks(int begin, int end) {
    if(end < begin) { return; }

    //Clamp the end and the beginning
    begin = qMax(0, begin);
    end = qMin(end, numberOfChunks());

    for(int i = end; i >= begin; i--) {
        Chunks.at(i)->deleteLater();
        Chunks.removeAt(i);
    }

    emit chunksRemoved(begin, end);
}

/**
  \brief Insert the chunk at row

  The chunk must be valid
  */
void cwTrip::insertChunk(int row, cwSurveyChunk* chunk) {
    if(chunk == NULL) { return; }
    if(row < 0) { row = 0; }
    if(row > Chunks.size()) { row = Chunks.size(); }

    //Make this own the chunk
    chunk->setParentTrip(this);

    Chunks.insert(row, chunk);

    emit chunksInserted(row, row);
}

/**
  \brief Adds the chunk to the trip
  */
 void cwTrip::addChunk(cwSurveyChunk* chunk) {
     insertChunk(numberOfChunks(), chunk);
 }

/**
  \brief Set chunks for the group

  This will reparent all the chunks to this object
  */
void cwTrip::setChucks(QList<cwSurveyChunk*> chunks) {
    //Remove old chunks
    removeChunks(0, numberOfChunks() - 1);

    Chunks = chunks;

    foreach(cwSurveyChunk* chunk, Chunks) {
        chunk->setParentTrip(this);
    }

    emit chunksInserted(0, numberOfChunks() - 1);
}

/**
  \brief Get's the chunk at i

  If i is out of bounds, this returns a null chunk
  */
cwSurveyChunk* cwTrip::chunk(int i) const {
    if(i < 0 || i > numberOfChunks()) {
        return NULL;
    }
    return Chunks[i];
}

/**
  \brief Gets all the chunks
  */
QList<cwSurveyChunk*> cwTrip::chunks() const {
    return Chunks;
}

/**
  \brief Gets the number of stations in the trip
  */
int cwTrip::numberOfStations() const {
    int stationCount = 0;
    foreach(cwSurveyChunk* chunk, Chunks) {
        stationCount += chunk->StationCount();
    }
    return stationCount;
}

/**
  \brief Gets all the unique stations for a trip

  A station may come up more than once on a trip, but only returns
  unique stations for this trip.
  */
QList< cwStationReference* > cwTrip::uniqueStations() const {
    QMap<QString, cwStationReference*> lookup;
    foreach(cwSurveyChunk* chunk, Chunks) {
        for(int i = 0; i < chunk->StationCount(); i++) {
            cwStationReference* station = chunk->Station(i);
            lookup[station->name()] = station;
        }
    }

    return lookup.values();
}

/**
  \brief Set's the parent's cave
  */
void cwTrip::setParentCave(cwCave* parentCave) {
    if(ParentCave != parentCave) {
        ParentCave = parentCave;
        setParent(parentCave);

        foreach(cwSurveyChunk* chunk, Chunks) {
            chunk->updateStationsWithNewCave();
        }
    }
}

