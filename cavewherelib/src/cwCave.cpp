/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwCave.h"
#include "cwTrip.h"
#include "cwStation.h"
#include "cwLength.h"
#include "cwErrorModel.h"
#include "cwCavingRegion.h"
#include "cwData.h"
#include "cwFixStationModel.h"
#include "cwGridConvergence.h"
#include "cwNameUtils.h"
#include "cwKeywordModel.h"
#include "cwTripCalibration.h"

//Qt includes
#include <QThread>
#include <QUuid>

cwCave::cwCave(QObject* parent) :
    QAbstractListModel(parent),
    Length(new cwLength(this)),
    Depth(new cwLength(this)),
    ErrorModel(new cwErrorModel(this)),
    FixStations(new cwFixStationModel(this)),
    StationPositionModelStale(false),
    Id(QUuid::createUuid()),
    m_gridConvergence(new cwGridConvergence(this)),
    m_equates(new cwEquateModel(this)),
    m_keywordModel(new cwKeywordModel(this))
{
    Length->setUnit(cwUnits::Meters);
    Depth->setUnit(cwUnits::Meters);

    Length->setUpdateValue(true);
    Depth->setUpdateValue(true);

//    ErrorModel->addParent(this);

    connect(FixStations, &cwFixStationModel::countChanged,
            this, &cwCave::recomputeGridConvergence);
    connect(FixStations, &QAbstractItemModel::modelReset,
            this, &cwCave::recomputeGridConvergence);
    // Filter dataChanged on the roles that actually influence the
    // formatted convergence — variance/id edits would otherwise walk
    // PROJ only to be discarded by the change-detection guard.
    connect(FixStations, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex&, const QModelIndex&, const QList<int>& roles) {
                if (roles.isEmpty()
                    || roles.contains(cwFixStationModel::InputCSRole)
                    || roles.contains(cwFixStationModel::EastingRole)
                    || roles.contains(cwFixStationModel::NorthingRole)
                    || roles.contains(cwFixStationModel::ElevationRole)
                    || roles.contains(cwFixStationModel::StationNameRole)) {
                    recomputeGridConvergence();
                }
            });
    recomputeGridConvergence();
}

// /**
//   \brief Copy constructor
//   */
// cwCave::cwCave(const cwCave& object) :
//     QAbstractListModel(nullptr),
//     cwUndoer(),
//     Length(new cwLength(this)),
//     Depth(new cwLength(this)),
//     ErrorModel(new cwErrorModel(this)),
//     StationPositionModelStale(false)
// {
//     Copy(object);
// }

// /**
//   \brief Assignment operator
//   */
// cwCave& cwCave::operator=(const cwCave& object) {
//     return Copy(object);
// }

cwCave::~cwCave() {
    Q_ASSERT(thread() == QThread::currentThread() || thread() == nullptr);
}

// /**
//   \brief Copies data from one object to another one
//   */
// cwCave& cwCave::Copy(const cwCave& object) {
//     if(&object == this) {
//         return *this;
//     }

//     //Set the name of the cave
//     setName(object.name());

//     //Set the station positions
//     setStationPositionLookup(object.stationPositionLookup());

//     //Remove all the old trips
//     int lastTripIndex = Trips.size() - 1;
//     Trips.clear();
//     if(lastTripIndex > 0) {
//         emit removedTrips(0, lastTripIndex);
//     }

//     //Copy all the trips from object
//     Trips.reserve(object.tripCount());
//     for(int i = 0; i < object.tripCount(); i++) {
//         cwTrip* trip = object.trip(i);
//         cwTrip* newTrip = new cwTrip(*trip); //Deep copy of the trip
//         newTrip->setParent(this);
//         newTrip->setParentCave(this);
//         newTrip->errorModel()->setParentModel(ErrorModel);
//         Trips.append(newTrip);
//     }

//     if(object.tripCount() - 1 > 0) {
//         emit insertedTrips(0, object.tripCount() - 1);
//     }

//     *Length = *(object.Length);
//     *Depth = *(object.Depth);

//     StationPositionModelStale = object.StationPositionModelStale;
//     Network = object.Network;

//     return *this;
// }

/**
  \brief Sets the name of the cwCave
  */
void cwCave::setName(QString name) {
    if(Name == name) {
        return;
    }
    if (!validateName(name).isEmpty()) {
        return;
    }
    pushUndo(new NameCommand(this, name));
}

QString cwCave::validateName(const QString& proposedName) const
{
    const auto* nameSet = parentRegion() ? &parentRegion()->caveNameSet() : nullptr;
    return cwNameUtils::validateEntityName(Name, proposedName, nameSet,
                                           QStringLiteral("cave"));
}

/**
  Returns proposedName sanitized and deduplicated against the existing trip
  names. cwTrip::setName silently rejects collisions and unsanitized names,
  so callers renaming a trip from an external string (e.g. an entry
  filename) should route through this first.
  */
QString cwCave::uniqueTripName(const QString& proposedName) const
{
    return m_tripNames.deduplicateName(cwNameUtils::sanitizeFileName(proposedName));
}

void cwCave::updateKeywords()
{
    if(!m_keywordModel) {
        return;
    }

    if(!Name.isEmpty()) {
        m_keywordModel->replace({cwKeywordModel::CaveKey, Name});
    } else {
        m_keywordModel->removeAll(cwKeywordModel::CaveKey);
    }
}

void cwCave::setId(const QUuid& id)
{
    if (!id.isNull()) {
        Id = id;
    } else if (Id.isNull()) {
        Id = QUuid::createUuid();
    }
}

void cwCave::setExternalCenterline(const cwExternalCenterline& value)
{
    if (m_externalCenterline == value) {
        return;
    }
    m_externalCenterline = value;
    emit externalCenterlineChanged();
}



/**
  Adds a new trip to the cave

  The trip will be added to the end of the all the trip
  */
void cwCave::addTripNullHelper() {
    cwTrip* trip = new cwTrip(undoStack());

    // Seed the new trip's survey-entry unit from the project default (unitSystem()
    // resolves the region, or Metric when the cave has no region yet). Only new
    // trips are seeded; loaded trips keep their own stored unit.
    trip->calibrations()->setDistanceUnit(cwUnits::surveyUnit(unitSystem()));

    QString tripName = QString("Trip %1").arg(tripCount() + 1);
    beginUndoMacro(QString("Add %1").arg(tripName));

    trip->setName(tripName);
    addTrip(trip);

    endUndoMacro();
}

/**
  \brief Adds a trip to the cave

  Once the trip is added to the cave, the cave ownes the trip
  */
void cwCave::addTrip(cwTrip* trip) {
    if(trip == nullptr) {
        addTripNullHelper();
        return;
    }
    insertTrip(Trips.size(), trip);
}

/**
  \brief Insert a survey trip to the cave

  i - The index where the trip will be inserted
  */
void cwCave::insertTrip(int i, cwTrip* trip) {
    if(i < 0 || i > Trips.size()) { return; }

    //Reparent the trip, if already in another cave
    cwCave* parentCave = dynamic_cast<cwCave*>(((QObject*)trip)->parent());
    if(parentCave != nullptr) {
        int index = parentCave->Trips.indexOf(trip);
        parentCave->removeTrip(index);
    }

    // Auto-rename to avoid filesystem path collisions in .cwproj layout.
    // Trip has no parentCave yet, so setName()'s guard won't fire.
    const QString deduped = m_tripNames.deduplicateName(trip->name());
    if (deduped != trip->name()) {
        trip->setName(deduped);
    }

    pushUndo(new InsertTripCommand(this, trip, i));
}

/**
  \brief Removes a trip from the cave

  i - The index where the trip will be remove.  The trip will be the responiblity of
  the caller to delete it
  */
void cwCave::removeTrip(int i) {
    if(i < 0 || i >= Trips.size()) { return; }
    pushUndo(new RemoveTripCommand(this, i, i));
}

/**
  \brief Removes all the trips from the cave
  */
void cwCave::clearTrips() {
    if(!Trips.isEmpty()) {
        pushUndo(new RemoveTripCommand(this, 0, Trips.size() - 1));
    }
}

cwCavingRegion *cwCave::parentRegion() const
{
    return dynamic_cast<cwCavingRegion*>(parent());
}

cwUnits::UnitSystem cwCave::unitSystem() const
{
    const cwCavingRegion* region = parentRegion();
    return region ? region->unitSystem() : cwUnits::Metric;
}

void cwCave::recomputeGridConvergence()
{
    // The readout owns the PROJ work and change detection; we just feed it the
    // current fix stations plus the region CS to fall back on when a fix
    // station omits its own input CS.
    const cwCavingRegion* region = parentRegion();
    const QString fallbackCS = region ? region->geoReference()->globalCoordinateSystem() : QString();
    m_gridConvergence->update(FixStations->fixStations(), fallbackCS);
}

/**
 * @brief cwCave::rowCount
 * @param parent
 * @return Returns the number of trips in the cwCave
 */
int cwCave::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return tripCount();
}

/**
 * @brief cwCave::data
 * @param index
 * @param role
 * @return
 */
QVariant cwCave::data(const QModelIndex &index, int role) const
{
   if(!index.isValid()) {
       return QVariant();
   }

   switch(role) {
   case TripObjectRole:
       return QVariant::fromValue(Trips.at(index.row()));
   default:
       return QVariant();
   }

   return QVariant();
}

/**
 * @brief cwCave::roleNames
 * @return Returns the roleNames of the model. See the Qt doc for details
 */
QHash<int, QByteArray> cwCave::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(TripObjectRole, "tripObjectRole");
    return roles;
}

/**
 * @brief cwCave::index
 * @param row
 * @param column
 * @param parent
 * @return
 */
QModelIndex cwCave::index(int row, int column, const QModelIndex &parent) const
{
   return QAbstractListModel::index(row, column, parent);
}

/**
  \brief Sets the undo stack for the cave and all of it's children
  */
void cwCave::setUndoStackForChildren() {
    setUndoStackForChildrenHelper(Trips);
}


cwCave::NameCommand::NameCommand(cwCave* cave, QString name) {
    CavePtr = cave;
    newName = name;
    oldName = cave->name();
    setText(QString("Change cave's name to %1").arg(name));
}

void cwCave::NameCommand::redo() {
    cwCave* cave = CavePtr;
    if (auto* region = cave->parentRegion()) {
        if (region->caves().contains(cave)) {
            region->caveNameSet().rename(oldName, newName);
        }
    }
    cave->Name = newName;
    cave->updateKeywords();
    emit cave->nameChanged();
}


void cwCave::NameCommand::undo() {
    cwCave* cave = CavePtr;
    if (auto* region = cave->parentRegion()) {
        if (region->caves().contains(cave)) {
            region->caveNameSet().rename(newName, oldName);
        }
    }
    cave->Name = oldName;
    cave->updateKeywords();
    emit cave->nameChanged();
}

cwCave::InsertRemoveTrip::InsertRemoveTrip(cwCave* cave,
                                                   int beginIndex, int endIndex) {
    CavePtr = cave;
    BeginIndex = beginIndex;
    EndIndex = endIndex;
    OwnsTrips = false;
}

cwCave::InsertRemoveTrip::~InsertRemoveTrip() {
    if(OwnsTrips) {
        for(auto trip : std::as_const(Trips)) {
            if(!trip.isNull()) {
                trip->deleteLater();
            }
        }
    }
}

void cwCave::InsertRemoveTrip::insertTrips() {
    cwCave* cave = CavePtr; //.data();
    emit cave->beginInsertTrips(BeginIndex, EndIndex);
    emit cave->beginInsertRows(QModelIndex(), BeginIndex, EndIndex);
    for(int i = 0; i < Trips.size(); i++) {
        int index = BeginIndex + i;
        cave->Trips.insert(index, Trips[i]);
        cave->m_tripNames.insert(Trips[i]->name());
        Trips[i]->setParentCave(cave);
        Trips[i]->errorModel()->setParentModel(cave->errorModel());
    }

    OwnsTrips = false;

    emit cave->endInsertRows();
    emit cave->insertedTrips(BeginIndex, EndIndex);
}

void cwCave::InsertRemoveTrip::removeTrips() {
    cwCave* cave = CavePtr; //.data();
    emit cave->beginRemoveTrips(BeginIndex, EndIndex);
    emit cave->beginRemoveRows(QModelIndex(), BeginIndex, EndIndex);

    //Remove all the trips from the back to the front
    for(int i = Trips.size() - 1; i >= 0; i--) {
        int index = BeginIndex + i;
        cave->m_tripNames.remove(cave->Trips.at(index)->name());
        cave->Trips.removeAt(index);

        //Do NOT uncomment, qml engine may garbage collect objects that aren't parented, and can cause double free problem
        // Trips[i]->setParentCave(nullptr);

        Trips[i]->errorModel()->setParentModel(nullptr);
    }

    OwnsTrips = true;

    emit cave->endRemoveRows();
    emit cave->removedTrips(BeginIndex, EndIndex);
}


cwCave::InsertTripCommand::InsertTripCommand(cwCave* cave,
                                             QList<cwTrip*> trips,
                                             int index) :
    cwCave::InsertRemoveTrip(cave, index, index + trips.size() -1)
{
    Trips.clear();
    for(int i = 0; i < trips.size(); i++) {
        Trips.append(trips.at(i));
    }

    if(Trips.size() == 1) {
        setText(QString("Add %1").arg(Trips.first()->name()));
    } else {
        setText(QString("Add %1 Trips").arg(Trips.size()));
    }
}

cwCave::InsertTripCommand::InsertTripCommand(cwCave* cave,
                                                     cwTrip* Trip,
                                                     int index) :
    cwCave::InsertRemoveTrip(cave, index, index)
{
    Trips.append(Trip);
    setText(QString("Add %1").arg(Trip->name()));
}


void cwCave::InsertTripCommand::redo() {
    insertTrips();
}

void cwCave::InsertTripCommand::undo() {
    removeTrips();
}

cwCave::RemoveTripCommand::RemoveTripCommand(cwCave* cave,
                                                     int beginIndex,
                                                     int endIndex) :
    InsertRemoveTrip(cave, beginIndex, endIndex)
{
    for(int i = beginIndex; i <= endIndex; i++) {
       Trips.append(cave->Trips[i]);
    }

    QString commandText;
    if(beginIndex != endIndex) {
        commandText = QString("Remove %1 Trips").arg(endIndex - beginIndex);
    } else {
        cwTrip* Trip = cave->Trips[beginIndex];
        commandText = QString("Remove %1").arg(Trip->name());
    }
}

void cwCave::RemoveTripCommand::redo() {
    removeTrips();
}

void cwCave::RemoveTripCommand::undo() {
    insertTrips();
}

/**
  \brief Gets all the stations in the cave

  To figure out how stations are structured see cwTrip and cwSurveyChunk

  Shots and stations are stored in the cwSurveyChunk
  */
QList<cwStation> cwCave::stations() const {
    QList<cwStation> allStations;
    foreach(cwTrip* trip, trips()) {
        allStations.append(trip->stations());
    }
    return allStations;
}

bool cwCave::validate(const cwEquate& equate) const
{
    if (!equate.isValid()) {
        return false;
    }

    const auto tripContains = [this](const QUuid& tripId) {
        for (const cwTrip* trip : Trips) {
            if (trip != nullptr && trip->id() == tripId) {
                return true;
            }
        }
        return false;
    };

    const QList<cwStationHandle> handles = equate.stations();
    for (const cwStationHandle& handle : handles) {
        switch (handle.scope()) {
        case cwStationHandle::NativeCave:
            if (handle.containerId() != Id) {
                return false;
            }
            break;
        case cwStationHandle::Trip:
            if (!tripContains(handle.containerId())) {
                return false;
            }
            break;
        default:
            // An out-of-enum scope (e.g. a cast int pushed through QML) names no
            // container this cave can resolve, so it is never a valid tie.
            return false;
        }
    }

    return true;
}

cwCaveData cwCave::data() const
{
    //Todo load trips
    return {
        Name,
        cwData::toDataList<cwTripData>(Trips),
        StationPositionModel,
        Id,
        static_cast<cwUnits::LengthUnit>(length()->unit()),
        static_cast<cwUnits::LengthUnit>(depth()->unit()),
        FixStations->fixStations(),
        m_externalCenterline,
        m_equates->equates()
    };
}

void cwCave::setData(const cwCaveData &data)
{
    setName(data.name);
    setId(data.id);
    length()->setUnit(data.lengthUnit);
    depth()->setUnit(data.depthUnit);
    setExternalCenterline(data.externalCenterline);

    clearTrips();

    FixStations->setFixStations(data.fixStations);
    m_equates->setEquates(data.equates);

    //Insert all trips
    for(const auto& tripData : data.trips) {
        cwTrip* trip = new cwTrip();
        trip->setData(tripData);
        addTrip(trip);
    }
}

/**
  \brief Sets the station position model for the cave

  This holds all the position for al the stations in the cave (this is populated
  after the loop closure has completed)
  */
void cwCave::setStationPositionLookup(const cwStationPositionLookup &model) {
    StationPositionModel = model;

    emit stationPositionPositionChanged();
}



/**
 * @brief cwCave::setSurveyNetwork
 * @param network
 *
 * Holds the calculated survey network for the cave. This is a look up for stations and
 * thier neighoring stations
 */
void cwCave::setSurveyNetwork(const cwSurveyNetwork &network)
{
    Network = network;
    emit surveyNetworkChanged();
}

/**
 * @brief cwCave::setStationPositionLookupStale
 * @param isStale
 *
 * Sets the station position lookup as stale. If true, the station position lookup, should
 * be recalculated, else, it shouldn't be recalculated.
 */
void cwCave::setStationPositionLookupStale(bool isStale)
{
    StationPositionModelStale = isStale;
}

/**
 * @brief cwCave::isStationPositionLookupStale
 * @return True if the station position lookup is old and stale, false it is up to date
 */
bool cwCave::isStationPositionLookupStale() const
{
    return StationPositionModelStale;
}
