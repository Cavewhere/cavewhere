/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwTrip.h"
#include "cwCave.h"
#include "cwSurveyChunk.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"
#include "cwSurveyNoteModel.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwSurveyNoteSketchModel.h"
#include "cwErrorModel.h"
#include "cwData.h"
#include "cwKeywordModel.h"
#include "cwNameUtils.h"

//Qt includes
#include <QDebug>
#include <QMap>
#include <QUuid>

cwTrip::cwTrip(QObject *parent) :
    QObject(parent),
    ParentCave(nullptr)
{
//    DistanceUnit = cwUnits::Meters;
    Team = new cwTeam(this);
    Calibration = new cwTripCalibration(this);
    DateTime = QDateTime(QDate::currentDate(), QTime());
    Notes = new cwSurveyNoteModel(this);
    NotesLidar = new cwSurveyNoteLiDARModel(this);
    NotesSketch = new cwSurveyNoteSketchModel(this);
    ErrorModel = new cwErrorModel(this);
    KeywordModel = new cwKeywordModel(this);
    Id = QUuid::createUuid();

    // After ErrorModel exists — setParentTrip registers the calibration's
    // child error model under this trip's, so its declination warnings
    // aggregate into the trip's warningCount.
    Calibration->setParentTrip(this);

    Notes->setParentTrip(this);
    NotesLidar->setParentTrip(this);
    NotesSketch->setParentTrip(this);

    KeywordModel->addExtension(Team->keywordModel());
    updateKeywordMetadata();

//    qDebug() << "Creating:" << this;
}

// void cwTrip::Copy(const cwTrip& object)
// {
//     ParentCave = nullptr;

//     //Copy the name of the trip
//     setName(object.Name);
//     setDate(object.DateTime);

//     //Copy the team
//     Team = new cwTeam(*(object.Team));
//     Team->setParent(this);

//     //Copy the calibration
//     Calibration = new cwTripCalibration(*(object.Calibration));
//     Calibration->setParent(this);

//     //Copy the notes model
//     Notes = new cwSurveyNoteModel(*(object.Notes));
//     Notes->setParentTrip(this);

//     ErrorModel = new cwErrorModel(this);

//     //Remove all the originals
//     int lastChunkIndex = Chunks.size() - 1;
//     Chunks.clear();
//     emit chunksRemoved(0, lastChunkIndex);

//     //Copy the chunks
//     Chunks.reserve(object.Chunks.size());
//     for(int i = 0; i < object.Chunks.size(); i++) {
//         cwSurveyChunk* objectsChunk = object.Chunks[i];
//         cwSurveyChunk* newChunk = new cwSurveyChunk(*objectsChunk);
//         newChunk->setParent(this);
//         newChunk->setParentTrip(this);
//         newChunk->errorModel()->setParentModel(ErrorModel);
//         Chunks.append(newChunk);
//     }
//     emit chunksInserted(0, object.Chunks.size() - 1);

// }

// /**
//   \brief Copy constructor
//   */
// cwTrip::cwTrip(const cwTrip& object)
//     : QObject(nullptr), cwUndoer()
// {
//     Copy(object);
// }

// /**
//   \brief Assignment operator
//   */
// cwTrip& cwTrip::operator=(const cwTrip& object) {
//     if(&object == this) { return *this; }
//     Copy(object);
//     return *this;
// }

cwTrip::~cwTrip()
{
//    qDebug() << "Deleting trip:" << this;
}

/**
  \brief Set's the name of the survey trip
  */
void cwTrip::setName(QString name) {
    if(name == Name) {
        return;
    }
    if (!validateName(name).isEmpty()) {
        return;
    }
    pushUndo(new NameCommand(this, name));
}

QString cwTrip::validateName(const QString& proposedName) const
{
    const auto* nameSet = parentCave() ? &parentCave()->tripNameSet() : nullptr;
    return cwNameUtils::validateEntityName(Name, proposedName, nameSet,
                                           QStringLiteral("trip"));
}

void cwTrip::setExternalCenterline(const cwExternalCenterline& value)
{
    if (m_externalCenterline == value) {
        return;
    }
    m_externalCenterline = value;
    emit externalCenterlineChanged();
}

void cwTrip::setId(const QUuid& id)
{
    if (!id.isNull()) {
        Id = id;
    } else if (Id.isNull()) {
        Id = QUuid::createUuid();
    }
}

/**
  Sets the date of the trip
  */
void cwTrip::setDate(QDateTime date) {
    if (date == DateTime) {
        return;
    }
    const QDateTime normalized = date.isValid()
        ? QDateTime(date.date(), QTime())
        : QDateTime();
    pushUndo(new DateCommand(this, normalized));
}


// /**
//   Sets the team for the trip
//   */
// void cwTrip::setTeam(cwTeam* team) {
//     if(team != Team) {
//         if(Team != nullptr)  {
//             Team->deleteLater(); //Delete the old team
//         }

//         Team = team;

//         if(Team != nullptr) {
//             Team->setParent(this);
//         }

//         emit teamChanged();
//     }
// }

/**
  Sets the calibration for the trip
  */
// void cwTrip::setCalibration(cwTripCalibration* calibration) {
//     if(calibration != Calibration) {
//         Calibration->deleteLater();
//         Calibration = calibration;
//         Calibration->setParent(this);
//         emit calibrationChanged();
//     }
// }

/**
 * @brief cwTrip::addShotToLastChunk
 * @param fromStation
 * @param toStation
 * @param shot
 *
 * This is a convenance functon for adding shots to the trip
 */
void cwTrip::addShotToLastChunk(const cwStation &fromStation, const cwStation &toStation, const cwShot &shot)
{
    cwSurveyChunk* chunk;
    if(chunks().isEmpty()) {
        //Create a new chunk
        chunk = new cwSurveyChunk();
        addChunk(chunk);
    } else {
        chunk = Chunks.last();
    }

    if(chunk->canAddShot(fromStation, toStation)) {
        chunk->appendShot(fromStation, toStation, shot);
    } else {
        if(!chunk->isValid()) {
            //error
            qDebug() << "Can't add shot to a brand spanken new cwSurveyChunk" << fromStation.name() << toStation.name();
            return;
        }

        chunk = new cwSurveyChunk();
        addChunk(chunk);
        //Chunks.append(chunk);
        if(chunk->canAddShot(fromStation, toStation)) {
            chunk->appendShot(fromStation, toStation, shot);
        } else {
            //error
            qDebug() << "Can't add shot to a brand spanken new cwSurveyChunk" << fromStation.name() << toStation.name();
            return;
        }
    }
}


/**
  \brief Remove the chunks
  */
void cwTrip::removeChunks(int begin, int end) {
    if(end < begin) { return; }

    //Clamp the end and the beginning
    begin = qMax(0, begin);
    end = qMin(end, chunkCount());

    emit chunksAboutToBeRemoved(begin, end);

    for(int i = end; i >= begin; i--) {
        Chunks.at(i)->deleteLater();
        Chunks.removeAt(i);
    }

    emit chunksRemoved(begin, end);
    emit numberOfChunksChanged();
}

/**
  \brief Insert the chunk at row

  The chunk must be valid
  */
void cwTrip::insertChunk(int row, cwSurveyChunk* chunk) {
    if(chunk == nullptr) { return; }
    if(row < 0) { row = 0; }
    if(row > Chunks.size()) { row = Chunks.size(); }

    //Make this own the chunk
    chunk->setParentTrip(this);
    chunk->errorModel()->setParentModel(errorModel());
    Chunks.insert(row, chunk);

    emit chunksInserted(row, row);
    emit numberOfChunksChanged();
}

/**
  \brief Adds the chunk to the trip
  */
 void cwTrip::addChunk(cwSurveyChunk* chunk) {
     insertChunk(chunkCount(), chunk);
 }

 /**
    This creates a new chunk that has one shot and two stations.

    This will append the chunk to the end of the chunk list
   */
 void cwTrip::addNewChunk() {
     cwSurveyChunk* chunk = new cwSurveyChunk(this);
     chunk->appendNewShot(); //Creates a valid chunk
     addChunk(chunk);
 }

 /**
  * @brief cwTrip::removeChunk
  * @param chunk
  *
  * Search through all the chunks, if it find the chunk, the chuck will
  * be removed and delete, with deleteLater().  If the chunk doesn't exist
  * in this trip then this function does nothing.
  */
 void cwTrip::removeChunk(cwSurveyChunk *chunk)
 {
     int index = 0;
     foreach(cwSurveyChunk* currentChunk, chunks()) {
         if(currentChunk == chunk) {
             removeChunks(index, index);
             break;
         }
         index++;
     }
 }

/**
  \brief Set chunks for the group

  This will reparent all the chunks to this object
  */
void cwTrip::setChucks(QList<cwSurveyChunk*> chunks) {
    //Remove old chunks
    removeChunks(0, chunkCount() - 1);

    Chunks = chunks;

    foreach(cwSurveyChunk* chunk, Chunks) {
        chunk->setParentTrip(this);
        chunk->errorModel()->setParentModel(errorModel());
    }

    emit chunksInserted(0, chunkCount() - 1);
}

/**
  \brief Get's the chunk at i

  If i is out of bounds, this returns a null chunk
  */
cwSurveyChunk* cwTrip::chunk(int i) const {
    if(i < 0 || i > chunkCount()) {
        return nullptr;
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
        stationCount += chunk->stationCount();
    }
    return stationCount;
}

/**
  \brief Returns true if this trip has a station with stationName, else returns fales
  */
bool cwTrip::hasStation(QString stationName) const {
    foreach(cwSurveyChunk* chunk, Chunks) {
        if(chunk->hasStation(stationName)) {
            return true;
        }
    }
    return false;
}

/**
  \brief Returns a list of neighboring stations with of stationName, that only exist in this
  trip.

  The trip has a surveyChunks, a survey chunk has shots and stations.  If the stationName exists
  in the trip it will have neighboring stations, if there's at least one shot.
  */
QSet<cwStation> cwTrip::neighboringStations(QString stationName) const {
    QSet<cwStation> neighbors;
    foreach(cwSurveyChunk* chunk, Chunks) {
        QSet<cwStation> chunkNeighbors = chunk->neighboringStations(stationName);
        neighbors.unite(chunkNeighbors);
    }
    return neighbors;
}

/**
 * @brief cwTrip::stationPositionModelUpdated
 *
 * Called from the cwCave that the stations position model has updated
 */
// void cwTrip::stationPositionModelUpdated()
// {
//     Notes->stationPositionModelUpdated();
// }

cwTripData cwTrip::data() const
{
    QList<cwSurveyChunkData> chunks;
    chunks.resize(Chunks.size());

    return {
        Name,
        DateTime,
        Team->data(),
        Calibration->data(),
        cwData::toDataList<cwSurveyChunkData>(Chunks),
        Notes->data(),
        NotesLidar->data(),
        NotesSketch->data(),
        Id,
        m_externalCenterline
    };
}

void cwTrip::setData(const cwTripData &data)
{
    setName(data.name);
    setId(data.id);
    setDate(data.date);
    setExternalCenterline(data.externalCenterline);
    Team->setData(data.team);
    Calibration->setData(data.calibrations);
    Notes->setData(data.noteModel);
    NotesLidar->setData(data.noteLiDARModel);
    NotesSketch->setData(data.sketchModel);

    removeChunks(0, Chunks.size() - 1);
    for(const auto& chunk : data.chunks) {
        auto chunkObj = new cwSurveyChunk();
        chunkObj->setData(chunk);
        addChunk(chunkObj);
    }

    updateKeywordMetadata();
}

/**
  \brief Gets all the unique stations for a trip

  A station may come up more than once on a trip, but only returns
  unique stations for this trip.  Empty station name's aren't return in
  this list.
  */
QList< cwStation > cwTrip::uniqueStations() const {
    QMap<QString, cwStation> lookup;
    foreach(cwSurveyChunk* chunk, Chunks) {
        for(int i = 0; i < chunk->stationCount(); i++) {
            cwStation station = chunk->station(i);
            if(!station.name().isEmpty()) {
                lookup[station.name()] = station;
            }
        }
    }
    return lookup.values();
}

/**
  \brief Sets the undo stack for the child objects
  */
void cwTrip::setUndoStackForChildren() {

}

/**
  \brief Set's the parent's cave
  */
void cwTrip::setParentCave(cwCave* parentCave) {
    if(ParentCave != parentCave) {
        if(ParentCave) {
            KeywordModel->removeExtension(ParentCave->keywordModel());
        }

        ParentCave = parentCave;
        setParent(parentCave);

        if(ParentCave) {
            KeywordModel->addExtension(ParentCave->keywordModel());
        }

        // Notes->setParentCave(ParentCave);
        emit parentCaveChanged();
    }
}

void cwTrip::updateKeywordMetadata()
{
    if(!KeywordModel) {
        return;
    }

    KeywordModel->replace({cwKeywordModel::TripNameKey, Name});

    const auto year = DateTime.toString(QStringLiteral("yyyy"));
    if(!year.isEmpty()) {
        KeywordModel->replace({cwKeywordModel::YearKey, year});
    }

    const auto fullDate = DateTime.toString(QStringLiteral("yyyy-MM-dd"));
    if(!fullDate.isEmpty()) {
        KeywordModel->replace({cwKeywordModel::DateKey, fullDate});
    }
}

/**
 * Keyword model for this trip's line plot. Carries Type="Line Plot" and extends
 * the trip's own keyword model, so the centerline filters both as "Line Plot"
 * and alongside its trip (Trip, Year, Date, Cave, Caver, ...). Created lazily on
 * first use; both the line plot geometry's and the station labels' keyword items
 * reference this rather than rebuilding it, so geometry and labels share one
 * trip-owned identity. Kept off keywordModel() itself because scraps/notes/leads
 * extend that model and would otherwise inherit "Line Plot".
 */
cwKeywordModel* cwTrip::linePlotKeywordModel()
{
    if(m_linePlotKeywordModel == nullptr) {
        m_linePlotKeywordModel = new cwKeywordModel(this);
        m_linePlotKeywordModel->add(cwKeyword(cwKeywordModel::TypeKey, QStringLiteral("Line Plot")));
        m_linePlotKeywordModel->addExtension(KeywordModel);
    }
    return m_linePlotKeywordModel;
}

cwTrip::NameCommand::NameCommand(cwTrip* trip, QString name) {
    Trip = trip;
    NewName = name;
    OldName = Trip->name();
    setText(QString("Change trip's name to %1").arg(name));
}

void cwTrip::NameCommand::redo() {
    if (auto* cave = Trip->parentCave()) {
        if (cave->trips().contains(Trip)) {
            cave->tripNameSet().rename(OldName, NewName);
        }
    }
    Trip->Name = NewName;
    Trip->updateKeywordMetadata();
    emit Trip->nameChanged();
}

void cwTrip::NameCommand::undo() {
    if (auto* cave = Trip->parentCave()) {
        if (cave->trips().contains(Trip)) {
            cave->tripNameSet().rename(NewName, OldName);
        }
    }
    Trip->Name = OldName;
    Trip->updateKeywordMetadata();
    emit Trip->nameChanged();
}

cwTrip::DateCommand::DateCommand(cwTrip* trip, QDateTime date) {
    Trip = trip;
    NewDate = date;
    OldDate = Trip->date();
    setText(QString("Change trip's date to %1").arg(NewDate.toString(Qt::TextDate)));
}

void cwTrip::DateCommand::redo() {
    Trip->DateTime = NewDate;
    Trip->updateKeywordMetadata();
    emit Trip->dateChanged(Trip->DateTime);
}

void cwTrip::DateCommand::undo() {
    Trip->DateTime = OldDate;
    Trip->updateKeywordMetadata();
    emit Trip->dateChanged(Trip->DateTime);
}

/**
  \brief Get's all the stations for the trip
  */
QList<cwStation> cwTrip::stations() const {
    QList<cwStation> stations;
    foreach(cwSurveyChunk* chunk, chunks()) {
        stations.append(chunk->stations());
    }
    return stations;
}
