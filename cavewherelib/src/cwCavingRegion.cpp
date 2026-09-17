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
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwLazLayerModel.h"
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
    // geoReference owns the CS + worldOrigin; the region only mirrors each change
    // into the LAZ layer model (it owns lazLayers). Consumers that react to CS /
    // worldOrigin connect to geoReference directly. The worldOrigin push runs
    // before the CS push for a CS-driven reset, matching the prior in-setter
    // ordering.
    connect(m_geoReference, &cwGeoReference::worldOriginChanged, this, [this] {
        m_lazLayers->setRegionWorldOrigin(m_geoReference->worldOrigin());
    });
    connect(m_geoReference, &cwGeoReference::globalCoordinateSystemChanged, this, [this] {
        m_lazLayers->setRegionGlobalCS(m_geoReference->globalCoordinateSystem());
    });

    //Built in the body rather than the init list: the node asks its parent
    //region for the coordinate system as it constructs, and only here is this
    //region a cwCavingRegion with m_geoReference in place.
    m_root = new cwSurveyNode(cwSurveyNode::RootNodeTag{}, this);

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

    //A fix station with no input CS of its own falls back to this region's, so a
    //CS change moves the convergence readout of every node in the tree. A node
    //this region no longer lists is simply absent from allNodes().
    connect(m_geoReference, &cwGeoReference::globalCoordinateSystemChanged, this, [this] {
        const QList<cwSurveyNode*> nodes = m_root->allNodes();
        for(cwSurveyNode* node : nodes) {
            node->recomputeGridConvergence();
        }
    });
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

cwSurveyNode* cwCavingRegion::addNode(cwSurveyNode* parent, cwSurveyNode::Kind kind)
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

    const QString kindName = QString::fromUtf8(kindKey);
    const QString newNodeName = QStringLiteral("%1 %2")
                                    .arg(kindName)
                                    .arg(parentNode->childNodeCount() + 1);

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

void cwCavingRegion::recomputeWorldOrigin()
{
    const QString globalCSTrimmed = m_geoReference->globalCoordinateSystem().trimmed();

    QList<cwGeoPoint> candidates;
    const QList<cwSurveyNode*> nodes = m_root->allNodes();
    for (cwSurveyNode* node : nodes) {
        if (node->fixStations() == nullptr) {
            continue;
        }
        for (const cwFixStation& fix : node->fixStations()->fixStations()) {
            QString inputCS = fix.inputCS().trimmed();
            if (inputCS.isEmpty()) {
                inputCS = globalCSTrimmed;
            }
            if (inputCS.isEmpty() || !cwCoordinateTransform::isValidCS(inputCS)) {
                continue;
            }

            const cwGeoPoint p(fix.easting(), fix.northing(), fix.elevation());

            if (globalCSTrimmed.isEmpty()
                || inputCS.compare(globalCSTrimmed, Qt::CaseInsensitive) == 0) {
                candidates.append(p);
            } else {
                cwCoordinateTransform t(inputCS, globalCSTrimmed);
                if (!t.isValid()) {
                    continue;
                }
                candidates.append(t.transform(p));
            }
        }
    }

    if (candidates.isEmpty()) {
        return;
    }

    cwGeoPoint sum;
    for (const auto& p : candidates) {
        sum.x += p.x;
        sum.y += p.y;
        sum.z += p.z;
    }
    const double n = double(candidates.size());
    m_geoReference->setWorldOrigin(cwGeoPoint{sum.x / n, sum.y / n, sum.z / n});
}

void cwCavingRegion::setData(const cwCavingRegionData &data)
{
    setName(data.name);
    setUnitSystem(data.unitSystem);
    m_geoReference->setGlobalCoordinateSystem(data.globalCoordinateSystem);
    // worldOrigin is intentionally not persisted (see cavewhere.proto:
    // "reserved 5; // Removed: worldOrigin ... recomputed on load"). On
    // disk-load, data.worldOrigin is always default-constructed cwGeoPoint{},
    // and setGlobalCoordinateSystem above already reset our state to match.
    // Only call setWorldOrigin when the data carries a non-default value —
    // otherwise we'd flip the explicit-set flag for a value the user never
    // actually chose, and the next LAZ add would skip its bbox-center
    // auto-adopt. (In-process data → setData round-trips still work because
    // a non-default value will be present.)
    if (data.worldOrigin != cwGeoPoint{}) {
        m_geoReference->setWorldOrigin(data.worldOrigin);
    }

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

    const bool withinOneCave = (firstCave == secondCave);
    if (withinOneCave && !firstCave->validate(equate)) {
        return false;
    }

    cwEquateModel* home = withinOneCave ? firstCave->equates() : m_equates;

    //A tie is a fact about two stations, so declaring it twice says nothing
    //more. Checked by membership rather than by equality: an equate that
    //already ties these two along with a third still says what the caller
    //asked for.
    const QList<cwEquate>& declared = home->equates();
    const bool alreadyTied = std::any_of(declared.cbegin(), declared.cend(),
                                         [&first, &second](const cwEquate& existing) {
        const QList<cwStationHandle> stations = existing.stations();
        return stations.contains(first) && stations.contains(second);
    });
    if (alreadyTied) {
        return true;
    }

    home->appendEquate(equate);
    return true;
}

cwCavingRegionData cwCavingRegion::data() const
{
    return {
        m_name.value(),
        cwData::toDataList<cwCaveData>(caves()),
        m_geoReference->globalCoordinateSystem(),
        m_geoReference->worldOrigin(),
        m_unitSystem,
        m_equates->equates()
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
