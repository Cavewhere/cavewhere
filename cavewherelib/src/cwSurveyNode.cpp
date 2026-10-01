/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurveyNode.h"
#include "cwTrip.h"
#include "cwStation.h"
#include "cwLength.h"
#include "cwErrorModel.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwData.h"
#include "cwFixStationModel.h"
#include "cwGridConvergence.h"
#include "cwNameUtils.h"
#include "cwKeywordModel.h"
#include "cwTripCalibration.h"

//Qt includes
#include <QSet>
#include <QThread>
#include <QUuid>

//Std includes
#include <algorithm>

namespace {
    //! Joins a node's path names into the one hierarchy string the keyword
    //! search groups by. A node directly under the region has only its own name
    //! in its path, so the separator shows up once a project has folders.
    constexpr auto kNodePathSeparator = QLatin1StringView(" / ");
}

cwSurveyNode::cwSurveyNode(QObject* parent) :
    cwSurveyNode(false, parent)
{
}

cwSurveyNode::cwSurveyNode(RootNodeTag, QObject* parent) :
    cwSurveyNode(true, parent)
{
}

cwSurveyNode::cwSurveyNode(bool isRoot, QObject* parent) :
    QAbstractListModel(parent),
    m_length(new cwLength(this)),
    m_depth(new cwLength(this)),
    m_errorModel(new cwErrorModel(this)),
    m_fixStations(new cwFixStationModel(this)),
    m_stationPositionLookupStale(false),
    m_id(QUuid::createUuid()),
    m_gridConvergence(new cwGridConvergence(this)),
    m_keywordModel(new cwKeywordModel(this)),
    m_isRoot(isRoot)
{
    m_length->setUnit(cwUnits::Meters);
    m_depth->setUnit(cwUnits::Meters);

    m_length->setUpdateValue(true);
    m_depth->setUpdateValue(true);

    connect(m_fixStations, &cwFixStationModel::countChanged,
            this, &cwSurveyNode::recomputeGridConvergence);
    connect(m_fixStations, &QAbstractItemModel::modelReset,
            this, &cwSurveyNode::recomputeGridConvergence);
    // Filter dataChanged on the roles that actually influence the
    // formatted convergence — variance/id edits would otherwise walk
    // PROJ only to be discarded by the change-detection guard.
    connect(m_fixStations, &QAbstractItemModel::dataChanged, this,
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

    wireScopeLabelAggregate();

    recomputeGridConvergence();
}

cwSurveyNode::~cwSurveyNode() {
    Q_ASSERT(thread() == QThread::currentThread() || thread() == nullptr);
}

void cwSurveyNode::wireScopeLabelAggregate()
{
    //Wired here rather than emitted beside each sibling-set pulse, so no emit
    //site can be added later that forgets the aggregate.
    connect(this, &cwSurveyNode::tripScopeLabelsChanged,
            this, &cwSurveyNode::scopeLabelsChanged);
    connect(this, &cwSurveyNode::childScopeLabelsChanged,
            this, &cwSurveyNode::scopeLabelsChanged);
}

/**
  \brief Sets the name of the node
  */
void cwSurveyNode::setName(QString name) {
    if(m_name == name) {
        return;
    }
    if (!validateName(name).isEmpty()) {
        return;
    }
    pushUndo(new NameCommand(this, name));
}

QString cwSurveyNode::validateName(const QString& proposedName) const
{
    //Names are unique per sibling set only, so the set to check is the one
    //holding this node: its parent's children. A node nothing lists — never
    //inserted, or removed and kept for undo — has no sibling set, so its name is
    //sanitized alone.
    const cwSanitizedNameSet* nameSet = nullptr;
    if(const cwSurveyNode* parent = parentNode()) {
        if(parent->m_childNodes.contains(const_cast<cwSurveyNode*>(this))) {
            nameSet = &parent->childNameSet();
        }
    }

    //"cave" is what the user sees every node called today.
    return cwNameUtils::validateEntityName(m_name, proposedName, nameSet,
                                           QStringLiteral("cave"));
}

/**
  Returns proposedName sanitized and deduplicated against the existing trip
  names. cwTrip::setName silently rejects collisions and unsanitized names,
  so callers renaming a trip from an external string (e.g. an entry
  filename) should route through this first.
  */
QString cwSurveyNode::uniqueTripName(const QString& proposedName) const
{
    return m_tripNames.deduplicateName(cwNameUtils::sanitizeFileName(proposedName));
}

/**
  The same, for a child node's name among its siblings.
  */
QString cwSurveyNode::uniqueChildName(const QString& proposedName) const
{
    return m_childNames.deduplicateName(cwNameUtils::sanitizeFileName(proposedName));
}

void cwSurveyNode::updateKeywords()
{
    if(!m_keywordModel) {
        return;
    }

    //The hierarchy the keyword search shows is where the node sits, not just
    //what it is called: two "Entrance" caves in different folders are
    //"North / Entrance" and "South / Entrance".
    const QStringList names = path();

    //An unnamed node has no place to show, the way an unnamed cave had no name
    //to show, and an unnamed ancestor would leave a dangling separator behind.
    const bool everyNameFilled = !names.isEmpty()
            && std::none_of(names.begin(), names.end(),
                            [](const QString& name) { return name.isEmpty(); });

    if(everyNameFilled) {
        m_keywordModel->replace({cwKeywordModel::CaveKey, names.join(kNodePathSeparator)});
    } else {
        m_keywordModel->removeAll(cwKeywordModel::CaveKey);
    }
}

/**
  \brief Refreshes the hierarchy keyword of this node and everything under it

  A rename or a move changes the path of every descendant, and only this node
  can see that happen.
  */
void cwSurveyNode::updateSubtreeKeywords()
{
    updateKeywords();
    for(cwSurveyNode* child : std::as_const(m_childNodes)) {
        child->updateSubtreeKeywords();
    }
}

void cwSurveyNode::setId(const QUuid& id)
{
    const QUuid oldId = m_id;
    if (!id.isNull()) {
        m_id = id;
    } else if (m_id.isNull()) {
        m_id = QUuid::createUuid();
    }

    if (m_id != oldId) {
        emit idChanged();
    }
}

void cwSurveyNode::setKind(Kind kind)
{
    if (m_kind == kind) {
        return;
    }
    m_kind = kind;
    emit kindChanged();
}

void cwSurveyNode::setReadOnly(bool readOnly)
{
    if (m_readOnly == readOnly) {
        return;
    }
    m_readOnly = readOnly;
    emit sourceChanged();
}

void cwSurveyNode::setSourceId(const QUuid& sourceId)
{
    if (m_sourceId == sourceId) {
        return;
    }
    m_sourceId = sourceId;
    emit sourceChanged();
}

void cwSurveyNode::setSourcePath(const QString& sourcePath)
{
    if (m_sourcePath == sourcePath) {
        return;
    }
    m_sourcePath = sourcePath;
    emit sourceChanged();
}

void cwSurveyNode::setExternalCenterline(const cwExternalCenterline& value)
{
    if (m_externalCenterline == value) {
        return;
    }

    const bool wasExternallyBacked = externallyBacked();
    m_externalCenterline = value;
    emit externalCenterlineChanged();

    if (externallyBacked() != wasExternallyBacked) {
        emit externallyBackedChanged();
    }
}

bool cwSurveyNode::externallyBacked() const
{
    if (!m_externalCenterline.isEmpty()) {
        return true;
    }
    const cwSurveyNode* parent = parentNode();
    return parent != nullptr && parent->externallyBacked();
}

/**
  Adds a new trip to the node

  The trip will be added to the end of the all the trip
  */
void cwSurveyNode::addTripNullHelper() {
    cwTrip* trip = new cwTrip(undoStack());

    // Seed the new trip's survey-entry unit from the project default (unitSystem()
    // resolves the region, or Metric when the node has no region yet). Only new
    // trips are seeded; loaded trips keep their own stored unit.
    trip->calibrations()->setDistanceUnit(cwUnits::surveyUnit(unitSystem()));

    QString tripName = QString("Trip %1").arg(tripCount() + 1);
    beginUndoMacro(QString("Add %1").arg(tripName));

    trip->setName(tripName);
    addTrip(trip);

    endUndoMacro();
}

/**
  \brief Adds a trip to the node

  Once the trip is added to the node, the node owns the trip
  */
void cwSurveyNode::addTrip(cwTrip* trip) {
    if(trip == nullptr) {
        addTripNullHelper();
        return;
    }
    insertTrip(m_trips.size(), trip);
}

/**
  \brief Insert a survey trip into the node

  i - The index where the trip will be inserted
  */
void cwSurveyNode::insertTrip(int i, cwTrip* trip) {
    if(i < 0 || i > m_trips.size()) { return; }

    //Reparent the trip, if already in another node
    if(cwSurveyNode* oldParent = trip->parentNode()) {
        const int index = oldParent->m_trips.indexOf(trip);
        if(index >= 0) {
            //The trip is moving, not being deleted — its id lives on in this
            //node, so the old node must stay quiet about it.
            oldParent->removeTripInternal(index);
        }
    }

    // Auto-rename to avoid filesystem path collisions in .cwproj layout.
    // Trip has no parent node yet, so setName()'s guard won't fire.
    const QString deduped = m_tripNames.deduplicateName(trip->name());
    if (deduped != trip->name()) {
        trip->setName(deduped);
    }

    pushUndo(new InsertTripCommand(this, trip, i));
}

/**
  \brief Removes a trip from the node

  i - The index where the trip will be remove.  The trip will be the responiblity of
  the caller to delete it
  */
void cwSurveyNode::removeTrip(int i) {
    if(i < 0 || i >= m_trips.size()) { return; }

    //Read the id before the removal, which can destroy the trip outright: with
    //no undo stack pushUndo executes the command and then deletes it, taking
    //the trip with it.
    const QUuid removedId = m_trips.at(i)->id();
    removeTripInternal(i);
    emit tripsDeleted({removedId});
}

void cwSurveyNode::removeTripInternal(int i) {
    if(i < 0 || i >= m_trips.size()) { return; }
    pushUndo(new RemoveTripCommand(this, i, i));
}

/**
  \brief Removes all the trips from the node
  */
void cwSurveyNode::clearTrips() {
    if(!m_trips.isEmpty()) {
        pushUndo(new RemoveTripCommand(this, 0, m_trips.size() - 1));
    }
}

/**
  \brief Appends a child node, sibling-name deduplicated and undoable
  */
void cwSurveyNode::addNode(cwSurveyNode* node)
{
    if(node == nullptr) { return; }
    insertNode(m_childNodes.size(), node);
}

/**
  \brief Appends a batch of child nodes as ONE undo command

  The batch is deduplicated against the existing children and against itself, so
  a load or an import that brings in N nodes at once costs one undo entry and one
  row-signal pair rather than N of each.
  */
void cwSurveyNode::addNodes(const QList<cwSurveyNode*>& nodes)
{
    //A node another parent lists is a move, and a move is its own command, so a
    //batch holding one is appended node by node inside a macro rather than as a
    //single insert. Every production caller (the loader, an import) hands over
    //fresh nodes and takes the batch path below.
    const bool holdsAMove = std::any_of(nodes.begin(), nodes.end(),
                                        [](const cwSurveyNode* node) {
        return node != nullptr && node->isListedByParent();
    });
    if(holdsAMove) {
        beginUndoMacro(QStringLiteral("Add %1 nodes").arg(nodes.size()));
        for(cwSurveyNode* node : nodes) {
            insertNode(m_childNodes.size(), node);
        }
        endUndoMacro();
        return;
    }

    QList<cwSurveyNode*> inserting;
    inserting.reserve(nodes.size());

    //One running copy dedupes the whole batch, including against the names the
    //insert command has yet to write into m_childNames.
    cwSanitizedNameSet siblingNames = m_childNames;

    for(cwSurveyNode* node : nodes) {
        if(prepareChildForInsert(node, siblingNames)) {
            inserting.append(node);
        }
    }

    if(!inserting.isEmpty()) {
        pushUndo(new InsertNodeCommand(this, inserting, m_childNodes.size()));
    }
}

/**
  \brief Readies \a node to become a child of this node

  Refuses a null node and one that would close a parent cycle, moves the node
  off its old parent, and renames it to a name free in \a siblingNames, which
  the deduplicated name is then added to. Returns true when the node is ready to
  be handed to an InsertNodeCommand.
  */
bool cwSurveyNode::prepareChildForInsert(cwSurveyNode* node, cwSanitizedNameSet& siblingNames)
{
    if(node == nullptr) { return false; }

    //A node may not take on itself or one of its own ancestors: that closes a
    //parent cycle, and every upward walk (path(), parentRegion(),
    //externallyBacked()) would run forever. addNode() is callable from QML.
    if(ancestorsOrSelf().contains(node)) { return false; }

    // Auto-rename to avoid filesystem path collisions in .cwproj layout. The
    // node is not in this list yet, so setName()'s guard won't fire.
    const QString deduped = siblingNames.deduplicateName(node->name());
    if(deduped != node->name()) {
        node->setName(deduped);
    }
    siblingNames.insert(node->name());

    return true;
}

/**
  \brief Inserts a child node at row

  A node already held by another node moves here, keeping the same QObject.
  */
void cwSurveyNode::insertNode(int row, cwSurveyNode* node)
{
    if(node == nullptr) { return; }

    if(node->isListedByParent()) {
        moveNodeHere(row, node);
        return;
    }

    if(row < 0 || row > m_childNodes.size()) { return; }

    //A throwaway copy: the insert command is what writes m_childNames.
    cwSanitizedNameSet siblingNames = m_childNames;
    if(!prepareChildForInsert(node, siblingNames)) { return; }

    pushUndo(new InsertNodeCommand(this, node, row));
}

void cwSurveyNode::moveNodeHere(int row, cwSurveyNode* node)
{
    //A node may not take on itself or one of its own ancestors, for the reason
    //prepareChildForInsert() gives.
    if(ancestorsOrSelf().contains(node)) { return; }

    cwSurveyNode* oldParent = node->parentNode();
    const int oldRow = oldParent->indexOfNode(node);

    //row counts the sibling list the node has already left, so a move within one
    //parent has one row fewer to land on.
    const int lastRow = m_childNodes.size() - (oldParent == this ? 1 : 0);
    const int destination = qBound(0, row, lastRow);

    if(oldParent == this && destination == oldRow) { return; }

    pushUndo(new MoveNodeCommand(node, this, destination));
}

/**
  \brief Removes the child node at row and says the whole subtree is gone
  */
void cwSurveyNode::removeNode(int row)
{
    if(row < 0 || row >= m_childNodes.size()) { return; }

    //Read the ids before the removal, which can destroy the subtree outright:
    //with no undo stack pushUndo executes the command and then deletes it,
    //taking the node and everything under it along.
    QList<QUuid> removedIds;
    m_childNodes.at(row)->walk([&removedIds](const cwSurveyNode* node) {
        removedIds.append(node->id());
        const QList<cwTrip*> nodeTrips = node->trips();
        for(const cwTrip* trip : nodeTrips) {
            removedIds.append(trip->id());
        }
    });

    removeNodeInternal(row);
    emit nodesDeleted(removedIds);
}

void cwSurveyNode::removeNodeInternal(int row)
{
    if(row < 0 || row >= m_childNodes.size()) { return; }
    pushUndo(new RemoveNodeCommand(this, row, row));
}

/**
  \brief Removes every child node, in one undo command

  Silent on nodesDeleted(), as clearTrips() is on tripsDeleted(): this is the
  project-close and load-replace path, where the ids live on.
  */
void cwSurveyNode::clearNodes()
{
    if(!m_childNodes.isEmpty()) {
        pushUndo(new RemoveNodeCommand(this, 0, m_childNodes.size() - 1));
    }
}

const QHash<QUuid, QString>& cwSurveyNode::tripScopeLabels() const
{
    return m_tripScopeLabels.labels(m_trips);
}

const QHash<QUuid, QString>& cwSurveyNode::childScopeLabels() const
{
    //Child blocks open in the same scope as the trip blocks, so the trips claim
    //their labels first and the children take labels apart from them.
    return m_childScopeLabels.labels(m_childNodes, m_trips);
}

void cwSurveyNode::invalidateTripScopeLabels()
{
    //The two halves are separable, and insertTrips/removeTrips separate them:
    //the cache goes stale the instant the trip list changes, while the pulse
    //waits for the model to settle. A rename needs no such split — the new name
    //is in place before nameChanged reaches here.
    m_tripScopeLabels.invalidate();
    emit tripScopeLabelsChanged();
    invalidateChildScopeLabelsAfterTripChange();
}

void cwSurveyNode::invalidateChildScopeLabelsAfterTripChange()
{
    //A trip label can push a child off the label it held. Pulsed only when there
    //are children to move, so a flat node's trip edits stay as quiet as before.
    m_childScopeLabels.invalidate();
    if(!m_childNodes.isEmpty()) {
        emit childScopeLabelsChanged();
    }
}

void cwSurveyNode::invalidateChildScopeLabels()
{
    m_childScopeLabels.invalidate();
    emit childScopeLabelsChanged();
}

void cwSurveyNode::connectTrip(cwTrip* trip)
{
    //A trip's label is unique among its node's trips, so this node is the only
    //object that can see a rename move one — and the only one that can tell the
    //*siblings* their labels moved with it. UniqueConnection because undo/redo
    //re-inserts the same trip.
    connect(trip, &cwTrip::nameChanged,
            this, &cwSurveyNode::invalidateTripScopeLabels, Qt::UniqueConnection);
    connect(this, &cwSurveyNode::tripScopeLabelsChanged,
            trip, &cwTrip::scopeChanged, Qt::UniqueConnection);

    //The trip owns solvedStations() but this node owns the lookup it reads, so
    //this node is the only object that can say when the answer moved.
    connect(this, &cwSurveyNode::stationPositionPositionChanged,
            trip, &cwTrip::solvedStationsChanged, Qt::UniqueConnection);

    //A harvest is one trip's news, but knownStations() is read across the node,
    //so this node is the only object that can say some trip in it learned names.
    connect(trip, &cwTrip::externalStationsChanged,
            this, &cwSurveyNode::tripExternalStationsChanged, Qt::UniqueConnection);

    //externallyBacked() reads this node's attachment and its ancestors', so an
    //attach or detach anywhere above moves every trip's answer and only this
    //node can say so.
    connect(this, &cwSurveyNode::externallyBackedChanged,
            trip, &cwTrip::externallyBackedChanged, Qt::UniqueConnection);
}

void cwSurveyNode::disconnectTrip(cwTrip* trip)
{
    //A trip this node no longer lists must not dirty its labels, and must not be
    //told they moved. Its own rename still reaches scopeChanged through
    //NameCommand, though scopePrefix() answers empty for it either way — the
    //node is what grants a place in the namespace, and it no longer does.
    disconnect(trip, &cwTrip::nameChanged,
               this, &cwSurveyNode::invalidateTripScopeLabels);
    disconnect(this, &cwSurveyNode::tripScopeLabelsChanged,
               trip, &cwTrip::scopeChanged);
    disconnect(this, &cwSurveyNode::stationPositionPositionChanged,
               trip, &cwTrip::solvedStationsChanged);
    disconnect(trip, &cwTrip::externalStationsChanged,
               this, &cwSurveyNode::tripExternalStationsChanged);
    disconnect(this, &cwSurveyNode::externallyBackedChanged,
               trip, &cwTrip::externallyBackedChanged);
}

void cwSurveyNode::connectNode(cwSurveyNode* node)
{
    //Each aggregate travels exactly one hop upward, so a depth-N tree pulses
    //once per level. UniqueConnection because undo/redo re-inserts the same node.
    connect(node, &cwSurveyNode::subtreeChanged,
            this, &cwSurveyNode::subtreeChanged, Qt::UniqueConnection);
    connect(node, &cwSurveyNode::scopeLabelsChanged,
            this, &cwSurveyNode::scopeLabelsChanged, Qt::UniqueConnection);
    connect(node, &cwSurveyNode::tripsDeleted,
            this, &cwSurveyNode::tripsDeleted, Qt::UniqueConnection);
    connect(node, &cwSurveyNode::nodesDeleted,
            this, &cwSurveyNode::nodesDeleted, Qt::UniqueConnection);

    //A child's label is unique among this node's children, so this node is the
    //only object that can see a rename move one.
    connect(node, &cwSurveyNode::nameChanged,
            this, &cwSurveyNode::invalidateChildScopeLabels, Qt::UniqueConnection);

    //externallyBacked() reads every ancestor's attachment, so this is the one
    //relay that travels downward — and it is never relayed up, so the two
    //directions cannot loop.
    connect(this, &cwSurveyNode::externallyBackedChanged,
            node, &cwSurveyNode::externallyBackedChanged, Qt::UniqueConnection);
}

void cwSurveyNode::disconnectNode(cwSurveyNode* node)
{
    disconnect(node, &cwSurveyNode::subtreeChanged,
               this, &cwSurveyNode::subtreeChanged);
    disconnect(node, &cwSurveyNode::scopeLabelsChanged,
               this, &cwSurveyNode::scopeLabelsChanged);
    disconnect(node, &cwSurveyNode::tripsDeleted,
               this, &cwSurveyNode::tripsDeleted);
    disconnect(node, &cwSurveyNode::nodesDeleted,
               this, &cwSurveyNode::nodesDeleted);
    disconnect(node, &cwSurveyNode::nameChanged,
               this, &cwSurveyNode::invalidateChildScopeLabels);
    disconnect(this, &cwSurveyNode::externallyBackedChanged,
               node, &cwSurveyNode::externallyBackedChanged);
}

void cwSurveyNode::setParentNode(cwSurveyNode* node)
{
    if(m_parentNode == node) {
        return;
    }
    m_parentNode = node;
    setParent(node);
    emit parentNodeChanged();
}

QList<cwTrip*> cwSurveyNode::allTrips() const
{
    QList<cwTrip*> trips;
    for(const cwSurveyNode* child : m_childNodes) {
        trips.append(child->allTrips());
    }
    trips.append(m_trips);
    return trips;
}

QList<cwSurveyNode*> cwSurveyNode::allNodes() const
{
    QList<cwSurveyNode*> nodes;
    for(cwSurveyNode* child : m_childNodes) {
        nodes.append(child);
        nodes.append(child->allNodes());
    }
    return nodes;
}

QList<const cwSurveyNode*> cwSurveyNode::ancestorsOrSelf() const
{
    QList<const cwSurveyNode*> chain;
    for(const cwSurveyNode* node = this; node != nullptr; node = node->parentNode()) {
        chain.append(node);
    }
    return chain;
}

QList<const cwSurveyNode*> cwSurveyNode::pathNodes() const
{
    QList<const cwSurveyNode*> nodes;
    const QList<const cwSurveyNode*> chain = ancestorsOrSelf();
    for(const cwSurveyNode* node : chain) {
        if(node->isRoot()) {
            break;
        }
        nodes.append(node);
    }
    std::reverse(nodes.begin(), nodes.end());
    return nodes;
}

QStringList cwSurveyNode::path() const
{
    QStringList names;
    const QList<const cwSurveyNode*> nodes = pathNodes();
    for(const cwSurveyNode* node : nodes) {
        names.append(node->name());
    }
    return names;
}

QList<QUuid> cwSurveyNode::pathIds() const
{
    QList<QUuid> ids;
    const QList<const cwSurveyNode*> nodes = pathNodes();
    for(const cwSurveyNode* node : nodes) {
        ids.append(node->id());
    }
    return ids;
}

cwSurveyNode* cwSurveyNode::sourceRoot() const
{
    const QList<const cwSurveyNode*> chain = ancestorsOrSelf();
    for(const cwSurveyNode* node : chain) {
        if(node->isSourceRoot()) {
            return const_cast<cwSurveyNode*>(node);
        }
    }
    return nullptr;
}

cwSurveyNode* cwSurveyNode::lowestCommonAncestor(const cwSurveyNode* other) const
{
    if(other == nullptr) {
        return nullptr;
    }

    const QList<const cwSurveyNode*> chain = ancestorsOrSelf();
    const QSet<const cwSurveyNode*> ancestors(chain.begin(), chain.end());

    const QList<const cwSurveyNode*> otherChain = other->ancestorsOrSelf();
    for(const cwSurveyNode* node : otherChain) {
        if(ancestors.contains(node)) {
            return const_cast<cwSurveyNode*>(node);
        }
    }
    return nullptr;
}

cwCavingRegion *cwSurveyNode::parentRegion() const
{
    //Only the top of the tree is parented by the region, so a node deeper down
    //asks the node above it.
    if(cwSurveyNode* parent = parentNode()) {
        return parent->parentRegion();
    }
    return dynamic_cast<cwCavingRegion*>(parent());
}

cwUnits::UnitSystem cwSurveyNode::unitSystem() const
{
    const cwCavingRegion* region = parentRegion();
    return region ? region->unitSystem() : cwUnits::Metric;
}

void cwSurveyNode::recomputeGridConvergence()
{
    // The readout owns the PROJ work and change detection; we just feed it the
    // current fix stations plus the region CS to fall back on when a fix
    // station omits its own input CS.
    const cwCavingRegion* region = parentRegion();
    const QString fallbackCS = region ? region->geoReference()->globalCoordinateSystem() : QString();
    m_gridConvergence->update(m_fixStations->fixStations(), fallbackCS);
}

/**
 * @brief cwSurveyNode::rowCount
 * @param parent
 * @return Returns the number of trips directly in the node
 */
int cwSurveyNode::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return tripCount();
}

/**
 * @brief cwSurveyNode::data
 * @param index
 * @param role
 * @return
 */
QVariant cwSurveyNode::data(const QModelIndex &index, int role) const
{
   if(!index.isValid()) {
       return QVariant();
   }

   switch(role) {
   case TripObjectRole:
       return QVariant::fromValue(m_trips.at(index.row()));
   default:
       return QVariant();
   }
}

/**
 * @brief cwSurveyNode::roleNames
 * @return Returns the roleNames of the model. See the Qt doc for details
 */
QHash<int, QByteArray> cwSurveyNode::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(TripObjectRole, "tripObjectRole");
    return roles;
}

/**
 * @brief cwSurveyNode::index
 * @param row
 * @param column
 * @param parent
 * @return
 */
QModelIndex cwSurveyNode::index(int row, int column, const QModelIndex &parent) const
{
   return QAbstractListModel::index(row, column, parent);
}

/**
  \brief Sets the undo stack for the node and all of it's children
  */
void cwSurveyNode::setUndoStackForChildren() {
    setUndoStackForChildrenHelper(m_trips);
    setUndoStackForChildrenHelper(m_childNodes);
}


cwSurveyNode::NameCommand::NameCommand(cwSurveyNode* node, QString name) {
    NodePtr = node;
    newName = name;
    oldName = node->name();
    setText(QString("Change cave's name to %1").arg(name));
}

void cwSurveyNode::NameCommand::rename(const QString& from, const QString& to)
{
    //The name lives in whichever sibling set holds this node, and only while it
    //is actually listed there.
    cwSurveyNode* node = NodePtr;
    if(auto* parent = node->parentNode()) {
        if(parent->m_childNodes.contains(node)) {
            parent->childNameSet().rename(from, to);
        }
    }
}

void cwSurveyNode::NameCommand::redo() {
    cwSurveyNode* node = NodePtr;
    rename(oldName, newName);
    node->m_name = newName;
    node->updateSubtreeKeywords();
    emit node->nameChanged();
}


void cwSurveyNode::NameCommand::undo() {
    cwSurveyNode* node = NodePtr;
    rename(newName, oldName);
    node->m_name = oldName;
    node->updateSubtreeKeywords();
    emit node->nameChanged();
}

cwSurveyNode::InsertRemoveTrip::InsertRemoveTrip(cwSurveyNode* node,
                                                   int beginIndex, int endIndex) {
    NodePtr = node;
    BeginIndex = beginIndex;
    EndIndex = endIndex;
    OwnsTrips = false;
}

cwSurveyNode::InsertRemoveTrip::~InsertRemoveTrip() {
    if(OwnsTrips) {
        for(auto trip : std::as_const(Trips)) {
            if(!trip.isNull()) {
                trip->deleteLater();
            }
        }
    }
}

void cwSurveyNode::InsertRemoveTrip::insertTrips() {
    cwSurveyNode* node = NodePtr;
    emit node->beginInsertTrips(BeginIndex, EndIndex);
    emit node->beginInsertRows(QModelIndex(), BeginIndex, EndIndex);
    for(int i = 0; i < Trips.size(); i++) {
        int index = BeginIndex + i;
        cwTrip* trip = Trips.at(i);
        node->m_trips.insert(index, trip);
        node->m_tripNames.insert(trip->name());
        trip->setParentNode(node);
        trip->errorModel()->setParentModel(node->errorModel());
        node->connectTrip(trip);
    }

    OwnsTrips = false;

    //Stale before the first emit: a handler woken by insertedTrips can reach
    //cwTrip::scopePrefix() (cwLinePlotLabelView does, through solvedStations),
    //and a cache still marked fresh would answer it from the trip list as it
    //stood before this insert.
    node->m_tripScopeLabels.invalidate();
    node->m_childScopeLabels.invalidate();

    emit node->endInsertRows();
    emit node->insertedTrips(BeginIndex, EndIndex);

    //Pulsed last, so a consumer woken by it reads a node whose trip list is settled.
    emit node->tripScopeLabelsChanged();
    node->invalidateChildScopeLabelsAfterTripChange();
    emit node->tripCountChanged();
    emit node->subtreeChanged();
}

void cwSurveyNode::InsertRemoveTrip::removeTrips() {
    cwSurveyNode* node = NodePtr;
    emit node->beginRemoveTrips(BeginIndex, EndIndex);
    emit node->beginRemoveRows(QModelIndex(), BeginIndex, EndIndex);

    //Remove all the trips from the back to the front
    for(int i = Trips.size() - 1; i >= 0; i--) {
        int index = BeginIndex + i;
        cwTrip* trip = node->m_trips.at(index);
        node->m_tripNames.remove(trip->name());
        node->m_trips.removeAt(index);

        //Do NOT uncomment, qml engine may garbage collect objects that aren't parented, and can cause double free problem
        // Trips[i]->setParentNode(nullptr);

        trip->errorModel()->setParentModel(nullptr);
        node->disconnectTrip(trip);
    }

    OwnsTrips = true;

    node->m_tripScopeLabels.invalidate();
    node->m_childScopeLabels.invalidate();

    emit node->endRemoveRows();
    emit node->removedTrips(BeginIndex, EndIndex);

    emit node->tripScopeLabelsChanged();
    node->invalidateChildScopeLabelsAfterTripChange();
    emit node->tripCountChanged();
    emit node->subtreeChanged();
}


cwSurveyNode::InsertTripCommand::InsertTripCommand(cwSurveyNode* node,
                                             QList<cwTrip*> trips,
                                             int index) :
    cwSurveyNode::InsertRemoveTrip(node, index, index + trips.size() -1)
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

cwSurveyNode::InsertTripCommand::InsertTripCommand(cwSurveyNode* node,
                                                     cwTrip* Trip,
                                                     int index) :
    cwSurveyNode::InsertRemoveTrip(node, index, index)
{
    Trips.append(Trip);
    setText(QString("Add %1").arg(Trip->name()));
}


void cwSurveyNode::InsertTripCommand::redo() {
    insertTrips();
}

void cwSurveyNode::InsertTripCommand::undo() {
    removeTrips();
}

cwSurveyNode::RemoveTripCommand::RemoveTripCommand(cwSurveyNode* node,
                                                     int beginIndex,
                                                     int endIndex) :
    InsertRemoveTrip(node, beginIndex, endIndex)
{
    for(int i = beginIndex; i <= endIndex; i++) {
       Trips.append(node->trip(i));
    }

    if(beginIndex != endIndex) {
        setText(QStringLiteral("Remove %1 Trips").arg(endIndex - beginIndex + 1));
    } else {
        setText(QStringLiteral("Remove %1").arg(node->trip(beginIndex)->name()));
    }
}

void cwSurveyNode::RemoveTripCommand::redo() {
    removeTrips();
}

void cwSurveyNode::RemoveTripCommand::undo() {
    insertTrips();
}

cwSurveyNode::InsertRemoveNode::InsertRemoveNode(cwSurveyNode* parentNode,
                                                 int beginIndex, int endIndex) {
    ParentPtr = parentNode;
    BeginIndex = beginIndex;
    EndIndex = endIndex;
    OwnsNodes = false;
}

cwSurveyNode::InsertRemoveNode::~InsertRemoveNode() {
    if(OwnsNodes) {
        for(auto node : std::as_const(Nodes)) {
            //A moved node is removed from its old parent and inserted under its
            //new one by two separate commands, so this command can still hold a
            //node that is alive in the tree. A parent that lists the node owns
            //it; only a node no parent lists is this command's to delete.
            if(!node.isNull() && !node->isListedByParent()) {
                node->deleteLater();
            }
        }
    }
}

void cwSurveyNode::InsertRemoveNode::insertNodes() {
    cwSurveyNode* parent = ParentPtr;
    emit parent->beginInsertNodes(BeginIndex, EndIndex);

    //A node that changes parents inherits a new answer for externallyBacked(),
    //which the downward relay alone never pulses: connectNode() wires the relay
    //but the flip happens here.
    QList<cwSurveyNode*> reBackedNodes;

    for(int i = 0; i < Nodes.size(); i++) {
        const int index = BeginIndex + i;
        cwSurveyNode* node = Nodes.at(i);
        const bool wasExternallyBacked = node->externallyBacked();
        parent->m_childNodes.insert(index, node);
        parent->m_childNames.insert(node->name());
        node->setParentNode(parent);
        if(node->externallyBacked() != wasExternallyBacked) {
            reBackedNodes.append(node);
        }
        //A child always shares its parent's stack, the way the region hands its
        //stack to a cave it takes on.
        node->setUndoStack(parent->undoStack());
        parent->connectNode(node);

        //The whole subtree sits at a new path now, so its hierarchy keywords do too.
        node->updateSubtreeKeywords();

        //The subtree may have landed in a different region, whose coordinate
        //system is what a fix station with no input CS of its own falls back to.
        //The node is the object that knows its region moved.
        node->recomputeGridConvergence();
        const QList<cwSurveyNode*> descendants = node->allNodes();
        for(cwSurveyNode* descendant : descendants) {
            descendant->recomputeGridConvergence();
        }
    }

    OwnsNodes = false;

    //Stale before the first emit, for the reason insertTrips() gives.
    parent->m_childScopeLabels.invalidate();

    emit parent->insertedNodes(BeginIndex, EndIndex);
    emit parent->childNodeCountChanged();
    emit parent->childScopeLabelsChanged();

    for(cwSurveyNode* node : std::as_const(reBackedNodes)) {
        emit node->externallyBackedChanged();
    }

    emit parent->subtreeChanged();
}

void cwSurveyNode::InsertRemoveNode::removeNodes() {
    cwSurveyNode* parent = ParentPtr;
    emit parent->beginRemoveNodes(BeginIndex, EndIndex);

    //Remove all the nodes from the back to the front
    for(int i = Nodes.size() - 1; i >= 0; i--) {
        const int index = BeginIndex + i;
        cwSurveyNode* node = parent->m_childNodes.at(index);
        parent->m_childNames.remove(node->name());
        parent->m_childNodes.removeAt(index);

        //The parent pointer and the QObject parent stay set, as a removed trip's
        //do, so undo can re-insert and the QML engine cannot collect the node.
        //externallyBacked() therefore keeps answering from that parent: the value
        //stays where it was, so no notify is due here.
        parent->disconnectNode(node);
    }

    OwnsNodes = true;

    parent->m_childScopeLabels.invalidate();

    emit parent->removedNodes(BeginIndex, EndIndex);
    emit parent->childNodeCountChanged();
    emit parent->childScopeLabelsChanged();
    emit parent->subtreeChanged();
}

cwSurveyNode::InsertNodeCommand::InsertNodeCommand(cwSurveyNode* parentNode,
                                                   const QList<cwSurveyNode*>& nodes,
                                                   int index) :
    cwSurveyNode::InsertRemoveNode(parentNode, index, index + nodes.size() - 1)
{
    Nodes.reserve(nodes.size());
    for(cwSurveyNode* node : nodes) {
        Nodes.append(node);
    }

    if(nodes.size() == 1) {
        setText(QStringLiteral("Add %1").arg(nodes.first()->name()));
    } else {
        setText(QStringLiteral("Add %1 nodes").arg(nodes.size()));
    }
}

cwSurveyNode::InsertNodeCommand::InsertNodeCommand(cwSurveyNode* parentNode,
                                                   cwSurveyNode* node,
                                                   int index) :
    cwSurveyNode::InsertNodeCommand(parentNode, QList<cwSurveyNode*>({node}), index)
{
}

void cwSurveyNode::InsertNodeCommand::redo() {
    insertNodes();
}

void cwSurveyNode::InsertNodeCommand::undo() {
    removeNodes();
}

cwSurveyNode::RemoveNodeCommand::RemoveNodeCommand(cwSurveyNode* parentNode,
                                                   int beginIndex,
                                                   int endIndex) :
    InsertRemoveNode(parentNode, beginIndex, endIndex)
{
    for(int i = beginIndex; i <= endIndex; i++) {
        Nodes.append(parentNode->childNode(i));
    }

    if(beginIndex != endIndex) {
        setText(QString("Remove %1 Nodes").arg(endIndex - beginIndex + 1));
    } else {
        setText(QString("Remove %1").arg(parentNode->childNode(beginIndex)->name()));
    }
}

void cwSurveyNode::RemoveNodeCommand::redo() {
    removeNodes();
}

void cwSurveyNode::RemoveNodeCommand::undo() {
    insertNodes();
}

cwSurveyNode::MoveNodeCommand::MoveNodeCommand(cwSurveyNode* node,
                                               cwSurveyNode* newParent,
                                               int newRow) :
    NodePtr(node),
    OldParentPtr(node->parentNode()),
    NewParentPtr(newParent),
    Remove(node->parentNode(),
           node->parentNode()->indexOfNode(node),
           node->parentNode()->indexOfNode(node)),
    Insert(newParent, node, newRow),
    OldName(node->name())
{
    setText(QStringLiteral("Move %1").arg(node->name()));
}

void cwSurveyNode::MoveNodeCommand::renameWhileUnlisted(const cwSanitizedNameSet& siblingNames,
                                                        const QString& desiredName)
{
    NodePtr->m_name = siblingNames.deduplicateName(desiredName);
}

void cwSurveyNode::MoveNodeCommand::redo()
{
    emit NodePtr->beginMoveNode();

    const QString previousName = NodePtr->name();

    Remove.redo();
    renameWhileUnlisted(NewParentPtr->childNameSet(), previousName);
    Insert.redo();

    //The insert registered the new name and refreshed the subtree's keywords,
    //so all that is left is to say the name moved.
    if(NodePtr->name() != previousName) {
        emit NodePtr->nameChanged();
    }

    emit NodePtr->nodeMoved();
}

void cwSurveyNode::MoveNodeCommand::undo()
{
    emit NodePtr->beginMoveNode();

    const QString previousName = NodePtr->name();

    Insert.undo();
    renameWhileUnlisted(OldParentPtr->childNameSet(), OldName);
    Remove.undo();

    if(NodePtr->name() != previousName) {
        emit NodePtr->nameChanged();
    }

    emit NodePtr->nodeMoved();
}

/**
  \brief Gets all the stations directly in the node

  To figure out how stations are structured see cwTrip and cwSurveyChunk

  Shots and stations are stored in the cwSurveyChunk
  */
QList<cwStation> cwSurveyNode::stations() const {
    QList<cwStation> allStations;
    for(cwTrip* trip : m_trips) {
        allStations.append(trip->stations());
    }
    return allStations;
}

bool cwSurveyNode::validate(const cwEquate& equate) const
{
    if (!equate.isValid()) {
        return false;
    }

    const auto tripContains = [this](const QUuid& tripId) {
        for (const cwTrip* trip : m_trips) {
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
            if (handle.containerId() != m_id) {
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
            // container this node can resolve, so it is never a valid tie.
            return false;
        }
    }

    return true;
}

cwCaveData cwSurveyNode::data() const
{
    return {
        m_name,
        cwData::toDataList<cwTripData>(m_trips),
        m_stationPositionLookup,
        m_id,
        static_cast<cwUnits::LengthUnit>(length()->unit()),
        static_cast<cwUnits::LengthUnit>(depth()->unit()),
        m_fixStations->fixStations(),
        m_externalCenterline,
        cwData::toDataList<cwCaveData>(m_childNodes),
        m_kind,
        m_readOnly,
        m_sourceId,
        m_sourcePath
    };
}

void cwSurveyNode::setData(const cwCaveData &data)
{
    setName(data.name);
    setId(data.id);
    setKind(data.kind);
    setReadOnly(data.readOnly);
    setSourceId(data.sourceId);
    setSourcePath(data.sourcePath);
    length()->setUnit(data.lengthUnit);
    depth()->setUnit(data.depthUnit);
    setExternalCenterline(data.externalCenterline);

    clearNodes();
    clearTrips();

    m_fixStations->setFixStations(data.fixStations);

    //Each child is filled in before it is inserted, so the whole subtree exists
    //by the time this node says a row appeared — the way a trip is filled in
    //before addTrip() announces it.
    for(const auto& nodeData : data.nodes) {
        auto node = new cwCave();
        node->setData(nodeData);
        addNode(node);
    }

    //Insert all trips
    for(const auto& tripData : data.trips) {
        cwTrip* trip = new cwTrip();
        trip->setData(tripData);
        addTrip(trip);
    }
}

/**
  \brief Sets the station position model for the node

  This holds all the position for al the stations in the node (this is populated
  after the loop closure has completed)
  */
void cwSurveyNode::setStationPositionLookup(const cwStationPositionLookup &model) {
    m_stationPositionLookup = model;

    emit stationPositionPositionChanged();
}



/**
 * @brief cwSurveyNode::setSurveyNetwork
 * @param network
 *
 * Holds the calculated survey network for the node. This is a look up for stations and
 * thier neighoring stations
 */
void cwSurveyNode::setSurveyNetwork(const cwSurveyNetwork &network)
{
    m_network = network;
    emit surveyNetworkChanged();
}

/**
 * @brief cwSurveyNode::setStationPositionLookupStale
 * @param isStale
 *
 * Sets the station position lookup as stale. If true, the station position lookup, should
 * be recalculated, else, it shouldn't be recalculated.
 */
void cwSurveyNode::setStationPositionLookupStale(bool isStale)
{
    m_stationPositionLookupStale = isStale;
}

/**
 * @brief cwSurveyNode::isStationPositionLookupStale
 * @return True if the station position lookup is old and stale, false it is up to date
 */
bool cwSurveyNode::isStationPositionLookupStale() const
{
    return m_stationPositionLookupStale;
}
