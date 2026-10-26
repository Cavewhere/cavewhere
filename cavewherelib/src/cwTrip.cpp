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
    scheduleColumnDataUpdate();
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
    connectColumnData(chunk);

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

bool cwTrip::canSplitChunk(cwSurveyChunk* chunk, int shotIndex) const
{
    if(chunk == nullptr || !Chunks.contains(chunk)) {
        return false;
    }

    if(shotIndex < 1 || shotIndex >= chunk->shotCount()) {
        return false;
    }

    //An empty shot is dropped, so the lower half needs a shot after it
    if(chunk->isShotDataEmpty(shotIndex)) {
        return shotIndex <= chunk->shotCount() - 2;
    }
    return true;
}

/**
  Splits \a chunk at \a shotIndex and inserts the lower half right after it.

  The upper half keeps the chunk's id and stations 0..shotIndex. A shot with
  readings starts the new chunk under a name-only copy of its from-station; an
  empty shot is dropped. Returns the new chunk, or nullptr when canSplitChunk()
  refuses.
  */
cwSurveyChunk* cwTrip::splitChunk(cwSurveyChunk* chunk, int shotIndex)
{
    if(!canSplitChunk(chunk, shotIndex)) {
        return nullptr;
    }

    const QList<cwStation> stations = chunk->stations();
    const QList<cwShot> shots = chunk->shots();

    cwSurveyChunkData data;
    data.id = QUuid::createUuid();
    if(chunk->isShotDataEmpty(shotIndex)) {
        data.stations = stations.mid(shotIndex + 1);
        data.shots = shots.mid(shotIndex + 1);
    } else {
        data.stations.append(cwStation(stations.at(shotIndex).name()));
        data.stations.append(stations.mid(shotIndex + 1));
        data.shots = shots.mid(shotIndex);
    }

    auto newChunk = new cwSurveyChunk();
    newChunk->setData(data);

    //The lower half lands in the trip before the upper one lets go of it, so
    //listeners always see every shot somewhere
    insertChunk(Chunks.indexOf(chunk) + 1, newChunk);
    chunk->removeTail(shotIndex + 1);

    return newChunk;
}

/**
  True when chunk \a upperIndex and the one below it share a tie station: the
  upper chunk's last station and the lower chunk's first station, per isTie().
  */
bool cwTrip::canMergeChunks(int upperIndex) const
{
    if(upperIndex < 0 || upperIndex >= Chunks.size() - 1) {
        return false;
    }

    const cwSurveyChunk* upper = Chunks.at(upperIndex);
    const cwSurveyChunk* lower = Chunks.at(upperIndex + 1);
    if(upper->stationCount() == 0 || lower->stationCount() == 0) {
        return false;
    }

    return isTie(upper->station(upper->stationCount() - 1).name(), lower->station(0).name());
}

/**
  True when both names are non-empty and match in any case
  */
bool cwTrip::isTie(const QString& upperLastName, const QString& lowerFirstName)
{
    return !upperLastName.isEmpty()
           && !lowerFirstName.isEmpty()
           && upperLastName.compare(lowerFirstName, Qt::CaseInsensitive) == 0;
}

/**
  Joins chunk \a upperIndex + 1 onto chunk \a upperIndex and removes it.

  The upper chunk keeps its id and its copy of the tie station. Each LRUD the
  upper copy lacks comes from the lower copy, and the lower copy's splays
  follow the upper copy's. Does nothing when canMergeChunks() refuses.
  */
void cwTrip::mergeChunks(int upperIndex)
{
    if(!canMergeChunks(upperIndex)) {
        return;
    }

    cwSurveyChunk* upper = Chunks.at(upperIndex);
    const cwSurveyChunk* lower = Chunks.at(upperIndex + 1);

    const int tieIndex = upper->stationCount() - 1;
    cwStation tie = upper->station(tieIndex);
    const cwStation lowerTie = lower->station(0);

    using Reading = cwDistanceReading (cwStation::*)() const;
    using SetReading = void (cwStation::*)(const cwDistanceReading&);
    bool tieChanged = false;
    const auto fillEmpty = [&](Reading reading, SetReading setReading) {
        if((tie.*reading)().state() == cwDistanceReading::State::Empty
                && (lowerTie.*reading)().state() != cwDistanceReading::State::Empty) {
            (tie.*setReading)((lowerTie.*reading)());
            tieChanged = true;
        }
    };
    fillEmpty(&cwStation::left, &cwStation::setLeft);
    fillEmpty(&cwStation::right, &cwStation::setRight);
    fillEmpty(&cwStation::up, &cwStation::setUp);
    fillEmpty(&cwStation::down, &cwStation::setDown);

    if(lowerTie.splayCount() > 0) {
        tie.setSplays(tie.splays() + lowerTie.splays());
        tieChanged = true;
    }

    if(tieChanged) {
        upper->setStation(tie, tieIndex);
    }

    //The tail lands in the upper chunk before the lower one goes, so listeners
    //always see every shot somewhere
    upper->appendTail(lower->stations().mid(1), lower->shots());
    removeChunks(upperIndex + 1, upperIndex + 1);
}

bool cwTrip::canMoveChunk(int from, int to) const
{
    const auto inRange = [this](int i) { return i >= 0 && i < Chunks.size(); };
    return from != to && inRange(from) && inRange(to);
}

/**
 * Moves the chunk at \a from so it sits at \a to afterward, the way QList::move
 * does. The chunk itself is unchanged
 */
void cwTrip::moveChunk(int from, int to)
{
    if(!canMoveChunk(from, to)) {
        return;
    }

    emit chunksAboutToBeMoved(from, to);
    Chunks.move(from, to);
    emit chunksMoved(from, to);
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
        connectColumnData(chunk);
    }

    emit chunksInserted(0, chunkCount() - 1);
}

/**
  \brief Get's the chunk at i

  If i is out of bounds, this returns a null chunk
  */
cwSurveyChunk* cwTrip::chunk(int i) const {
    if(i < 0 || i >= chunkCount()) {
        return nullptr;
    }
    return Chunks.at(i);
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

cwUnits::UnitSystem cwTrip::unitSystem() const
{
    const cwCave* cave = parentCave();
    return cave ? cave->unitSystem() : cwUnits::Metric;
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
    return identityKeywordModel(m_linePlotKeywordModel, QStringLiteral("Line Plot"));
}

/**
 * Keyword model for this trip's splay shots. Carries Type="Splays" and extends
 * the trip's own keyword model, the splay counterpart of linePlotKeywordModel():
 * filtering out the Type keyword hides the trip's splays while the centerline
 * stays visible, and trip-level filters (Trip, Year, Date, Cave, Caver, ...)
 * hide both. Created lazily on first use.
 */
cwKeywordModel* cwTrip::splaysKeywordModel()
{
    return identityKeywordModel(m_splaysKeywordModel, QStringLiteral("Splays"));
}

/**
 * Lazily builds a trip-owned identity keyword model: a Type keyword plus an
 * extension of the trip's own keyword model, so the item filters both by its
 * type and alongside its trip (Trip, Year, Date, Cave, Caver, ...).
 */
cwKeywordModel* cwTrip::identityKeywordModel(cwKeywordModel*& model, const QString& type)
{
    if(model == nullptr) {
        model = new cwKeywordModel(this);
        model->add(cwKeyword(cwKeywordModel::TypeKey, type));
        model->addExtension(KeywordModel);
    }
    return model;
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

int cwTrip::ignoreAllWarnings(const QList<int>& errorTypeIds)
{
    int ignored = 0;
    for(cwSurveyChunk* chunk : std::as_const(Chunks)) {
        ignored += chunk->ignoreWarnings(errorTypeIds);
    }
    return ignored;
}

int cwTrip::clearIgnoredWarnings()
{
    return clearIgnoredWarnings(QList<int>());
}

int cwTrip::clearIgnoredWarnings(const QList<int>& errorTypeIds)
{
    int cleared = 0;
    for(cwSurveyChunk* chunk : std::as_const(Chunks)) {
        cleared += chunk->clearIgnoredWarnings(errorTypeIds);
    }
    return cleared;
}

int cwTrip::warningCountForTypeId(int errorTypeId) const
{
    int count = 0;
    for(const cwSurveyChunk* chunk : Chunks) {
        count += chunk->warningCountForTypeId(errorTypeId);
    }
    return count;
}

int cwTrip::ignorableWarningCount() const
{
    int count = 0;
    for(const cwSurveyChunk* chunk : Chunks) {
        count += chunk->ignorableWarningCount();
    }
    return count;
}

void cwTrip::connectColumnData(cwSurveyChunk* chunk)
{
    connect(chunk, &cwSurveyChunk::dataChanged, this, [this](cwSurveyChunk::DataRole role, int) {
        switch(role) {
        case cwSurveyChunk::StationLeftRole:
        case cwSurveyChunk::StationRightRole:
        case cwSurveyChunk::StationUpRole:
        case cwSurveyChunk::StationDownRole:
            scheduleColumnDataUpdate();
            break;
        default:
            break;
        }
    });
    connect(chunk, &cwSurveyChunk::stationSplaysChanged, this, &cwTrip::scheduleColumnDataUpdate);
    connect(chunk, &cwSurveyChunk::added, this, &cwTrip::scheduleColumnDataUpdate);
    connect(chunk, &cwSurveyChunk::removed, this, &cwTrip::scheduleColumnDataUpdate);
    scheduleColumnDataUpdate();
}

void cwTrip::scheduleColumnDataUpdate()
{
    if(m_columnDataPending) {
        return;
    }
    m_columnDataPending = true;
    QMetaObject::invokeMethod(this, [this]() {
        m_columnDataPending = false;
        updateColumnData();
    }, Qt::QueuedConnection);
}

void cwTrip::updateColumnData()
{
    const auto hasLrudText = [](const cwStation& station) {
        return station.left().state() != cwDistanceReading::State::Empty
               || station.right().state() != cwDistanceReading::State::Empty
               || station.up().state() != cwDistanceReading::State::Empty
               || station.down().state() != cwDistanceReading::State::Empty;
    };

    bool hasLrud = false;
    bool hasSplay = false;
    for(const cwSurveyChunk* chunk : std::as_const(Chunks)) {
        for(int i = 0; i < chunk->stationCount(); ++i) {
            const cwStation station = chunk->station(i);
            hasLrud = hasLrud || hasLrudText(station);
            hasSplay = hasSplay || station.splayCount() > 0;
        }
    }

    if(m_hasLrudData != hasLrud) {
        m_hasLrudData = hasLrud;
        emit hasLrudDataChanged();
    }
    if(m_hasSplayData != hasSplay) {
        m_hasSplayData = hasSplay;
        emit hasSplayDataChanged();
    }
}
