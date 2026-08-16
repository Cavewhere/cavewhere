/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwCoordinateTransform.h"
#include "cwDebug.h"
#include "cwFixStationValidator.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"
#include "cwLocalProjectionManager.h"
#include "cwLocalProjectionToken.h"
#include "cwProject.h"
#include "cwData.h"
#include "cwNameUtils.h"

//Qt includes
#include <QThread>
#include <QDebug>

cwCavingRegion::cwCavingRegion(QObject *parent) :
    QAbstractListModel(parent),
    m_geoReference(new cwGeoReference(this)),
    m_lazLayers(new cwLazLayerModel(this)),
    m_fixStationValidator(new cwFixStationValidator(this)),
    m_localProjectionManager(new cwLocalProjectionManager(this))
{
    // Every GIS layer loads into the frame, and the frame is derived from what
    // those layers say about themselves — so each one is handed the manager it
    // reads the frame off, and waits on, before it decodes.
    m_lazLayers->setLocalProjectionToken(cwLocalProjectionToken(m_localProjectionManager));

    // geoReference owns the frame; the region tells the things it owns when it
    // moves — the LAZ layers, whose points are in the frame they were decoded
    // into, and every cave's grid convergence, which is an angle in the frame
    // and so moves with it. A cave can't watch the frame itself: it learns which
    // region it belongs to only when it is parented, so joining is the other
    // half — a cave converges to nothing until it has a region to read the frame
    // off. Consumers the region doesn't own connect to geoReference directly.
    const auto recomputeConvergence = [this] {
        for (cwCave* cave : std::as_const(m_caves)) {
            cave->recomputeGridConvergence();
        }
    };

    // Re-decoding a directory of point clouds is the most expensive thing the
    // frame can cause, so it hangs off the narrower signal: a freeze or a change
    // of anchor leaves every coordinate where it was.
    connect(m_geoReference, &cwGeoReference::localCoordinateSystemChanged,
            m_lazLayers, &cwLazLayerModel::reloadAll);
    connect(m_geoReference, &cwGeoReference::localProjectionChanged, this, recomputeConvergence);
    connect(this, &cwCavingRegion::caveCountChanged, this, recomputeConvergence);

    // What the default datum is derived from: the layers and the frame. Rows
    // coming and going, and the two roles the ladder reads, are the whole of
    // the layer half — a point count landing says nothing about a datum.
    connect(m_lazLayers, &QAbstractItemModel::rowsInserted,
            this, &cwCavingRegion::defaultFixDatumChanged);
    connect(m_lazLayers, &QAbstractItemModel::rowsRemoved,
            this, &cwCavingRegion::defaultFixDatumChanged);
    connect(m_lazLayers, &QAbstractItemModel::modelReset,
            this, &cwCavingRegion::defaultFixDatumChanged);
    connect(m_lazLayers, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex&, const QModelIndex&, const QList<int>& roles) {
        if (roles.isEmpty()
            || roles.contains(cwLazLayerModel::SourceCSRole)
            || roles.contains(cwLazLayerModel::EnabledRole)) {
            emit defaultFixDatumChanged();
        }
    });
    connect(m_geoReference, &cwGeoReference::localCoordinateSystemChanged,
            this, &cwCavingRegion::defaultFixDatumChanged);
}

QString cwCavingRegion::defaultFixSourceCS() const
{
    for (const cwLazLayer* layer : m_lazLayers->layers()) {
        if (layer->enabled()
            && !cwCoordinateTransform::geographicDatumFor(layer->sourceCS()).isEmpty()) {
            return layer->sourceCS();
        }
    }

    const QString frameCS = m_geoReference->localCoordinateSystem();
    if (!cwCoordinateTransform::geographicDatumFor(frameCS).isEmpty()) {
        return frameCS;
    }

    return QString();
}

QString cwCavingRegion::defaultFixDatum() const
{
    const QString datum = cwCoordinateTransform::geographicDatumFor(defaultFixSourceCS());
    return datum.isEmpty() ? cwCoordinateTransform::Wgs84 : datum;
}

void cwCavingRegion::setUnitSystem(cwUnits::UnitSystem system)
{
    if (m_unitSystem == system) {
        return;
    }
    m_unitSystem = system;
    emit unitSystemChanged();
}

void cwCavingRegion::setFutureManagerToken(const cwFutureManagerToken& token)
{
    m_lazLayers->setFutureManagerToken(token);
}

// /**
//   \brief Copy constructor
//   */
// cwCavingRegion::cwCavingRegion(const cwCavingRegion& object) :
//     QAbstractListModel(nullptr),
//     cwUndoer(object.undoStack())
// {
//     copy(object);
// }

// /**
//   \brief Alignment operator
//   */
// cwCavingRegion& cwCavingRegion::operator=(const cwCavingRegion& object) {
//     return copy(object);
// }

/**
 * @brief cwCavingRegion::rowCount
 * @param parent
 * @return
 */
int cwCavingRegion::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return caveCount();
}

/**
 * @brief cwCavingRegion::data
 * @param index
 * @param role
 * @return
 */
QVariant cwCavingRegion::data(const QModelIndex &index, int role) const
{
    if(!index.isValid()) {
        return QVariant();
    }

    switch(role) {
    case CaveObjectRole:
        return QVariant::fromValue(m_caves.at(index.row()));
    }

    return QVariant();
}

/**
 * @brief cwCavingRegion::roleNames
 * @return
 */
QHash<int, QByteArray> cwCavingRegion::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(CaveObjectRole, "caveObjectRole");
    return roles;
}

/**
 * @brief cwCavingRegion::index
 * @param row
 * @param column
 * @param parent
 * @return
 */
QModelIndex cwCavingRegion::index(int row, int column, const QModelIndex &parent) const
{
    return QAbstractListModel::index(row, column, parent);
}

// /**
//   \brief Copy's the object into this object
//   */
// cwCavingRegion& cwCavingRegion::copy(const cwCavingRegion& object) {
//     Q_ASSERT(object.thread() == thread() || object.thread() == nullptr || thread() == nullptr);
//     Q_ASSERT(QThread::currentThread() == thread() || thread() == nullptr);

//     if(&object == this) {
//         return *this;
//     }

//     //Clear old caves
//     int lastIndex = m_caves.size() - 1;
//     removeCaves(0, lastIndex);

//     if(!object.m_caves.isEmpty()) {
//         emit beginInsertCaves(0, object.m_caves.size() - 1);
//         emit beginInsertRows(QModelIndex(), 0, object.m_caves.size() - 1);
//     }

//     //Add new caves
//     m_caves.reserve(object.m_caves.size());
//     foreach(cwCave* cave, object.m_caves) {

//         //Strange copying to make sure the newCaves are
//         //On the correct thread
//         bool threadIsNull = thread() == nullptr;
//         if(threadIsNull) {
//             moveToThread(QThread::currentThread());
//         }

//         cwCave* newCave = new cwCave(*cave);
//         newCave->setParent(this);  //Uncomment because this cause problems with QML

//         if(threadIsNull) {
//             moveToThread(nullptr);
//         }

//         m_caves.append(newCave);
//     }

//     if(m_caves.size() - 1 >= 0) {
//         emit insertedCaves(0, m_caves.size() -1);
//         emit endInsertRows();
//         emit caveCountChanged();
//     }

//     return *this;
// }


/**
  \brief Creates a new cave and adds it to the caving region
  */
void cwCavingRegion::addCaveHelper() {
    QString newCaveName = QString("Cave %1").arg(caveCount() + 1);
    beginUndoMacro(QString("Add %1").arg(newCaveName));

    cwCave* cave = new cwCave();
    cave->setUndoStack(undoStack());
    cave->setName(newCaveName);
    addCave(cave);

    endUndoMacro();
}

/**
  \brief Adds a cave to the region
  */
void cwCavingRegion::addCave(cwCave* cave) {
    if(cave == nullptr) {
        addCaveHelper();
        return;
    };
    insertCave(m_caves.size(), cave);
}

void cwCavingRegion::addCaves(QList<cwCave*> caves) {
    // Use a temporary set for batch dedup without modifying m_caveNames
    cwSanitizedNameSet batchSet = m_caveNames;
    foreach(cwCave* cave, caves) {
        unparentCave(cave);
        cave->setUndoStack(undoStack());

        // Auto-rename to avoid filesystem path collisions in .cwproj layout
        const QString deduped = batchSet.deduplicateName(cave->name());
        if (deduped != cave->name()) {
            cave->setName(deduped);
        }
        batchSet.insert(cave->name());
    }

    //Run the insert cave command
    if(!caves.isEmpty()) {
        int firstIndex = m_caves.size();
        pushUndo(new InsertCaveCommand(this, caves, firstIndex));
    }
}

/**
  \brief Inserts a cave into the region at inedx
  */
void cwCavingRegion::insertCave(int index, cwCave* cave) {
    if(index < 0 || index > m_caves.size()) { return; }

    unparentCave(cave);

    // Auto-rename to avoid filesystem path collisions in .cwproj layout.
    // The cave has no parent yet, so setName()'s guard won't fire.
    const QString deduped = m_caveNames.deduplicateName(cave->name());
    if (deduped != cave->name()) {
        cave->setName(deduped);
    }

    //Run the insert cave command
    pushUndo(new InsertCaveCommand(this, cave, index));
}

/**
  \brief Removes the cave at index
  */
void cwCavingRegion::removeCave(int index) {
    removeCaves(index, index);
}

/**
  \brief Remove all the caves between beginIndex and the endIndex

  The caves will be delete at a later time
  */
void cwCavingRegion::removeCaves(int beginIndex, int endIndex) {
    //Make sure the indexes are good
    if(beginIndex < 0 || beginIndex >= m_caves.size() ||
            endIndex < 0 || endIndex >= m_caves.size()) {
        return;
    }

    //The beginIndex needs to be greater than the end index
    if(beginIndex > endIndex) {
        return;
    }

    pushUndo(new RemoveCaveCommand(this, beginIndex, endIndex));
}

/**
  \brief Removes all the caves from the region
  */
void cwCavingRegion::clearCaves() {
    if(!m_caves.isEmpty()) {
        removeCaves(0, m_caves.size() - 1);
    }
}

/**
  \brief Get's the index of the cave
  */
int cwCavingRegion::indexOf(cwCave* cave) {
    return m_caves.indexOf(cave);
}

cwProject *cwCavingRegion::parentProject() const
{
    return dynamic_cast<cwProject*>(parent());
}

void cwCavingRegion::setData(const cwCavingRegionData &data)
{
    setName(data.name);
    setUnitSystem(data.unitSystem);

    // A load must not derive the local projection: it is stored precisely so
    // that opening a project can't move it, and the caves arriving is an event
    // cwLocalProjectionManager reacts to. Quiescing the manager until the
    // stored frame is restored keeps it from building a frame that restore()
    // would overwrite moments later.
    m_localProjectionManager->setLoading(true);

    clearCaves();

    QList<cwCave*> newCaves;
    newCaves.reserve(data.caves.size());
    for(const auto& caveData : data.caves) {
        auto newCave = new cwCave(this);
        newCave->setData(caveData);
        newCaves.append(newCave);
    }
    addCaves(newCaves);

    m_geoReference->restore(data.geoReference.state,
                            data.geoReference.localCoordinateSystem,
                            data.geoReference.anchor,
                            data.geoReference.verticalDatum);
    m_localProjectionManager->setLoading(false);
}

cwCavingRegionData cwCavingRegion::data() const
{
    return {
        .name = m_name.value(),
        .caves = cwData::toDataList<cwCaveData>(m_caves),
        .unitSystem = m_unitSystem,
        .geoReference = {
            .state = m_geoReference->state(),
            .localCoordinateSystem = m_geoReference->localCoordinateSystem(),
            .anchor = m_geoReference->anchor(),
            .verticalDatum = m_geoReference->verticalDatum()
        }
    };
}

/**
  \brief Sets the undo stack for this region

  This will also set undo stack for the children as well
  */
void cwCavingRegion::setUndoStackForChildren() {
    setUndoStackForChildrenHelper(m_caves);
}


/**
  \brief Unparents the cave
  */
void cwCavingRegion::unparentCave(cwCave* cave) {
    //Reparent the trip, if already in another cave
    cwCavingRegion* parentRegion = dynamic_cast<cwCavingRegion*>(((QObject*)cave)->parent());
    if(parentRegion != nullptr) {
        int index = parentRegion->m_caves.indexOf(cave);
        parentRegion->removeCave(index);
    }
}

cwCavingRegion::InsertRemoveCave::InsertRemoveCave(cwCavingRegion* region,
                                                   int beginIndex, int endIndex) {
    Region = region;
    BeginIndex = beginIndex;
    EndIndex = endIndex;
    OwnsCaves = false;
}

/**
  Delete all the caves if it owns them
  */
cwCavingRegion::InsertRemoveCave::~InsertRemoveCave() {
    if(OwnsCaves) {
        foreach(auto cave, Caves) {
            if(cave) {
                cave->deleteLater();
            }
        }
    }
}

/**
  \brief Insert the caves in this command into the region
  */
void cwCavingRegion::InsertRemoveCave::insertCaves() {
   // if(Region.isNull()) { return; }
    cwCavingRegion* regionPtr = Region; //.data();

    emit regionPtr->beginInsertCaves(BeginIndex, EndIndex);
    emit regionPtr->beginInsertRows(QModelIndex(), BeginIndex, EndIndex);
    for(int i = 0; i < Caves.size(); i++) {
        int index = BeginIndex + i;
        regionPtr->m_caves.insert(index, Caves.at(i));
        regionPtr->m_caveNames.insert(Caves.at(i)->name());
        Caves.at(i)->setParent(regionPtr);
    }

    OwnsCaves = false;

    emit regionPtr->insertedCaves(BeginIndex, EndIndex);
    emit regionPtr->endInsertRows();
    emit regionPtr->caveCountChanged();
}

/**
  \brief Removes the caves in this command from the region
  */
void cwCavingRegion::InsertRemoveCave::removeCaves() {
//    if(Region.isNull()) { return; }
    cwCavingRegion* regionPtr = Region; //.data();

    emit regionPtr->beginRemoveCaves(BeginIndex, EndIndex);
    emit regionPtr->beginRemoveRows(QModelIndex(), BeginIndex, EndIndex);

    for(int i = Caves.size() - 1; i >= 0; i--) {
        int index = BeginIndex + i;
        regionPtr->m_caveNames.remove(regionPtr->m_caves.at(index)->name());
        regionPtr->m_caves.removeAt(index);

        //Do NOT uncomment, qml engine may garbage collect objects that aren't parented, and can cause double free problem
        // Caves[i]->setParent(nullptr);
    }

    OwnsCaves = true;

    emit regionPtr->removedCaves(BeginIndex, EndIndex);
    emit regionPtr->endRemoveRows();
    emit regionPtr->caveCountChanged();
}


cwCavingRegion::InsertCaveCommand::InsertCaveCommand(cwCavingRegion* parentRegion,
                                                     QList<cwCave*> caves,
                                                     int index) :
    cwCavingRegion::InsertRemoveCave(parentRegion, index, index + caves.size() -1)
{
    Caves.reserve(caves.size());
    for(auto cave : caves) {
        Caves.append(cave);
    }

    if(caves.size() == 1) {
        setText(QString("Add %1").arg(caves.first()->name()));
    } else {
        setText(QString("Add %1 caves").arg(caves.size()));
    }
}

cwCavingRegion::InsertCaveCommand::InsertCaveCommand(cwCavingRegion* parentRegion,
                                                     cwCave* cave,
                                                     int index) :
    cwCavingRegion::InsertRemoveCave(parentRegion, index, index)
{
    Caves.append(cave);
    setText(QString("Add %1").arg(cave->name()));
}

void cwCavingRegion::InsertCaveCommand::redo() {
    insertCaves();
}

void cwCavingRegion::InsertCaveCommand::undo() {
    removeCaves();
}

cwCavingRegion::RemoveCaveCommand::RemoveCaveCommand(cwCavingRegion* region,
                                                     int beginIndex,
                                                     int endIndex) :
    InsertRemoveCave(region, beginIndex, endIndex)
{
    for(int i = beginIndex; i <= endIndex; i++) {
       Caves.append(region->m_caves.at(i));
    }

    QString commandText;
    if(beginIndex != endIndex) {
        commandText = QString("Remove %1 caves").arg(endIndex - beginIndex);
    } else {
        cwCave* cave = region->m_caves.at(beginIndex);
        commandText = QString("Remove %1").arg(cave->name());
    }
}

void cwCavingRegion::RemoveCaveCommand::redo() {
    removeCaves();
}

void cwCavingRegion::RemoveCaveCommand::undo() {
    insertCaves();
}
