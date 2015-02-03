/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwRegionTreeModel.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwScrap.h"
#include "cwSurveyNoteModel.h"
#include "cwNote.h"
#include "cwGlobalIcons.h"

//Qt include
#include <QUrl>
#include <QDebug>

cwRegionTreeModel::cwRegionTreeModel(QObject *parent) :
    QAbstractItemModel(parent),
    Region(nullptr)
{

}

QHash<int, QByteArray> cwRegionTreeModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[TypeRole] = "indexType";
    roles[ObjectRole] = "object";
    return roles;
}


/**
  \brief Get's the caving region that this model represents
  */
cwCavingRegion *cwRegionTreeModel::cavingRegion() const {
    return Region;
}

/**
 * @brief cwRegionTreeModel::beginInsertCaves
 * @param parent
 * @param begin
 * @param end
 */
void cwRegionTreeModel::beginInsertCaves(QModelIndex parent, int begin, int end)
{
    Q_UNUSED(parent);
    beginInsertRows(QModelIndex(), begin, end);
}

/**
 * @brief cwRegionTreeModel::insertedCaves
 * @param parent
 * @param begin
 * @param end
 */
void cwRegionTreeModel::insertedCaves(QModelIndex parent, int begin, int end)
{
    Q_UNUSED(parent);
    Q_ASSERT(parent == QModelIndex());

    addCaveConnections(begin, end);
    endInsertRows();

    for(int i = begin; i <= end; i++) {
        cwCave* cave = Region->cave(i);
        if(cave->tripCount() > 0) {
            QModelIndex parentCaveIndex = index(cave);
            beginInsertRows(parentCaveIndex, 0, cave->tripCount() - 1);
            insertedTrips(cave, 0, cave->tripCount() - 1);
        }
    }
}


/**
 * @brief cwRegionTreeModel::beginRemoveCaves
 * @param parent
 * @param begin
 * @param end
 */
void cwRegionTreeModel::beginRemoveCaves(QModelIndex parent, int begin, int end)
{
    Q_UNUSED(parent);
    Q_ASSERT(parent == QModelIndex());

    for(int i = begin; i <= end; i++) {
        cwCave* cave = index(i, 0, QModelIndex()).data(ObjectRole).value<cwCave*>();
        beginRemoveTrips(cave, 0, cave->trips().size() - 1);
        endRemoveRows(); //beginRemoveTrips() starts the endRemoveRows
    }

    removeCaveConnections(begin, end);
    beginRemoveRows(QModelIndex(), begin, end);
}

/**
 * @brief cwRegionTreeModel::removedCaves
 * @param parent
 * @param begin
 * @param end
 */
void cwRegionTreeModel::removedCaves(QModelIndex parent, int begin, int end)
{
    Q_ASSERT(parent == QModelIndex());
    Q_UNUSED(parent);
    Q_UNUSED(begin);
    Q_UNUSED(end);
    endRemoveRows();
}

/**
 * @brief cwRegionTreeModel::beginInsertTrips
 * @param parent
 * @param begin
 * @param end
 */
void cwRegionTreeModel::beginInsertTrips(QModelIndex parent, int begin, int end)
{
    Q_ASSERT(parent == QModelIndex());
    Q_UNUSED(parent);

    Q_ASSERT(qobject_cast<cwCave*>(sender()) != nullptr);
    cwCave* parentCave = static_cast<cwCave*>(sender());

    QModelIndex parentIndex = index(parentCave);

    beginInsertRows(parentIndex, begin, end);
}

/**
 * @brief cwRegionTreeModel::insertedTrips
 * @param parent
 * @param begin
 * @param end
 */
void cwRegionTreeModel::insertedTrips(QModelIndex parent, int begin, int end)
{
    Q_ASSERT(parent == QModelIndex());
    Q_UNUSED(parent);

    Q_ASSERT(qobject_cast<cwCave*>(sender()) != nullptr);
    cwCave* parentCave = static_cast<cwCave*>(sender());

    insertedTrips(parentCave, begin, end);
}

/**
 * @brief cwRegionTreeModel::beginRemoveTrips
 * @param parent
 * @param begin
 * @param end
 */
void cwRegionTreeModel::beginRemoveTrips(QModelIndex parent, int begin, int end)
{
    Q_ASSERT(parent == QModelIndex());
    Q_UNUSED(parent);

    Q_ASSERT(qobject_cast<cwCave*>(sender()) != nullptr);
    cwCave* parentCave = static_cast<cwCave*>(sender());

    beginRemoveTrips(parentCave, begin, end);
}

/**
 * @brief cwRegionTreeModel::removedTrips
 * @param parent
 * @param begin
 * @param end
 */
void cwRegionTreeModel::removedTrips(QModelIndex parent, int begin, int end)
{
    Q_ASSERT(parent == QModelIndex());
    Q_ASSERT(qobject_cast<cwCave*>(sender()) != nullptr);
    Q_UNUSED(parent);
    Q_UNUSED(begin);
    Q_UNUSED(end);
    endRemoveRows();
}

/**
 * @brief cwRegionTreeModel::beginInsertNotes
 * @param parent
 * @param begin
 * @param end
 */
void cwRegionTreeModel::beginInsertNotes(QModelIndex parent, int begin, int end)
{
    Q_ASSERT(parent == QModelIndex());
    Q_UNUSED(parent);

    Q_ASSERT(qobject_cast<cwSurveyNoteModel*>(sender()) != nullptr);
    cwSurveyNoteModel* parentNoteModel = static_cast<cwSurveyNoteModel*>(sender());

    beginRemoveNotes(parentNoteModel->parentTrip(), begin, end);
}

/**
 * @brief cwRegionTreeModel::insertedNotes
 * @param parent
 * @param begin
 * @param end
 */
void cwRegionTreeModel::insertedNotes(QModelIndex parent, int begin, int end)
{
    Q_ASSERT(parent == QModelIndex());
    Q_UNUSED(parent);

    Q_ASSERT(qobject_cast<cwSurveyNoteModel*>(sender()) != nullptr);
    cwSurveyNoteModel* parentNoteModel = static_cast<cwSurveyNoteModel*>(sender());

    insertedNotes(parentNoteModel->parentTrip(), begin, end);
}

/**
 * @brief cwRegionTreeModel::beginRemoveNotes
 * @param parent
 * @param begin
 * @param end
 */
void cwRegionTreeModel::beginRemoveNotes(QModelIndex parent, int begin, int end)
{
    Q_ASSERT(parent == QModelIndex());
    Q_UNUSED(parent);

    Q_ASSERT(qobject_cast<cwSurveyNoteModel*>(sender()) != nullptr);
    cwSurveyNoteModel* noteModel = static_cast<cwSurveyNoteModel*>(sender());

    beginRemoveNotes(noteModel->parentTrip(), begin, end);
}

/**
 * @brief cwRegionTreeModel::removeNotes
 * @param parent
 * @param begin
 * @param end
 */
void cwRegionTreeModel::removeNotes(QModelIndex parent, int begin, int end)
{
    Q_ASSERT(parent == QModelIndex());
    Q_ASSERT(qobject_cast<cwSurveyNoteModel*>(sender()) != nullptr);
    Q_UNUSED(parent);
    Q_UNUSED(begin);
    Q_UNUSED(end);
    endRemoveRows();
}

/**
 * @brief cwRegionTreeModel::beginInsertScraps
 * @param begin
 * @param end
 */
void cwRegionTreeModel::beginInsertScraps(int begin, int end)
{
    Q_ASSERT(qobject_cast<cwNote*>(sender()) != nullptr);
    cwNote* parentNote = static_cast<cwNote*>(sender());

    QModelIndex parentIndex = index(parentNote);

    beginInsertRows(parentIndex, begin, end);
}

/**
 * @brief cwRegionTreeModel::insertedScraps
 * @param begin
 * @param end
 */
void cwRegionTreeModel::insertedScraps(int begin, int end)
{
    Q_ASSERT(qobject_cast<cwNote*>(sender()) != nullptr);
    insertedScraps(qobject_cast<cwNote*>(sender()), begin, end);
}

/**
 * @brief cwRegionTreeModel::beginRemoveScraps
 * @param begin
 * @param end
 */
void cwRegionTreeModel::beginRemoveScraps(int begin, int end)
{
    Q_ASSERT(qobject_cast<cwNote*>(sender()) != nullptr);
    cwNote* parentNote = static_cast<cwNote*>(sender());
    QModelIndex parentIndex = index(parentNote);

    beginRemoveRows(parentIndex, begin, end);
}

/**
 * @brief cwRegionTreeModel::removedScraps
 * @param begin
 * @param end
 */
void cwRegionTreeModel::removedScraps(int begin, int end)
{
    Q_UNUSED(begin);
    Q_UNUSED(end);
    Q_ASSERT(qobject_cast<cwNote*>(sender()) != nullptr);
    endRemoveRows();
}

/**
 * @brief cwRegionTreeModel::setCavingRegion
 * @param region
 */
void cwRegionTreeModel::setCavingRegion(cwCavingRegion* region) {
   //Remove all the connections
    removeCaveConnections(0, rowCount() - 1);

    //Reset the model
    beginResetModel();
    Region = region;
    connect(Region, SIGNAL(rowsAboutToBeInserted(QModelIndex,int,int)), SLOT(beginInsertCaves(QModelIndex,int,int)));
    connect(Region, SIGNAL(rowsInserted(QModelIndex,int,int)), SLOT(insertedCaves(QModelIndex,int,int)));
    connect(Region, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)), SLOT(beginRemoveCaves(QModelIndex,int,int)));
    connect(Region, SIGNAL(rowsRemoved(QModelIndex,int,int)), SLOT(removedCaves(QModelIndex,int,int)));
    endResetModel();

    addCaveConnections(0, rowCount() - 1);
}

/**
 * @brief cwRegionTreeModel::index
 * @param row
 * @param column
 * @param parent
 * @return
 */
QModelIndex cwRegionTreeModel::index ( int row, int column, const QModelIndex & parent) const {
    Q_ASSERT(column == 0);
    if(Region.isNull()) { return QModelIndex(); }
    if(row < 0) { return QModelIndex(); }

    switch(parent.data(TypeRole).toInt()) {
    case RegionType: {
        if(!parent.isValid()) {
            //Try to get a cave
            if(row >= Region->caveCount()) {
                return QModelIndex();
            }
            return createIndex(row, column, Region->cave(row));
        }
        Q_ASSERT(false);
        break;
    }

    case CaveType: {
        cwCave* parentCave = qobject_cast<cwCave*>((QObject*)parent.internalPointer());
        if(parentCave != nullptr) {
            if(row >= parentCave->tripCount()) {
                return QModelIndex();
            }

            return createIndex(row, column, parentCave->trip(row));
        }
        Q_ASSERT(false);
        break;
    }

    case TripType: {
        cwTrip* parentTrip = qobject_cast<cwTrip*>((QObject*)parent.internalPointer());
        if(parentTrip != nullptr) {
            if(row >= parentTrip->notes()->notes().size()) {
                return QModelIndex();
            }

            return createIndex(row, column, parentTrip->notes()->notes().at(row));
        }
        Q_ASSERT(false);
        break;
    }

    case NoteType: {
        cwNote* parentNote = qobject_cast<cwNote*>((QObject*)parent.internalPointer());
        if(parentNote != nullptr) {
            if(row >= parentNote->scraps().size()) {
                return QModelIndex();
            }

            return createIndex(row, column, parentNote->scrap(row));
        }
        Q_ASSERT(false);
        break;
    }

    default:
        break;
    }

    return QModelIndex();
}

/**
  \brief Gets the cave index of the model

  If the cave doesn't exist in the model this return QModelIndex()
  */
QModelIndex cwRegionTreeModel::index (cwCave* cave) const {
    if(Region == nullptr) { return QModelIndex(); }
    if(cave == nullptr) { return QModelIndex(); }
    int caveIndex = Region->indexOf(cave);
    if(caveIndex < 0) { QModelIndex(); }
    return index(caveIndex, 0, QModelIndex());
}

/**
  \brief Gets the cave index of the model

  If the cave doesn't exist in the model this return QModelIndex()
  */
QModelIndex cwRegionTreeModel::index (cwTrip* trip) const {
    cwCave* parentCave = trip->parentCave();
    if(parentCave == nullptr) { return QModelIndex(); }
    int tripIndex = parentCave->indexOf(trip);

    QModelIndex parentIndex = index(parentCave);
    if(!parentIndex.isValid()) { return QModelIndex(); }

    if(tripIndex < 0) { return QModelIndex(); }

    return index(tripIndex, 0, parentIndex);
}

/**
 * @brief cwRegionTreeModel::index
 * @param note
 * @return
 */
QModelIndex cwRegionTreeModel::index(cwNote *note) const
{
    cwTrip* parentTrip = note->parentTrip();
    if(parentTrip == nullptr) { return QModelIndex(); }
    int noteIndex = parentTrip->notes()->notes().indexOf(note);

    QModelIndex parentIndex = index(parentTrip);
    if(!parentIndex.isValid()) { return QModelIndex(); }

    if(noteIndex < 0) { return QModelIndex(); }

    return index(noteIndex, 0, parentIndex);
}

/**
 * @brief cwRegionTreeModel::index
 * @param scrap
 * @return
 */
QModelIndex cwRegionTreeModel::index(cwScrap *scrap) const
{
    cwNote* parentNote = scrap->parentNote();
    if(parentNote == nullptr) { return QModelIndex(); }
    int scrapIndex = parentNote->scraps().indexOf(scrap);

    QModelIndex parentIndex = index(parentNote);
    if(!parentIndex.isValid()) { return QModelIndex(); }

    if(scrapIndex < 0) { return QModelIndex(); }

    return index(scrapIndex, 0, parentIndex);
}

/**
 * @brief cwRegionTreeModel::parent
 * @param index
 * @return
 */
QModelIndex cwRegionTreeModel::parent ( const QModelIndex & index ) const {
    switch(index.data(TypeRole).toInt()) {
    case RegionType:
        return QModelIndex();
    case CaveType:
        return QModelIndex();

    case TripType: {

        cwTrip* trip = qobject_cast<cwTrip*>(static_cast<QObject*>(index.internalPointer()));
        if(trip != nullptr) {
            cwCave* parentCave = trip->parentCave();
            int row = Region->indexOf(parentCave);
            Q_ASSERT(row >= 0); //This should always return with a valid index

            return createIndex(row, 0, parentCave);
        }

        Q_ASSERT(false);
        break;
    }

    case NoteType: {
        cwNote* note = qobject_cast<cwNote*>(static_cast<QObject*>(index.internalPointer()));
        if(note != nullptr) {
            cwTrip* parentTrip = note->parentTrip();
            int row = parentTrip->notes()->notes().indexOf(note);
            Q_ASSERT(row >= 0);

            return createIndex(row, 0, parentTrip);
        }

        Q_ASSERT(false);
        break;
    }

    case ScrapType: {
        cwScrap* scrap = qobject_cast<cwScrap*>(static_cast<QObject*>(index.internalPointer()));
        if(scrap != nullptr) {
            cwNote* parentNote = scrap->parentNote();
            int row = parentNote->scraps().indexOf(scrap);
            Q_ASSERT(row >= 0);

            return createIndex(row, 0, parentNote);
        }
        Q_ASSERT(false);
        break;
    }

    default:
        break;
    }

    return QModelIndex();
}

/**
 * @brief cwRegionTreeModel::rowCount
 * @param parent
 * @return
 */
int cwRegionTreeModel::rowCount ( const QModelIndex & parent ) const {
    if(Region == nullptr) { return 0; }

    if(!parent.isValid()) {
        return Region->caveCount();
    }

    switch(parent.data(TypeRole).toInt()) {
    case RegionType: {
        Q_ASSERT(false);
        break;
    }

    case CaveType: {
        cwCave* parentCave = qobject_cast<cwCave*>((QObject*)parent.internalPointer());
        if(parentCave != nullptr) {
            return parentCave->tripCount();
        }
        Q_ASSERT(false);
        break;

    }

    case TripType: {
        cwTrip* parentTrip = qobject_cast<cwTrip*>((QObject*)parent.internalPointer());
        if(parentTrip != nullptr) {
            return parentTrip->notes()->rowCount();
        }
        Q_ASSERT(false);
        break;
    }

    case NoteType: {
        cwNote* parentNote = qobject_cast<cwNote*>((QObject*)parent.internalPointer());
        if(parentNote != nullptr) {
            return parentNote->scraps().size();
        }
        Q_ASSERT(false);
        break;
    }

    default:
        break;
    }

    return 0;
}

/**
 * @brief cwRegionTreeModel::columnCount
 * @return
 */
int cwRegionTreeModel::columnCount ( const QModelIndex & /*parent*/) const {
    if(Region == nullptr) { return 0; }

    return 1; //Always one column
}

/**
 * @brief cwRegionTreeModel::data
 * @param index
 * @param role
 * @return
 */
QVariant cwRegionTreeModel::data ( const QModelIndex & index, int role ) const {
    if(!index.isValid()) {     //Root item selected
        switch(role) {
        case TypeRole:
            return RegionType;
        }
        return QVariant();
    }

    cwCave* currentCave = qobject_cast<cwCave*>(static_cast<QObject*>(index.internalPointer()));
    if(currentCave != nullptr) {
        switch(role) {
        case ObjectRole:
            return QVariant::fromValue(currentCave);
        case TypeRole:
            return CaveType;
        default:
            return QVariant();
        }
    }

    cwTrip* currentTrip = qobject_cast<cwTrip*>(static_cast<QObject*>(index.internalPointer()));
    if(currentTrip != nullptr) {
        switch(role) {
        case ObjectRole:
            return QVariant::fromValue(currentTrip);
        case TypeRole:
            return TripType;
        default:
            return QVariant();
        }
    }

    cwNote* note = qobject_cast<cwNote*>(static_cast<QObject*>(index.internalPointer()));
    if(note != nullptr) {
        switch(role) {
        case ObjectRole:
            return QVariant::fromValue(note);
        case TypeRole:
            return NoteType;
        default:
            return QVariant();
        }
    }

    cwScrap* scrap = qobject_cast<cwScrap*>(static_cast<QObject*>(index.internalPointer()));
    if(scrap != nullptr) {
        switch(role) {
        case ObjectRole:
            return QVariant::fromValue(scrap);
        case TypeRole:
            return ScrapType;
        default:
            return QVariant();
        }
    }

    return QVariant();
}

/**
  \brief Gets the trip at index

  If index isn't a trip, then this returns null
  */
cwTrip* cwRegionTreeModel::trip(const QModelIndex& index) const {
    QVariant tripVariant = data(index, ObjectRole);
    if(tripVariant.canConvert<cwTrip*>()) {
        cwTrip* trip = tripVariant.value<cwTrip*>();
        return trip;
    }
    return nullptr;
}

/**
  \brief Gets the cave at inde
  */
cwCave* cwRegionTreeModel::cave(const QModelIndex& index) const {
    QVariant caveVariant = data(index, ObjectRole);
    if(caveVariant.canConvert<cwCave*>()) {
        cwCave* cave = caveVariant.value<cwCave*>();
        return cave;
    }
    return nullptr;
}

/**
 * @brief cwRegionTreeModel::note
 * @param index
 * @return
 */
cwNote *cwRegionTreeModel::note(const QModelIndex &index) const
{
    QVariant noteVariant = data(index, ObjectRole);
    if(noteVariant.canConvert<cwNote*>()) {
        cwNote* note = noteVariant.value<cwNote*>();
        return note;
    }
    return nullptr;
}

/**
 * @brief cwRegionTreeModel::scrap
 * @param index
 * @return
 */
cwScrap *cwRegionTreeModel::scrap(const QModelIndex &index) const
{
    QVariant scrapVariant = data(index, ObjectRole);
    if(scrapVariant.canConvert<cwScrap*>()) {
        cwScrap* note = scrapVariant.value<cwScrap*>();
        return note;
    }
    return nullptr;
}

/**
  \brief Adds all the connection for a cave
  */
void cwRegionTreeModel::addCaveConnections(int beginIndex, int endIndex) {

    for(int i = beginIndex; i <= endIndex; i++) {
        cwCave* cave = Region->cave(i);

        connect(cave, SIGNAL(rowsAboutToBeInserted(QModelIndex,int,int)),
                this, SLOT(beginInsertTrips(QModelIndex,int,int)), Qt::UniqueConnection);
        connect(cave, SIGNAL(rowsInserted(QModelIndex,int,int)),
                this, SLOT(insertedTrips(QModelIndex,int,int)), Qt::UniqueConnection);
        connect(cave, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)),
                this, SLOT(beginRemoveTrips(QModelIndex,int,int)), Qt::UniqueConnection);
        connect(cave, SIGNAL(rowsRemoved(QModelIndex,int,int)),
                this, SLOT(removedTrips(QModelIndex,int,int)), Qt::UniqueConnection);

        addTripConnections(cave, 0, cave->tripCount() - 1);
    }
}

/**
  \brief Removes all the connection for a cave
  */
void cwRegionTreeModel::removeCaveConnections(int beginIndex, int endIndex) {
    for(int i = beginIndex; i <= endIndex; i++) {
        cwCave* cave = Region->cave(i);
        disconnect(cave, 0, this, 0); //disconnect signals and slots to this object
    }
}

/**
  \brief Adds connection for the trips between beginIndex and endIndex
  */
void cwRegionTreeModel::addTripConnections(cwCave* parentCave, int beginIndex, int endIndex) {
    for(int i = beginIndex; i <= endIndex; i++) {
        cwTrip* currentTrip = parentCave->trip(i);

        connect(currentTrip->notes(), SIGNAL(rowsAboutToBeInserted(QModelIndex,int,int)),
                this, SLOT(beginInsertNotes(QModelIndex,int,int)), Qt::UniqueConnection);
        connect(currentTrip->notes(), SIGNAL(rowsInserted(QModelIndex,int,int)),
                this, SLOT(insertedNotes(QModelIndex,int,int)), Qt::UniqueConnection);
        connect(currentTrip->notes(), SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)),
                this, SLOT(beginRemoveNotes(QModelIndex,int,int)), Qt::UniqueConnection);
        connect(currentTrip->notes(), SIGNAL(rowsRemoved(QModelIndex,int,int)),
                this, SLOT(removeNotes(QModelIndex,int,int)), Qt::UniqueConnection);

        addNoteConnections(currentTrip, 0, currentTrip->notes()->notes().size() - 1);
    }
}

/**
  \brief Removes the connections for a trips between beginIndex and endIndex
  */
void cwRegionTreeModel::removeTripConnections(cwCave* parentCave, int beginIndex, int endIndex) {

    for(int i = beginIndex; i <= endIndex; i++) {
        cwTrip* trip = parentCave->trip(i);
        disconnect(trip, 0, this, 0); //disconnect signals and slots to this object
    }

}

/**
 * @brief cwRegionTreeModel::addNoteConnections
 * @param parentTrip
 * @param beginIndex
 * @param endIndex
 */
void cwRegionTreeModel::addNoteConnections(cwTrip *parentTrip, int beginIndex, int endIndex)
{
   for(int i = beginIndex; i <= endIndex; i++) {
       cwNote* note = parentTrip->notes()->notes().at(i);
       connect(note, SIGNAL(beginInsertingScraps(int,int)),
               this, SLOT(beginInsertScraps(int,int)), Qt::UniqueConnection);
       connect(note, SIGNAL(insertedScraps(int,int)),
               this, SLOT(insertedScraps(int,int)), Qt::UniqueConnection);
       connect(note, SIGNAL(beginRemovingScraps(int,int)),
               this, SLOT(beginRemoveScraps(int,int)), Qt::UniqueConnection);
       connect(note, SIGNAL(removedScraps(int,int)),
               this, SLOT(removedScraps(int,int)), Qt::UniqueConnection);
   }
}

/**
 * @brief cwRegionTreeModel::removeNoteConnections
 * @param parentTrip
 * @param beginIndex
 * @param endIndex
 */
void cwRegionTreeModel::removeNoteConnections(cwTrip *parentTrip, int beginIndex, int endIndex)
{
    for(int i = beginIndex; i <= endIndex; i++) {
        cwNote* note = parentTrip->notes()->notes().at(i);
        disconnect(note, 0, this, 0);
    }
}

/**
 * @brief cwRegionTreeModel::beginRemoveTrips
 * @param parentCave
 * @param begin
 * @param end
 */
void cwRegionTreeModel::beginRemoveTrips(cwCave *parentCave, int begin, int end)
{
    QModelIndex parentIndex = index(parentCave);

    for(int i = begin; i <= end; i++) {
        cwTrip* trip = index(i, 0, parentIndex).data(ObjectRole).value<cwTrip*>();
        beginRemoveNotes(trip, 0, trip->notes()->rowCount() - 1);
        endRemoveRows(); //beginRemoveTrips() starts the endRemoveRows
    }

    removeTripConnections(parentCave, begin, end);
    beginRemoveRows(parentIndex, begin, end);
}

/**
 * @brief cwRegionTreeModel::beginRemoveNotes
 * @param parentTrip
 * @param begin
 * @param end
 */
void cwRegionTreeModel::beginRemoveNotes(cwTrip *parentTrip, int begin, int end)
{
    QModelIndex parentIndex = index(parentTrip);

    for(int i = begin; i <= end; i++) {
        cwNote* note = index(i, 0, parentIndex).data(ObjectRole).value<cwNote*>();
        beginRemoveScraps(note, 0, note->scraps().size() - 1);
        endRemoveRows();
    }

    removeNoteConnections(parentTrip, begin, end);
    beginRemoveRows(parentIndex, begin, end);
}

/**
 * @brief cwRegionTreeModel::beginRemoveScraps
 * @param parentNote
 * @param begin
 * @param end
 */
void cwRegionTreeModel::beginRemoveScraps(cwNote *parentNote, int begin, int end)
{
    QModelIndex parentIndex = index(parentNote);

    beginRemoveRows(parentIndex, begin, end);
}

/**
 * @brief cwRegionTreeModel::insertedTrips
 * @param parentCave
 * @param begin
 * @param end
 */
void cwRegionTreeModel::insertedTrips(cwCave *parentCave, int begin, int end)
{
    addTripConnections(parentCave, begin, end);
    endInsertRows();

    for(int i = begin; i <= end; i++) {
        cwTrip* trip = parentCave->trip(i);
        int lastIndex = trip->notes()->rowCount() - 1;
        if(lastIndex >= 0) {
            QModelIndex parentTripIndex = index(trip);
            beginInsertRows(parentTripIndex, 0, lastIndex);
            insertedNotes(trip, 0, lastIndex);
        }
    }
}

/**
 * @brief cwRegionTreeModel::insertedNotes
 * @param parentTrip
 * @param begin
 * @param end
 */
void cwRegionTreeModel::insertedNotes(cwTrip *parentTrip, int begin, int end)
{
    addNoteConnections(parentTrip, begin, end);
    endInsertRows();

    for(int i = begin; i <= end; i++) {
        cwNote* note = parentTrip->notes()->notes().at(i);
        int lastIndex = note->scraps().size() - 1;
        if(lastIndex >= 0) {
            QModelIndex parentNoteIndex = index(note);
            beginInsertRows(parentNoteIndex, 0, lastIndex);
            insertedScraps(note, 0, lastIndex);
        }
    }
}

/**
 * @brief cwRegionTreeModel::insertedScraps
 * @param parentNote
 * @param begin
 * @param end
 */
void cwRegionTreeModel::insertedScraps(cwNote *parentNote, int begin, int end)
{
    Q_UNUSED(parentNote);
    Q_UNUSED(begin);
    Q_UNUSED(end);
    endInsertRows();
}
