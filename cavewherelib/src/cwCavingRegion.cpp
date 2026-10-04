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
#include "cwEquateModel.h"
#include "cwFixStationValidator.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"
#include "cwLocalProjectionManager.h"
#include "cwLocalProjectionToken.h"
#include "cwProject.h"
#include "cwData.h"
#include "cwTrip.h"

//Qt includes
#include <QThread>
#include <QDebug>
#include <QMetaEnum>

//Std includes
#include <algorithm>

cwCavingRegion::cwCavingRegion(QObject *parent) :
    QAbstractListModel(parent),
    m_geoReference(new cwGeoReference(this)),
    m_lazLayers(new cwLazLayerModel(this)),
    m_equates(new cwEquateModel(this))
{
    //Built in the body rather than the init list: the node asks its parent
    //region for the coordinate system as it constructs, and only here is this
    //region a cwCavingRegion with m_geoReference in place. The validator and
    //the projection manager walk the tree as they construct, so they follow it.
    m_root = new cwSurveyNode(cwSurveyNode::RootNodeTag{}, this);
    m_fixStationValidator = new cwFixStationValidator(this);
    m_localProjectionManager = new cwLocalProjectionManager(this);

    // Every GIS layer loads into the frame, and the frame is derived from what
    // those layers say about themselves — so each one is handed the manager it
    // reads the frame off, and waits on, before it decodes.
    m_lazLayers->setLocalProjectionToken(cwLocalProjectionToken(m_localProjectionManager));

    // geoReference owns the frame; the region tells the things it owns when it
    // moves — the LAZ layers, whose points are in the frame they were decoded
    // into, and every node's grid convergence, which is an angle in the frame
    // and so moves with it. A node can't watch the frame itself: it learns which
    // region it belongs to only when it is inserted, so joining is the other
    // half, handled by the node insert — a node converges to nothing until it
    // has a region to read the frame off. Consumers the region doesn't own
    // connect to geoReference directly.
    const auto recomputeConvergence = [this] {
        const QList<cwSurveyNode*> nodes = m_root->allNodes();
        for (cwSurveyNode* node : nodes) {
            node->recomputeGridConvergence();
        }
    };

    // Re-decoding a directory of point clouds is the most expensive thing the
    // frame can cause, so it hangs off the narrower signal: a freeze or a change
    // of anchor leaves every coordinate where it was.
    connect(m_geoReference, &cwGeoReference::localCoordinateSystemChanged,
            m_lazLayers, &cwLazLayerModel::reloadAll);
    connect(m_geoReference, &cwGeoReference::localProjectionChanged, this, recomputeConvergence);

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

    //This model's rows ARE the root's child nodes, so every row signal is the
    //root's, relayed one hop. Both halves of each pair come from the same funnel
    //on a direct connection, which is what keeps begin/end paired.
    connect(m_root, &cwSurveyNode::beginInsertNodes, this, [this](int begin, int end) {
        emit beginInsertCaves(begin, end);
        beginInsertRows(QModelIndex(), begin, end);
    });
    connect(m_root, &cwSurveyNode::insertedNodes, this, [this](int begin, int end) {
        emit insertedCaves(begin, end);
        endInsertRows();
    });
    connect(m_root, &cwSurveyNode::beginRemoveNodes, this, [this](int begin, int end) {
        emit beginRemoveCaves(begin, end);
        beginRemoveRows(QModelIndex(), begin, end);
    });
    connect(m_root, &cwSurveyNode::removedNodes, this, [this](int begin, int end) {
        emit removedCaves(begin, end);
        endRemoveRows();
    });

    connect(m_root, &cwSurveyNode::childNodeCountChanged,
            this, &cwCavingRegion::caveCountChanged);
    //Already the aggregate of every label move at or below the root, so the
    //region subscribes once and to the root alone.
    connect(m_root, &cwSurveyNode::scopeLabelsChanged,
            this, &cwCavingRegion::scopeLabelsChanged);
    connect(m_root, &cwSurveyNode::tripsDeleted,
            this, &cwCavingRegion::ownersDeleted);
    connect(m_root, &cwSurveyNode::nodesDeleted,
            this, &cwCavingRegion::ownersDeleted);

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
        return QVariant::fromValue(cave(index.row()));
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


int cwCavingRegion::caveCount() const {
    return m_root->childNodeCount();
}

bool cwCavingRegion::hasCaves() const {
    return caveCount() > 0;
}

/**
  \brief Get's a cave at index
  */
cwCave* cwCavingRegion::cave(int index) const {
    return qobject_cast<cwCave*>(m_root->childNode(index));
}

/**
  \brief Gets all the caves in the region

  Every node the loader, the importers and addCave() construct is a cwCave, so
  the cast answers for every child the root holds.
  */
QList<cwCave*> cwCavingRegion::caves() const {
    const QList<cwSurveyNode*> nodes = m_root->childNodes();
    QList<cwCave*> caves;
    caves.reserve(nodes.size());
    for(cwSurveyNode* node : nodes) {
        auto cave = qobject_cast<cwCave*>(node);
        //The row indices of caves() and cave(i) have to line up, so a child that
        //is not a cwCave is kept as a null row and caught here instead.
        Q_ASSERT(cave != nullptr);
        caves.append(cave);
    }
    return caves;
}

cwSurveyNode* cwCavingRegion::addNode(cwSurveyNode* parent,
                                      cwSurveyNode::Kind kind,
                                      const QString& proposedName)
{
    cwSurveyNode* parentNode = parent == nullptr ? m_root : parent;
    if(parentNode->parentRegion() != this) {
        //A node from another tree is not this region's to add to.
        return nullptr;
    }

    const char* const kindKey =
        QMetaEnum::fromType<cwSurveyNode::Kind>().valueToKey(static_cast<int>(kind));
    if(kindKey == nullptr) {
        //QML hands enums over as plain ints, so kind may name no Kind at all.
        return nullptr;
    }

    const QString baseName = proposedName.isEmpty()
                                 ? QStringLiteral("New %1").arg(QString::fromUtf8(kindKey))
                                 : proposedName;
    const QString newNodeName = parentNode->uniqueChildName(baseName);

    beginUndoMacro(QStringLiteral("Add %1").arg(newNodeName));

    auto node = new cwCave();
    node->setKind(kind);
    node->setUndoStack(undoStack());
    node->setName(newNodeName);
    parentNode->addNode(node);

    endUndoMacro();

    return node;
}

void cwCavingRegion::moveNode(cwSurveyNode* node, cwSurveyNode* newParent, int row)
{
    if(node == nullptr || node->isRoot()) { return; }

    cwSurveyNode* destination = newParent == nullptr ? m_root : newParent;
    if(node->parentRegion() != this || destination->parentRegion() != this) {
        //A node from another tree is not this region's to move.
        return;
    }

    if(!node->isListedByParent()) {
        //Nothing holds the node, so there is nothing to move it from. An insert
        //is the verb for that.
        return;
    }

    //insertNode routes an already-listed node through MoveNodeCommand, and
    //refuses a destination inside the node's own subtree.
    destination->insertNode(row, node);
}

/**
  \brief Adds a cave to the region
  */
void cwCavingRegion::addCave(cwCave* cave) {
    if(cave == nullptr) {
        addNode(nullptr, cwSurveyNode::Kind::Cave);
        return;
    }
    m_root->addNode(cave);
}

void cwCavingRegion::addCaves(QList<cwCave*> caves) {
    m_root->addNodes(QList<cwSurveyNode*>(caves.begin(), caves.end()));
}

/**
  \brief Inserts a cave into the region at index
  */
void cwCavingRegion::insertCave(int index, cwCave* cave) {
    m_root->insertNode(index, cave);
}

/**
  \brief Removes the cave at index, and says so through ownersDeleted()
  */
void cwCavingRegion::removeCave(int index) {
    //The root reads the whole subtree's owner ids before the removal and emits
    //them as nodesDeleted, which this region relays as ownersDeleted.
    m_root->removeNode(index);
}

/**
  \brief Removes all the caves from the region
  */
void cwCavingRegion::clearCaves() {
    m_root->clearNodes();
}

/**
  \brief Get's the index of the cave
  */
int cwCavingRegion::indexOf(cwCave* cave) {
    return m_root->indexOfNode(cave);
}

QString cwCavingRegion::uniqueCaveName(const QString& proposedName) const
{
    return m_root->uniqueChildName(proposedName);
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

    m_equates->setEquates(data.equates);

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

cwCave* cwCavingRegion::caveFor(const cwStationHandle& handle) const
{
    if (!handle.isValid()) {
        return nullptr;
    }

    const QList<cwSurveyNode*> nodes = m_root->allNodes();
    for (cwSurveyNode* node : nodes) {
        switch (handle.scope()) {
        case cwStationHandle::NativeCave:
            if (node->id() == handle.containerId()) {
                return qobject_cast<cwCave*>(node);
            }
            break;
        case cwStationHandle::Trip:
            for (const cwTrip* trip : node->trips()) {
                if (trip != nullptr && trip->id() == handle.containerId()) {
                    return qobject_cast<cwCave*>(node);
                }
            }
            break;
        default:
            //An out-of-enum scope (a cast int pushed through qml) names no
            //container any node can resolve.
            return nullptr;
        }
    }

    return nullptr;
}

bool cwCavingRegion::tieStations(const cwStationHandle& first,
                                 const cwStationHandle& second)
{
    const cwEquate equate(QList<cwStationHandle>({first, second}));
    if (!equate.isValid()) {
        return false;
    }

    cwCave* firstCave = caveFor(first);
    cwCave* secondCave = caveFor(second);
    if (firstCave == nullptr || secondCave == nullptr) {
        return false;
    }

    const bool withinOneNode = (firstCave == secondCave);
    if (withinOneNode && !firstCave->validate(equate)) {
        return false;
    }

    //A tie is a fact about two stations, so declaring it twice says nothing
    //more.
    if (isTied(first, second)) {
        return true;
    }

    m_equates->appendEquate(equate);
    return true;
}

bool cwCavingRegion::isTied(const cwStationHandle& first,
                            const cwStationHandle& second) const
{
    //Checked by membership rather than by equality: an equate that already
    //ties these two along with a third still ties them.
    const QList<cwEquate>& declared = m_equates->equates();
    return std::any_of(declared.cbegin(), declared.cend(),
                       [&first, &second](const cwEquate& existing) {
        const QList<cwStationHandle> stations = existing.stations();
        return stations.contains(first) && stations.contains(second);
    });
}

cwCavingRegionData cwCavingRegion::data() const
{
    return {
        .name = m_name.value(),
        .caves = cwData::toDataList<cwCaveData>(caves()),
        .unitSystem = m_unitSystem,
        .geoReference = {
            .state = m_geoReference->state(),
            .localCoordinateSystem = m_geoReference->localCoordinateSystem(),
            .anchor = m_geoReference->anchor(),
            .verticalDatum = m_geoReference->verticalDatum()
        },
        .equates = m_equates->equates()
    };
}

/**
  \brief Sets the undo stack for this region

  This will also set undo stack for the children as well
  */
void cwCavingRegion::setUndoStackForChildren() {
    //Called from the cwUndoer base constructor before m_root exists; there the
    //stack is already null, so setUndoStack() short-circuits and never gets here.
    m_root->setUndoStack(undoStack());
}


const QHash<QUuid, QString>& cwCavingRegion::caveScopeLabels() const
{
    return m_root->childScopeLabels();
}
