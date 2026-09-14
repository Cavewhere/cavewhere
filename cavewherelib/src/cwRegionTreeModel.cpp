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
#include "cwSurveyNode.h"
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
 * @brief cwRegionTreeModel::beginInsertNodes
 *
 * Child nodes of \a parentNode are about to appear at rows \a begin to \a end.
 * A node lists its child-node rows before its trip rows, so a child node's row
 * in this model is its row in the parent's child list.
 */
void cwRegionTreeModel::beginInsertNodes(cwSurveyNode* parentNode, int begin, int end)
{
    Q_ASSERT(begin <= end);
    beginInsertRows(index(parentNode), begin, end);
}

/**
 * @brief cwRegionTreeModel::insertedNodes
 */
void cwRegionTreeModel::insertedNodes(cwSurveyNode* parentNode, int begin, int end)
{
    Q_ASSERT(begin <= end);

    for(int i = begin; i <= end; i++) {
        addNodeConnections(parentNode->childNode(i), false);
    }

    endInsertRows();

    for(int i = begin; i <= end; i++) {
        insertedExistingChildren(parentNode->childNode(i));
    }
}

/**
 * @brief cwRegionTreeModel::insertedExistingChildren
 *
 * A node usually arrives with children already in it — a cave loaded from disk
 * brings its trips, and a whole subtree moves in one insert. Those rows never
 * emit an insert of their own, so this announces them: cwScrapManager and
 * cwSketchManager discover a loaded project's scraps and sketches through these
 * inserts alone.
 *
 * Qt's own contract says a parent row carries its subtree in with it, so these
 * announcements come after the children are already reachable and leave
 * rowCount() unchanged across the begin/end pair. QAbstractItemModelTester
 * therefore rejects this path, and the tests put a tester over a tree the model
 * already holds rather than over a subtree joining it. removeChildRows() is the
 * mirror image. Retiring both is what makes the model tester-clean, and that
 * belongs with whatever gives those managers another way to discover rows.
 */
void cwRegionTreeModel::insertedExistingChildren(cwSurveyNode* node)
{
    const QModelIndex nodeIndex = index(node);

    if(node->childNodeCount() > 0) {
        beginInsertRows(nodeIndex, 0, node->childNodeCount() - 1);
        insertedNodes(node, 0, node->childNodeCount() - 1);
    }

    if(node->tripCount() > 0) {
        const int firstRow = firstTripRow(node);
        beginInsertRows(nodeIndex, firstRow, firstRow + node->tripCount() - 1);
        insertedTrips(node, 0, node->tripCount() - 1);
    }
}

/**
 * @brief cwRegionTreeModel::beginRemoveNodes
 */
void cwRegionTreeModel::beginRemoveNodes(cwSurveyNode* parentNode, int begin, int end)
{
    Q_ASSERT(begin <= end);

    for(int i = begin; i <= end; i++) {
        removeChildRows(parentNode->childNode(i));
    }

    removeNodeConnections(parentNode, begin, end);
    beginRemoveRows(index(parentNode), begin, end);
}

/**
 * @brief cwRegionTreeModel::removeChildRows
 *
 * Takes out the rows \a node holds, deepest first, so a subtree leaves the model
 * from the bottom up. The mirror of insertedExistingChildren(), and it announces
 * rows the data keeps for the same reason.
 */
void cwRegionTreeModel::removeChildRows(cwSurveyNode* node)
{
    if(node->childNodeCount() > 0) {
        beginRemoveNodes(node, 0, node->childNodeCount() - 1);
        endRemoveRows(); //beginRemoveNodes() starts the endRemoveRows
    }

    if(node->tripCount() > 0) {
        beginRemoveTrips(node, 0, node->tripCount() - 1);
        endRemoveRows(); //beginRemoveTrips() starts the endRemoveRows
    }
}

/**
 * @brief cwRegionTreeModel::beginInsertTrips
 */
void cwRegionTreeModel::beginInsertTrips(cwSurveyNode* parentNode, int begin, int end)
{
    Q_ASSERT(begin <= end);
    const int firstRow = firstTripRow(parentNode);
    beginInsertRows(index(parentNode), begin + firstRow, end + firstRow);
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
    Q_ASSERT(begin <= end);

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
    //Drop every connection the old region's tree holds. Row removal takes the
    //deeper rows out first and so unwires a level at a time, but swapping the
    //region skips that walk, and a still-wired trip left behind would resolve
    //its index against the new region's root.
    if(!Region.isNull()) {
        removeSubtreeConnections(Region->rootNode());
        disconnect(Region, nullptr, this, nullptr);
    }

    //Reset the model
    beginResetModel();

    Region = region;

    endResetModel();

    if(!Region.isNull()) {
        addNodeConnections(Region->rootNode(), true);
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

    //Under a node come its child-node rows, then its trip rows
    if(cwSurveyNode* parentNode = nodeForIndex(parent)) {
        if(row < parentNode->childNodeCount()) {
            return createIndex(row, column, parentNode->childNode(row));
        }

        const int tripRow = row - firstTripRow(parentNode);
        if(tripRow < parentNode->tripCount()) {
            return createIndex(row, column, parentNode->trip(tripRow));
        }

        return QModelIndex();
    }

    switch(parent.data(TypeRole).toInt()) {
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
  \brief Gets the node index of the model

  The region's root node owns no row of its own — its children are the top-level
  rows — so the root, and any node outside this model's region, return
  QModelIndex().
  */
QModelIndex cwRegionTreeModel::index (cwSurveyNode* node) const {
    if(node == nullptr) { return QModelIndex(); }
    if(node->isRoot()) { return QModelIndex(); }
    if(!isInTree(node)) { return QModelIndex(); }

    const int row = rowOf(node);
    if(row < 0) { return QModelIndex(); }

    return createIndex(row, 0, node);
}

/**
  \brief Gets the trip index of the model

  If the trip doesn't exist in the model this return QModelIndex()
  */
QModelIndex cwRegionTreeModel::index (cwTrip* trip) const {
    if(trip == nullptr) { return QModelIndex(); }
    if(!isInTree(trip->parentNode())) { return QModelIndex(); }

    const int row = rowOf(trip);
    if(row < 0) { return QModelIndex(); }

    return createIndex(row, 0, trip);
}

/**
  \brief The node an index stands for, the region's root for the invalid index

  Every other row answers nullptr.
  */
cwSurveyNode* cwRegionTreeModel::nodeForIndex(const QModelIndex& index) const
{
    if(Region.isNull()) { return nullptr; }
    if(!index.isValid()) { return Region->rootNode(); }
    return qobject_cast<cwSurveyNode*>(static_cast<QObject*>(index.internalPointer()));
}

/**
  \brief Checks that \a node hangs off this model's region root
  */
bool cwRegionTreeModel::isInTree(const cwSurveyNode* node) const
{
    if(Region.isNull() || node == nullptr) { return false; }

    const cwSurveyNode* root = Region->rootNode();
    for(const cwSurveyNode* current = node; current != nullptr; current = current->parentNode()) {
        if(current == root) { return true; }

        //A removed node keeps pointing at the parent it came from so undo can
        //put it back. Only a node its parent still lists owns a row here.
        if(!current->isListedByParent()) { return false; }
    }
    return false;
}

/**
  \brief The row \a node takes under its parent
  */
int cwRegionTreeModel::rowOf(cwSurveyNode* node) const
{
    cwSurveyNode* parentNode = node->parentNode();
    if(parentNode == nullptr) { return -1; }
    return parentNode->indexOfNode(node);
}

/**
  \brief The row \a trip takes under its node, the child nodes counted ahead of it
  */
int cwRegionTreeModel::rowOf(cwTrip* trip) const
{
    cwSurveyNode* parentNode = trip->parentNode();
    if(parentNode == nullptr) { return -1; }

    const int tripIndex = parentNode->indexOf(trip);
    if(tripIndex < 0) { return -1; }

    return firstTripRow(parentNode) + tripIndex;
}

/**
  \brief The row \a node's first trip takes, which its child-node rows come ahead of
  */
int cwRegionTreeModel::firstTripRow(const cwSurveyNode* node) const
{
    return node->childNodeCount();
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

    case NodeType: {
        cwSurveyNode* node = nodeForIndex(index);
        if(node != nullptr) {
            return this->index(node->parentNode());
        }

        Q_ASSERT(false);
        break;
    }

    case TripType: {

        cwTrip* trip = qobject_cast<cwTrip*>(static_cast<QObject*>(index.internalPointer()));
        if(trip != nullptr) {
            return this->index(trip->parentNode());
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
            return createIndex(rowOf(parentTrip), 0, parentTrip);
        }

        Q_ASSERT(false);
        break;

    }

    case NotesLiDARType: {
        cwSurveyNoteLiDARModel* notes = qobject_cast<cwSurveyNoteLiDARModel*>(static_cast<QObject*>(index.internalPointer()));
        if(notes != nullptr) {
            auto parentTrip = notes->parentTrip();
            return createIndex(rowOf(parentTrip), 0, parentTrip);
        }

        Q_ASSERT(false);
        break;
    }

    case NotesSketchType: {
        cwSurveyNoteSketchModel* sketchModel = qobject_cast<cwSurveyNoteSketchModel*>(static_cast<QObject*>(index.internalPointer()));
        if(sketchModel != nullptr) {
            auto parentTrip = sketchModel->parentTrip();
            return createIndex(rowOf(parentTrip), 0, parentTrip);
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

    //A node's rows are its child nodes followed by its trips
    if(cwSurveyNode* node = nodeForIndex(parent)) {
        return node->childNodeCount() + node->tripCount();
    }

    switch(parent.data(TypeRole).toInt()) {
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

    cwSurveyNode* currentNode = qobject_cast<cwSurveyNode*>(static_cast<QObject*>(index.internalPointer()));
    if(currentNode != nullptr) {
        switch(role) {
        case ObjectRole:
            return QVariant::fromValue(currentNode);
        case TypeRole:
            return NodeType;
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
  \brief Gets the survey node at index

  If index isn't a node, then this returns null
  */
cwSurveyNode* cwRegionTreeModel::node(const QModelIndex& index) const {
    return indexToObject<cwSurveyNode>(this, index);
}

/**
  \brief Gets the cave at index

  Every node is a cwCave until the shim retires, so this is the node at index.
  */
cwCave* cwRegionTreeModel::cave(const QModelIndex& index) const {
    return qobject_cast<cwCave*>(node(index));
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
  \brief Wires \a node's own row signals, and its whole subtree when recursive

  A node reports two row groups of its own — its child nodes and its trips — so
  the same eight connections serve every level of the tree, the region's root
  node included.
  */
void cwRegionTreeModel::addNodeConnections(cwSurveyNode* node, bool recursive) {
    if(node == nullptr) { return; }

    if(!m_connectionChecker.add(node)) {
        return;
    }

    if(node->isRoot()) {
        //The region is the list model over the root's children, and it relays
        //the root's node-row signals to its own rows signals. This model takes
        //the root's rows from the region so that it keeps announcing them ahead
        //of the page system and the QML views: those subscribe to the region
        //after this model does, and one of them re-reads a whole cave when it
        //appears. Announcing the cave's rows after that read would show its
        //notes to a listener that already has them.
        connect(Region, &QAbstractItemModel::rowsAboutToBeInserted,
                this, [this, node](const QModelIndex&, int begin, int end) { beginInsertNodes(node, begin, end); });
        connect(Region, &QAbstractItemModel::rowsInserted,
                this, [this, node](const QModelIndex&, int begin, int end) { insertedNodes(node, begin, end); });
        connect(Region, &QAbstractItemModel::rowsAboutToBeRemoved,
                this, [this, node](const QModelIndex&, int begin, int end) { beginRemoveNodes(node, begin, end); });
        connect(Region, &QAbstractItemModel::rowsRemoved,
                this, [this] { endRemoveRows(); });
    } else {
        connect(node, &cwSurveyNode::beginInsertNodes,
                this, [this, node](int begin, int end) { beginInsertNodes(node, begin, end); });
        connect(node, &cwSurveyNode::insertedNodes,
                this, [this, node](int begin, int end) { insertedNodes(node, begin, end); });
        connect(node, &cwSurveyNode::beginRemoveNodes,
                this, [this, node](int begin, int end) { beginRemoveNodes(node, begin, end); });
        connect(node, &cwSurveyNode::removedNodes,
                this, [this] { endRemoveRows(); });
    }

    connect(node, &cwSurveyNode::beginInsertTrips,
            this, [this, node](int begin, int end) { beginInsertTrips(node, begin, end); });
    connect(node, &cwSurveyNode::insertedTrips,
            this, [this, node](int begin, int end) { insertedTrips(node, begin, end); });
    connect(node, &cwSurveyNode::beginRemoveTrips,
            this, [this, node](int begin, int end) { beginRemoveTrips(node, begin, end); });
    connect(node, &cwSurveyNode::removedTrips,
            this, [this] { endRemoveRows(); });

    if(recursive) {
        addTripConnections(node, 0, node->tripCount() - 1);

        const QList<cwSurveyNode*> children = node->childNodes();
        for(cwSurveyNode* child : children) {
            addNodeConnections(child, true);
        }
    }
}

/**
  \brief Removes the connections for the child nodes between beginIndex and endIndex

  The children's own rows are taken out by beginRemoveNodes() before this runs,
  so this drops one level only.
  */
void cwRegionTreeModel::removeNodeConnections(cwSurveyNode* parentNode, int beginIndex, int endIndex) {
    for(int i = beginIndex; i <= endIndex; i++) {
        cwSurveyNode* node = parentNode->childNode(i);
        m_connectionChecker.remove(node);
        disconnect(node, nullptr, this, nullptr); //disconnect signals and slots to this object
    }
}

/**
  \brief Drops the connections \a node and everything under it hold to this model
  */
void cwRegionTreeModel::removeSubtreeConnections(cwSurveyNode* node) {
    if(node == nullptr) { return; }

    const QList<cwSurveyNode*> children = node->childNodes();
    for(cwSurveyNode* child : children) {
        removeSubtreeConnections(child);
    }

    for(int i = 0; i < node->tripCount(); i++) {
        cwTrip* trip = node->trip(i);
        const int noteCount = trip->notes()->notes().size();
        if(noteCount > 0) {
            removeNoteConnections(trip, 0, noteCount - 1);
        }
    }

    if(node->tripCount() > 0) {
        removeTripConnections(node, 0, node->tripCount() - 1);
    }

    m_connectionChecker.remove(node);
    disconnect(node, nullptr, this, nullptr);
}

/**
  \brief Adds connection for the trips between beginIndex and endIndex
  */
void cwRegionTreeModel::addTripConnections(cwSurveyNode* parentNode, int beginIndex, int endIndex, bool recursive) {
    for(int i = beginIndex; i <= endIndex; i++) {
        cwTrip* currentTrip = parentNode->trip(i);

        if(!m_connectionChecker.add(currentTrip)) {
            continue;
        }

        { //Add the notes
            auto notes = currentTrip->notes();

            if(!m_connectionChecker.add(notes)) {
                continue;
            }

            connect(notes, SIGNAL(rowsAboutToBeInserted(QModelIndex,int,int)),
                    this, SLOT(beginInsertNotes(QModelIndex,int,int)), Qt::UniqueConnection);
            connect(notes, SIGNAL(rowsInserted(QModelIndex,int,int)),
                    this, SLOT(insertedNotes(QModelIndex,int,int)), Qt::UniqueConnection);
            connect(notes, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)),
                    this, SLOT(beginRemoveNotes(QModelIndex,int,int)), Qt::UniqueConnection);
            connect(notes, SIGNAL(rowsRemoved(QModelIndex,int,int)),
                    this, SLOT(removeNotes(QModelIndex,int,int)), Qt::UniqueConnection);

            if(recursive) {
                addNoteConnections(currentTrip, 0, notes->notes().size() - 1);
            }
        }

        { // --- LiDAR notes (flat model) ---
            auto* lidars = currentTrip->notesLiDAR();

            if(!m_connectionChecker.add(lidars)) {
                continue;
            }

            Q_ASSERT(lidars);
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
                    this, [this, lidars](const QModelIndex& parent, int first, int last) {
                        Q_UNUSED(parent); Q_UNUSED(first); Q_UNUSED(last);
                        endRemoveRows();
                    });
        }

        { // --- Sketches (flat model) ---
            auto* sketches = currentTrip->notesSketch();

            if(!m_connectionChecker.add(sketches)) {
                continue;
            }

            Q_ASSERT(sketches);
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
                    this, [this, sketches](const QModelIndex& parent, int first, int last) {
                        Q_UNUSED(parent); Q_UNUSED(first); Q_UNUSED(last);
                        endRemoveRows();
                    });
        }

    }
}


/**
  \brief Removes the connections for a trips between beginIndex and endIndex
  */
void cwRegionTreeModel::removeTripConnections(cwSurveyNode* parentNode, int beginIndex, int endIndex) {
    for(int i = beginIndex; i <= endIndex; i++) {
        cwTrip* trip = parentNode->trip(i);

        m_connectionChecker.remove(trip);

        disconnect(trip, nullptr, this, nullptr); //disconnect signals and slots to this object
        disconnect(trip->notes(), nullptr, this, nullptr);
        disconnect(trip->notesLiDAR(), nullptr, this, nullptr);
        disconnect(trip->notesSketch(), nullptr, this, nullptr);
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

        if(!m_connectionChecker.add(note)) {
            continue;
        }

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

        m_connectionChecker.remove(note);

        disconnect(note, 0, this, 0);
    }
}

/**
 * @brief cwRegionTreeModel::beginRemoveTrips
 * @param parentNode
 * @param begin
 * @param end
 */
void cwRegionTreeModel::beginRemoveTrips(cwSurveyNode *parentNode, int begin, int end)
{
    Q_ASSERT(begin <= end);

    for(int i = begin; i <= end; i++) {
        cwTrip* trip = parentNode->trip(i);
        if(trip->notes()->rowCount() > 0) {
            beginRemoveNotes(trip, 0, trip->notes()->rowCount() - 1);
            endRemoveRows(); //beginRemoveNotes() starts the endRemoveRows
        }
    }

    removeTripConnections(parentNode, begin, end);

    const int firstRow = firstTripRow(parentNode);
    beginRemoveRows(index(parentNode), begin + firstRow, end + firstRow);
}

/**
 * @brief cwRegionTreeModel::beginRemoveNotes
 * @param parentTrip
 * @param begin
 * @param end
 */
void cwRegionTreeModel::beginRemoveNotes(cwTrip *parentTrip, int begin, int end)
{
    Q_ASSERT(begin <= end);
    QModelIndex parentIndex = index(parentTrip->notes());

    for(int i = begin; i <= end; i++) {
        cwNote* note = index(i, 0, parentIndex).data(ObjectRole).value<cwNote*>();
        if(note->hasScraps()) {
            beginRemoveScraps(note, 0, note->scraps().size() - 1);
            endRemoveRows();
        }
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
    Q_ASSERT(begin <= end);
    QModelIndex parentIndex = index(parentNote);

    beginRemoveRows(parentIndex, begin, end);
}

/**
 * @brief cwRegionTreeModel::insertedTrips
 * @param parentNode
 * @param begin
 * @param end
 */
void cwRegionTreeModel::insertedTrips(cwSurveyNode *parentNode, int begin, int end)
{
    Q_ASSERT(begin <= end);
    addTripConnections(parentNode, begin, end, false);
    endInsertRows();

    for(int i = begin; i <= end; i++) {
        cwTrip* trip = parentNode->trip(i);
        int lastIndex = trip->notes()->rowCount() - 1;
        if(lastIndex >= 0) {
            QModelIndex parenIndex = index(trip->notes());
            beginInsertRows(parenIndex, 0, lastIndex);
            insertedNotes(trip, 0, lastIndex);
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
 * @brief cwRegionTreeModel::insertedNotes
 * @param parentTrip
 * @param begin
 * @param end
 */
void cwRegionTreeModel::insertedNotes(cwTrip *parentTrip, int begin, int end)
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
    Q_ASSERT(begin <= end);
    endInsertRows();
}
