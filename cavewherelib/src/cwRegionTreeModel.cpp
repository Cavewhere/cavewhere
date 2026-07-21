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
#include "cwSurveyNoteLiDARModel.h"
#include "cwNoteLiDAR.h"
#include "cwSurveyNoteSketchModel.h"
#include "cwSketch.h"

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
    Q_ASSERT(begin <= end);
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
    Q_ASSERT(begin <= end);

    addCaveConnections(begin, end, false /* recusive */);
    endInsertRows();

    for(int i = begin; i <= end; i++) {
        cwCave* cave = Region->cave(i);
        if(cave->tripCount() > 0) {
            QModelIndex parentCaveIndex = index(cave);
            beginInsertRows(parentCaveIndex, 0, cave->tripCount() - 1);
            insertedTripsForCave(cave, 0, cave->tripCount() - 1);
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
{;
    Q_UNUSED(parent);
    Q_ASSERT(begin <= end);
    Q_ASSERT(parent == QModelIndex());

    for(int i = begin; i <= end; i++) {
        cwCave* cave = index(i, 0, QModelIndex()).data(ObjectRole).value<cwCave*>();
        if(cave->hasTrips()) {
            beginRemoveTripsForCave(cave, 0, cave->trips().size() - 1);
            endRemoveRows(); //beginRemoveTrips() starts the endRemoveRows
        }
    }

    removeCaveConnections(begin, end, false); //trips already unwound above
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
    Q_ASSERT(begin <= end);

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
    Q_ASSERT(begin <= end);

    Q_ASSERT(qobject_cast<cwCave*>(sender()) != nullptr);
    cwCave* parentCave = static_cast<cwCave*>(sender());

    insertedTripsForCave(parentCave, begin, end);
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
    Q_ASSERT(begin <= end);

    Q_ASSERT(qobject_cast<cwCave*>(sender()) != nullptr);
    cwCave* parentCave = static_cast<cwCave*>(sender());

    beginRemoveTripsForCave(parentCave, begin, end);
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
    Q_ASSERT(begin <= end);

    Q_ASSERT(qobject_cast<cwSurveyNoteModel*>(sender()) != nullptr);
    cwSurveyNoteModel* parentNoteModel = static_cast<cwSurveyNoteModel*>(sender());

    QModelIndex parentIndex = index(parentNoteModel->parentTrip()->notes());

    beginInsertRows(parentIndex, begin, end);
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
    Q_ASSERT(begin <= end);

    Q_ASSERT(qobject_cast<cwSurveyNoteModel*>(sender()) != nullptr);
    cwSurveyNoteModel* parentNoteModel = static_cast<cwSurveyNoteModel*>(sender());

    insertedNotesForTrip(parentNoteModel->parentTrip(), begin, end);
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
    Q_ASSERT(begin <= end);

    Q_ASSERT(qobject_cast<cwSurveyNoteModel*>(sender()) != nullptr);
    cwSurveyNoteModel* noteModel = static_cast<cwSurveyNoteModel*>(sender());

    beginRemoveNotesForTrip(noteModel->parentTrip(), begin, end);
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
    Q_ASSERT(begin <= end);
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
    Q_ASSERT(begin <= end);

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
    Q_ASSERT(begin <= end);

    insertedScrapsForNote(qobject_cast<cwNote*>(sender()), begin, end);
}

/**
 * @brief cwRegionTreeModel::beginRemoveScraps
 * @param begin
 * @param end
 */
void cwRegionTreeModel::beginRemoveScraps(int begin, int end)
{
    Q_ASSERT(qobject_cast<cwNote*>(sender()) != nullptr);
    Q_ASSERT(begin <= end);

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
    Q_ASSERT(begin <= end);

    endRemoveRows();
}

/**
 * @brief cwRegionTreeModel::setCavingRegion
 * @param region
 */
void cwRegionTreeModel::setCavingRegion(cwCavingRegion* region) {
    //Remove all the connections
    if(rowCount() > 0) {
        removeCaveConnections(0, rowCount() - 1);
    }

    //Reset the model
    beginResetModel();
    if(region == nullptr && Region != nullptr) {
        disconnect(Region, nullptr, this, nullptr);
    }

    Region = region;

    if(Region) {
        connect(Region, &cwCavingRegion::rowsAboutToBeInserted, this, &cwRegionTreeModel::beginInsertCaves);
        connect(Region, &cwCavingRegion::rowsInserted, this, &cwRegionTreeModel::insertedCaves);
        connect(Region, &cwCavingRegion::rowsAboutToBeRemoved, this, &cwRegionTreeModel::beginRemoveCaves);
        connect(Region, &cwCavingRegion::rowsRemoved, this, &cwRegionTreeModel::removedCaves);
    }

    endResetModel();

    if(rowCount() > 0) {
        addCaveConnections(0, rowCount() - 1);
    }
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
            if(row == static_cast<int>(TripRows::NotesModel)) {
                return createIndex(row, column, parentTrip->notes());
            } else if(row == static_cast<int>(TripRows::NotesLiDARModel)) {
                return createIndex(row, column, parentTrip->notesLiDAR());
            } else if(row == static_cast<int>(TripRows::NotesSketchModel)) {
                return createIndex(row, column, parentTrip->notesSketch());
            } else {
                //Bad, row isn't a known trip-row model
                Q_ASSERT(false);
            }
        }
        //Bad cast / internal pointer
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

    case NotesType: {
        // Children are cwNote* from the paper notes model
        auto* model = qobject_cast<cwSurveyNoteModel*>((QObject*)parent.internalPointer());
        Q_ASSERT(model);
        if (!model) { return QModelIndex(); }
        if (row >= model->notes().size()) { return QModelIndex(); }
        return createIndex(row, column, model->notes().at(row));
    }

    case NotesLiDARType: {
        // Children are cwNote* from the paper notes model
        auto* model = qobject_cast<cwSurveyNoteLiDARModel*>((QObject*)parent.internalPointer());
        Q_ASSERT(model);
        if (!model) { return QModelIndex(); }
        if (row >= model->notes().size()) { return QModelIndex(); }
        return createIndex(row, column, model->notes().at(row));
    }

    case NotesSketchType: {
        auto* model = qobject_cast<cwSurveyNoteSketchModel*>((QObject*)parent.internalPointer());
        Q_ASSERT(model);
        if (!model) { return QModelIndex(); }
        if (row >= model->notes().size()) { return QModelIndex(); }
        return createIndex(row, column, model->notes().at(row));
    }

    case ScrapType:
    case NoteLiDARType:
    case SketchType:
        // These are leaf nodes; they should not have children
        return QModelIndex();

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

template<typename NoteModel, typename Note>
QModelIndex indexOfNote(const cwRegionTreeModel* thiz, const Note* note) {
    auto parent = qobject_cast<NoteModel*>(note->parent());
    if(parent == nullptr) { return QModelIndex(); }
    int noteIndex = parent->notes().indexOf(note);

    QModelIndex parentIndex = thiz->index(parent);
    if(!parentIndex.isValid()) { return QModelIndex(); }

    if(noteIndex < 0) { return QModelIndex(); }

    return thiz->index(noteIndex, 0, parentIndex);
}

/**
 * @brief cwRegionTreeModel::index
 * @param note
 * @return
 */
QModelIndex cwRegionTreeModel::index(cwNote *note) const
{
    return indexOfNote<cwSurveyNoteModel, cwNote>(this, note);
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

QModelIndex cwRegionTreeModel::index(cwSurveyNoteModel* model) const {
    if(model == nullptr) {
        return QModelIndex();
    }

    cwTrip* parentTrip = model->parentTrip();
    if(parentTrip == nullptr) {
        return QModelIndex();
    }

    QModelIndex tripIndex = index(parentTrip);
    if(!tripIndex.isValid()) {
        return QModelIndex();
    }

    // The NotesType container is always row 0 under Trip
    return index(static_cast<int>(TripRows::NotesModel), 0, tripIndex);
}

QModelIndex cwRegionTreeModel::index(cwSurveyNoteLiDARModel* model) const {
    if(model == nullptr) {
        return QModelIndex();
    }

    cwTrip* parentTrip = model->parentTrip();
    if(parentTrip == nullptr) {
        return QModelIndex();
    }

    QModelIndex tripIndex = index(parentTrip);
    if(!tripIndex.isValid()) {
        return QModelIndex();
    }

    // The NotesLiDARType container is always row 1 under Trip
    return index(static_cast<int>(TripRows::NotesLiDARModel), 0, tripIndex);
}

QModelIndex cwRegionTreeModel::index(cwNoteLiDAR* lidar) const {
    return indexOfNote<cwSurveyNoteLiDARModel, cwNoteLiDAR>(this, lidar);
}

QModelIndex cwRegionTreeModel::index(cwSketch* sketch) const {
    return indexOfNote<cwSurveyNoteSketchModel, cwSketch>(this, sketch);
}

QModelIndex cwRegionTreeModel::index(cwSurveyNoteSketchModel* model) const {
    if(model == nullptr) {
        return QModelIndex();
    }

    cwTrip* parentTrip = model->parentTrip();
    if(parentTrip == nullptr) {
        return QModelIndex();
    }

    QModelIndex tripIndex = index(parentTrip);
    if(!tripIndex.isValid()) {
        return QModelIndex();
    }

    return index(static_cast<int>(TripRows::NotesSketchModel), 0, tripIndex);
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

            auto noteModel = qobject_cast<cwSurveyNoteModel*>(note->parent());
            Q_ASSERT(noteModel);

            int row = noteModel->notes().indexOf(note);
            Q_ASSERT(row >= 0);

            return createIndex(static_cast<int>(TripRows::NotesModel), 0, noteModel);
        }

        Q_ASSERT(false);
        break;
    }

    case NoteLiDARType: {
        cwNoteLiDAR* note = qobject_cast<cwNoteLiDAR*>(static_cast<QObject*>(index.internalPointer()));
        if(note != nullptr) {

            auto noteModel = qobject_cast<cwSurveyNoteLiDARModel*>(note->parent());
            Q_ASSERT(noteModel);

            int row = noteModel->notes().indexOf(note);
            Q_ASSERT(row >= 0);

            return createIndex(static_cast<int>(TripRows::NotesLiDARModel), 0, noteModel);
        }

        Q_ASSERT(false);
        break;
    }

    case SketchType: {
        cwSketch* sketch = qobject_cast<cwSketch*>(static_cast<QObject*>(index.internalPointer()));
        if(sketch != nullptr) {

            auto sketchModel = qobject_cast<cwSurveyNoteSketchModel*>(sketch->parent());
            Q_ASSERT(sketchModel);

            int row = sketchModel->notes().indexOf(sketch);
            Q_ASSERT(row >= 0);

            return createIndex(static_cast<int>(TripRows::NotesSketchModel), 0, sketchModel);
        }

        Q_ASSERT(false);
        break;
    }

    case NotesType: {
        cwSurveyNoteModel* notes = qobject_cast<cwSurveyNoteModel*>(static_cast<QObject*>(index.internalPointer()));
        if(notes != nullptr) {
            auto parentTrip = notes->parentTrip();
            int row = parentTrip->parentCave()->indexOf(parentTrip);
            return createIndex(row, 0, parentTrip);
        }

        Q_ASSERT(false);
        break;

    }

    case NotesLiDARType: {
        cwSurveyNoteLiDARModel* notes = qobject_cast<cwSurveyNoteLiDARModel*>(static_cast<QObject*>(index.internalPointer()));
        if(notes != nullptr) {
            auto parentTrip = notes->parentTrip();
            int row = parentTrip->parentCave()->indexOf(parentTrip);
            return createIndex(row, 0, parentTrip);
        }

        Q_ASSERT(false);
        break;
    }

    case NotesSketchType: {
        cwSurveyNoteSketchModel* sketchModel = qobject_cast<cwSurveyNoteSketchModel*>(static_cast<QObject*>(index.internalPointer()));
        if(sketchModel != nullptr) {
            auto parentTrip = sketchModel->parentTrip();
            int row = parentTrip->parentCave()->indexOf(parentTrip);
            return createIndex(row, 0, parentTrip);
        }

        Q_ASSERT(false);
        break;
    }

    case ScrapType: {
        cwScrap* scrap = qobject_cast<cwScrap*>(static_cast<QObject*>(index.internalPointer()));
        if(scrap != nullptr) {
            cwNote* parentNote = scrap->parentNote();
            int row = parentNote->parentTrip()->notes()->notes().indexOf(parentNote);
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

    case TripType:
        return static_cast<int>(TripRows::NumberOfRows);

    case NotesType: {
        auto* model = qobject_cast<cwSurveyNoteModel*>((QObject*)parent.internalPointer());
        Q_ASSERT(model);
        return model ? model->rowCount() : 0;
    }

    case NotesLiDARType: {
        auto* model = qobject_cast<cwSurveyNoteLiDARModel*>((QObject*)parent.internalPointer());
        Q_ASSERT(model);
        return model ? model->rowCount() : 0;
    }

    case NotesSketchType: {
        auto* model = qobject_cast<cwSurveyNoteSketchModel*>((QObject*)parent.internalPointer());
        Q_ASSERT(model);
        return model ? model->rowCount() : 0;
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

    if (auto* noteModel = qobject_cast<cwSurveyNoteModel*>((QObject*)index.internalPointer())) {
        if (role == ObjectRole) { return QVariant::fromValue(noteModel); }
        if (role == TypeRole)   { return NotesType; }
        return QVariant();
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

    if (auto* lidarModel = qobject_cast<cwSurveyNoteLiDARModel*>((QObject*)index.internalPointer())) {
        if (role == ObjectRole) { return QVariant::fromValue(lidarModel); }
        if (role == TypeRole)   { return NotesLiDARType; }
        return QVariant();
    }

    if (auto* lidarNote = qobject_cast<cwNoteLiDAR*>((QObject*)index.internalPointer())) {
        if (role == ObjectRole) { return QVariant::fromValue(lidarNote); }
        if (role == TypeRole)   { return NoteLiDARType; }
        return QVariant();
    }

    if (auto* sketchModel = qobject_cast<cwSurveyNoteSketchModel*>((QObject*)index.internalPointer())) {
        if (role == ObjectRole) { return QVariant::fromValue(sketchModel); }
        if (role == TypeRole)   { return NotesSketchType; }
        return QVariant();
    }

    if (auto* sketch = qobject_cast<cwSketch*>((QObject*)index.internalPointer())) {
        if (role == ObjectRole) { return QVariant::fromValue(sketch); }
        if (role == TypeRole)   { return SketchType; }
        return QVariant();
    }

    return QVariant();
}

template <typename T>
T* indexToObject(const cwRegionTreeModel* thiz, const QModelIndex& index) {
    QVariant tripVariant = thiz->data(index, cwRegionTreeModel::ObjectRole);
    if(tripVariant.canConvert<T*>()) {
        T* trip = tripVariant.value<T*>();
        return trip;
    }
    return nullptr;
}

/**
  \brief Gets the trip at index

  If index isn't a trip, then this returns null
  */
cwTrip* cwRegionTreeModel::trip(const QModelIndex& index) const {
    return indexToObject<cwTrip>(this, index);
}

/**
  \brief Gets the cave at inde
  */
cwCave* cwRegionTreeModel::cave(const QModelIndex& index) const {
    return indexToObject<cwCave>(this, index);
}

cwNote *cwRegionTreeModel::note(const QModelIndex &index) const
{
    return indexToObject<cwNote>(this, index);
}

cwScrap *cwRegionTreeModel::scrap(const QModelIndex &index) const
{
    return indexToObject<cwScrap>(this, index);
}

cwSurveyNoteModel* cwRegionTreeModel::notesModel(const QModelIndex& index) const {
    return indexToObject<cwSurveyNoteModel>(this, index);
}

cwSurveyNoteLiDARModel* cwRegionTreeModel::notesLiDARModel(const QModelIndex& index) const {
    return indexToObject<cwSurveyNoteLiDARModel>(this, index);
}

cwSurveyNoteSketchModel* cwRegionTreeModel::notesSketchModel(const QModelIndex& index) const {
    return indexToObject<cwSurveyNoteSketchModel>(this, index);
}

cwNoteLiDAR* cwRegionTreeModel::noteLiDAR(const QModelIndex& index) const {
    return indexToObject<cwNoteLiDAR>(this, index);
}

cwSketch* cwRegionTreeModel::sketch(const QModelIndex& index) const {
    return indexToObject<cwSketch>(this, index);
}

QObject *cwRegionTreeModel::object(const QModelIndex &index) const
{
    return data(index, ObjectRole).value<QObject*>();
}

/**
  \brief The per-trip objects the model observes, in the order they are wired.

  addTripConnections() and removeTripConnections() both drive off this single list,
  so the set of objects the model connects can never diverge from the set it
  disconnects. The trip is first: it carries no row signals itself, but its record in
  the connection checker gates the whole trip (issue #576).
  */
QList<QObject*> cwRegionTreeModel::tripConnectionObjects(cwTrip* trip) {
    return {
        trip,
        trip->notes(),
        trip->notesLiDAR(),
        trip->notesSketch()
    };
}

/**
  \brief Wires \a model's flat row signals through to this model's begin/end rows.
  */
template <typename Model>
void cwRegionTreeModel::connectFlatModel(Model* model) {
    connect(model, &Model::rowsAboutToBeInserted,
            this, [this, model](const QModelIndex& parent, int first, int last) {
                Q_UNUSED(parent);
                beginInsertRows(index(model), first, last);
            });
    connect(model, &Model::rowsInserted,
            this, [this](const QModelIndex& parent, int first, int last) {
                Q_UNUSED(parent); Q_UNUSED(first); Q_UNUSED(last);
                endInsertRows();
            });
    connect(model, &Model::rowsAboutToBeRemoved,
            this, [this, model](const QModelIndex& parent, int first, int last) {
                Q_UNUSED(parent);
                beginRemoveRows(index(model), first, last);
            });
    connect(model, &Model::rowsRemoved,
            this, [this](const QModelIndex& parent, int first, int last) {
                Q_UNUSED(parent); Q_UNUSED(first); Q_UNUSED(last);
                endRemoveRows();
            });
}

/**
  \brief Records \a object in the registry and wires its row signals if newly recorded.

  Returns false (wiring nothing) if the object was already recorded — the checker
  emits the duplicate-connection warning in that case.
  */
bool cwRegionTreeModel::connectObject(QObject* object) {
    return m_connectionRegistry.add(object, [this, object]{ wireObjectSignals(object); });
}

/**
  \brief Wires \a object's row signals to this model, selected by its concrete type.
  A cwTrip carries no row signals and is recorded for bookkeeping only.
  */
void cwRegionTreeModel::wireObjectSignals(QObject* object) {
    if(auto* cave = qobject_cast<cwCave*>(object)) {
        connect(cave, &cwCave::rowsAboutToBeInserted,
                this, &cwRegionTreeModel::beginInsertTrips, Qt::UniqueConnection);
        connect(cave, &cwCave::rowsInserted,
                this, &cwRegionTreeModel::insertedTrips, Qt::UniqueConnection);
        connect(cave, &cwCave::rowsAboutToBeRemoved,
                this, &cwRegionTreeModel::beginRemoveTrips, Qt::UniqueConnection);
        connect(cave, &cwCave::rowsRemoved,
                this, &cwRegionTreeModel::removedTrips, Qt::UniqueConnection);
    } else if(auto* notes = qobject_cast<cwSurveyNoteModel*>(object)) {
        connect(notes, &cwSurveyNoteModel::rowsAboutToBeInserted,
                this, &cwRegionTreeModel::beginInsertNotes, Qt::UniqueConnection);
        connect(notes, &cwSurveyNoteModel::rowsInserted,
                this, &cwRegionTreeModel::insertedNotes, Qt::UniqueConnection);
        connect(notes, &cwSurveyNoteModel::rowsAboutToBeRemoved,
                this, &cwRegionTreeModel::beginRemoveNotes, Qt::UniqueConnection);
        connect(notes, &cwSurveyNoteModel::rowsRemoved,
                this, &cwRegionTreeModel::removeNotes, Qt::UniqueConnection);
    } else if(auto* lidars = qobject_cast<cwSurveyNoteLiDARModel*>(object)) {
        connect(lidars, &cwSurveyNoteLiDARModel::rowsAboutToBeInserted,
                this, [this, lidars](const QModelIndex& parent, int first, int last) {
                    Q_UNUSED(parent);
                    QModelIndex parentIndex = index(lidars);
                    beginInsertRows(parentIndex, first, last);
                });
        connect(lidars, &cwSurveyNoteLiDARModel::rowsInserted,
                this, [this](const QModelIndex& parent, int first, int last) {
                    Q_UNUSED(parent); Q_UNUSED(first); Q_UNUSED(last);
                    endInsertRows();
                });
        connect(lidars, &cwSurveyNoteLiDARModel::rowsAboutToBeRemoved,
                this, [this, lidars](const QModelIndex& parent, int first, int last) {
                    Q_UNUSED(parent);
                    QModelIndex parentIndex = index(lidars);
                    beginRemoveRows(parentIndex, first, last);
                });
        connect(lidars, &cwSurveyNoteLiDARModel::rowsRemoved,
                this, [this](const QModelIndex& parent, int first, int last) {
                    Q_UNUSED(parent); Q_UNUSED(first); Q_UNUSED(last);
                    endRemoveRows();
                });
    } else if(auto* sketches = qobject_cast<cwSurveyNoteSketchModel*>(object)) {
        connect(sketches, &cwSurveyNoteSketchModel::rowsAboutToBeInserted,
                this, [this, sketches](const QModelIndex& parent, int first, int last) {
                    Q_UNUSED(parent);
                    QModelIndex parentIndex = index(sketches);
                    beginInsertRows(parentIndex, first, last);
                });
        connect(sketches, &cwSurveyNoteSketchModel::rowsInserted,
                this, [this](const QModelIndex& parent, int first, int last) {
                    Q_UNUSED(parent); Q_UNUSED(first); Q_UNUSED(last);
                    endInsertRows();
                });
        connect(sketches, &cwSurveyNoteSketchModel::rowsAboutToBeRemoved,
                this, [this, sketches](const QModelIndex& parent, int first, int last) {
                    Q_UNUSED(parent);
                    QModelIndex parentIndex = index(sketches);
                    beginRemoveRows(parentIndex, first, last);
                });
        connect(sketches, &cwSurveyNoteSketchModel::rowsRemoved,
                this, [this](const QModelIndex& parent, int first, int last) {
                    Q_UNUSED(parent); Q_UNUSED(first); Q_UNUSED(last);
                    endRemoveRows();
                });
    }
    //cwTrip and anything else: recorded for bookkeeping only, no row signals.
}

/**
  \brief Unrecords \a object from the registry and tears down every connection it has
  to this model. The wholesale disconnect is a no-op for objects (e.g. a cwTrip) that
  were recorded but never wired.
  */
void cwRegionTreeModel::disconnectObject(QObject* object) {
    m_connectionRegistry.remove(object);
}

/**
  \brief Adds all the connection for a cave
  */
void cwRegionTreeModel::addCaveConnections(int beginIndex, int endIndex, bool recusive) {
    for(int i = beginIndex; i <= endIndex; i++) {
        cwCave* cave = Region->cave(i);

        if(!connectObject(cave)) {
            continue;
        }

        if(recusive) {
            addTripConnections(cave, 0, cave->tripCount() - 1);
        }
    }
}

/**
  \brief Removes all the connection for a cave
  */
void cwRegionTreeModel::removeCaveConnections(int beginIndex, int endIndex, bool recursive) {
    for(int i = beginIndex; i <= endIndex; i++) {
        cwCave* cave = Region->cave(i);

        // Mirror addCaveConnections' recursion so a whole-cave teardown (e.g.
        // setCavingRegion) unwinds the trips it added. Callers that already
        // unwound the trips per-row (beginRemoveCaves) pass recursive = false.
        if(recursive && cave->hasTrips()) {
            removeTripConnections(cave, 0, cave->tripCount() - 1);
        }

        disconnectObject(cave);
    }
}

/**
  \brief Adds connection for the trips between beginIndex and endIndex
  */
void cwRegionTreeModel::addTripConnections(cwCave* parentCave, int beginIndex, int endIndex, bool recursive) {
    for(int i = beginIndex; i <= endIndex; i++) {
        cwTrip* currentTrip = parentCave->trip(i);

        const auto objects = tripConnectionObjects(currentTrip);
        Q_ASSERT(!objects.isEmpty() && objects.first() == currentTrip);

        // The trip's own record gates the whole set: if it is already recorded the
        // trip (and its sub-models) was wired on a previous pass, so skip it.
        if(!connectObject(objects.first())) {
            continue;
        }

        for(int j = 1; j < objects.size(); j++) {
            connectObject(objects.at(j));
        }

        if(recursive) {
            addNoteConnections(currentTrip, 0, currentTrip->notes()->notes().size() - 1);
        }
    }
}


/**
  \brief Removes the connections for a trips between beginIndex and endIndex
  */
void cwRegionTreeModel::removeTripConnections(cwCave* parentCave, int beginIndex, int endIndex, bool recursive) {
    for(int i = beginIndex; i <= endIndex; i++) {
        cwTrip* trip = parentCave->trip(i);

        // Mirror addTripConnections' recursion so a whole-trip teardown unwinds the
        // paper-note rows it added. Callers that already unwound the notes per-row
        // (beginRemoveTrips) pass recursive = false.
        if(recursive) {
            const int noteCount = trip->notes()->notes().size();
            if(noteCount > 0) {
                removeNoteConnections(trip, 0, noteCount - 1);
            }
        }

        // Tear down exactly what addTripConnections wired, driven off the same list.
        // Removing only a subset (issue #576) left models recorded as connected, so
        // re-adding the same trip (sync checkout / undo) tripped the "already
        // connected" guard and skipped re-wiring the models.
        for(QObject* object : tripConnectionObjects(trip)) {
            disconnectObject(object);
        }
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

        m_connectionRegistry.add(note, [this, note]{
            connect(note, &cwNote::beginInsertingScraps,
                    this, &cwRegionTreeModel::beginInsertScraps, Qt::UniqueConnection);
            connect(note, &cwNote::insertedScraps,
                    this, &cwRegionTreeModel::insertedScraps, Qt::UniqueConnection);
            connect(note, &cwNote::beginRemovingScraps,
                    this, &cwRegionTreeModel::beginRemoveScraps, Qt::UniqueConnection);
            connect(note, &cwNote::removedScraps,
                    this, &cwRegionTreeModel::removedScraps, Qt::UniqueConnection);
        });
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

        m_connectionRegistry.remove(note);
    }
}

/**
 * @brief cwRegionTreeModel::beginRemoveTripsForCave
 * @param parentCave
 * @param begin
 * @param end
 */
void cwRegionTreeModel::beginRemoveTripsForCave(cwCave *parentCave, int begin, int end)
{
    Q_ASSERT(begin <= end);
    QModelIndex parentIndex = index(parentCave);

    for(int i = begin; i <= end; i++) {
        cwTrip* trip = index(i, 0, parentIndex).data(ObjectRole).value<cwTrip*>();
        if(trip->notes()->rowCount() > 0) {
            beginRemoveNotesForTrip(trip, 0, trip->notes()->rowCount() - 1);
            endRemoveRows(); //beginRemoveTrips() starts the endRemoveRows
        }
    }

    removeTripConnections(parentCave, begin, end, false); //notes already unwound above
    beginRemoveRows(parentIndex, begin, end);
}

/**
 * @brief cwRegionTreeModel::beginRemoveNotesForTrip
 * @param parentTrip
 * @param begin
 * @param end
 */
void cwRegionTreeModel::beginRemoveNotesForTrip(cwTrip *parentTrip, int begin, int end)
{
    Q_ASSERT(begin <= end);
    QModelIndex parentIndex = index(parentTrip->notes());

    for(int i = begin; i <= end; i++) {
        cwNote* note = index(i, 0, parentIndex).data(ObjectRole).value<cwNote*>();
        if(note->hasScraps()) {
            beginRemoveScrapsForNote(note, 0, note->scraps().size() - 1);
            endRemoveRows();
        }
    }

    removeNoteConnections(parentTrip, begin, end);
    beginRemoveRows(parentIndex, begin, end);
}

/**
 * @brief cwRegionTreeModel::beginRemoveScrapsForNote
 * @param parentNote
 * @param begin
 * @param end
 */
void cwRegionTreeModel::beginRemoveScrapsForNote(cwNote *parentNote, int begin, int end)
{
    Q_ASSERT(begin <= end);
    QModelIndex parentIndex = index(parentNote);

    beginRemoveRows(parentIndex, begin, end);
}

/**
 * @brief cwRegionTreeModel::insertedTripsForCave
 * @param parentCave
 * @param begin
 * @param end
 */
void cwRegionTreeModel::insertedTripsForCave(cwCave *parentCave, int begin, int end)
{
    Q_ASSERT(begin <= end);
    addTripConnections(parentCave, begin, end, false);
    endInsertRows();

    for(int i = begin; i <= end; i++) {
        cwTrip* trip = parentCave->trip(i);
        int lastIndex = trip->notes()->rowCount() - 1;
        if(lastIndex >= 0) {
            QModelIndex parenIndex = index(trip->notes());
            beginInsertRows(parenIndex, 0, lastIndex);
            insertedNotesForTrip(trip, 0, lastIndex);
        }

        // Sketches loaded into a pre-existing trip (on project reload) must
        // emit rowsInserted so listeners like cwScrapManager and
        // cwSketchManager discover them — they subscribe to the sketches
        // model's rowsInserted above, but that signal never fires for
        // entries that were already in the model when the connection was
        // made.
        auto* sketches = trip->notesSketch();
        const int sketchLast = sketches ? sketches->rowCount() - 1 : -1;
        if(sketchLast >= 0) {
            QModelIndex sketchParent = index(sketches);
            beginInsertRows(sketchParent, 0, sketchLast);
            endInsertRows();
        }
    }

}

/**
 * @brief cwRegionTreeModel::insertedNotesForTrip
 * @param parentTrip
 * @param begin
 * @param end
 */
void cwRegionTreeModel::insertedNotesForTrip(cwTrip *parentTrip, int begin, int end)
{
    Q_ASSERT(begin <= end);

    addNoteConnections(parentTrip, begin, end);
    endInsertRows();

    for(int i = begin; i <= end; i++) {
        cwNote* note = parentTrip->notes()->notes().at(i);
        int lastIndex = note->scraps().size() - 1;
        if(lastIndex >= 0) {
            QModelIndex parentNoteIndex = index(note);
            beginInsertRows(parentNoteIndex, 0, lastIndex);
            insertedScrapsForNote(note, 0, lastIndex);
        }
    }

}

/**
 * @brief cwRegionTreeModel::insertedScrapsForNote
 * @param parentNote
 * @param begin
 * @param end
 */
void cwRegionTreeModel::insertedScrapsForNote(cwNote *parentNote, int begin, int end)
{
    Q_UNUSED(parentNote);
    Q_UNUSED(begin);
    Q_UNUSED(end);
    Q_ASSERT(begin <= end);
    endInsertRows();
}
