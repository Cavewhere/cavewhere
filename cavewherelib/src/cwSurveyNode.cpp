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
#include "cwCavernNaming.h"
#include "cwScopeLabels.h"
#include "cwData.h"
#include "cwFixStationModel.h"
#include "cwGridConvergence.h"
#include "cwFixStation.h"
#include "cwNameUtils.h"
#include "cwKeywordModel.h"
#include "cwTripCalibration.h"

//Qt includes
#include <QFileInfo>
#include <QSet>
#include <QThread>
#include <QUuid>

//Std includes
#include <algorithm>
#include <optional>

namespace {
    //! Joins a node's path names into the one hierarchy string the keyword
    //! search groups by. A node directly under the region has only its own name
    //! in its path, so the separator shows up once a project has folders.
    constexpr auto kNodePathSeparator = QLatin1StringView(" / ");

    //! The solved key \a name resolves to in \a view, or its canonical key
    //! before a solve.
    QString solvedKeyOf(const cwStationPositionLookup& view, const QString& name)
    {
        const QString key = view.key(name);
        return key.isEmpty() ? cwStation::canonicalKey(name) : key;
    }
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
    m_entrances(new cwEntranceModel(this)),
    m_fixStationDiagnostics(new cwFixStationDiagnosticsModel(this)),
    m_attachedFixes(new cwAttachedFixModel(this)),
    m_fixStationTable(new QConcatenateTablesProxyModel(this)),
    m_id(QUuid::createUuid()),
    m_gridConvergence(new cwGridConvergence(this)),
    m_keywordModel(new cwKeywordModel(this)),
    m_isRoot(isRoot)
{
    m_length->setUnit(cwUnits::Meters);
    m_depth->setUnit(cwUnits::Meters);

    m_length->setUpdateValue(true);
    m_depth->setUpdateValue(true);

    m_fixStationTable->addSourceModel(m_fixStationDiagnostics);
    m_fixStationTable->addSourceModel(m_attachedFixes);
    connect(m_fixStationTable, &QAbstractItemModel::rowsInserted,
            this, &cwSurveyNode::fixStationCountChanged);
    connect(m_fixStationTable, &QAbstractItemModel::rowsRemoved,
            this, &cwSurveyNode::fixStationCountChanged);
    connect(m_fixStationTable, &QAbstractItemModel::modelReset,
            this, &cwSurveyNode::fixStationCountChanged);

    connect(this, &cwSurveyNode::fixStationCountChanged,
            this, &cwSurveyNode::updateSubtreeFixStationCount);
    connect(this, &cwSurveyNode::attachedFixesChanged,
            this, &cwSurveyNode::updateSubtreeFixStationCount);
    connect(this, &cwSurveyNode::insertedTrips,
            this, &cwSurveyNode::updateSubtreeFixStationCount);
    connect(this, &cwSurveyNode::removedTrips,
            this, &cwSurveyNode::updateSubtreeFixStationCount);
    connect(this, &cwSurveyNode::insertedNodes,
            this, &cwSurveyNode::updateSubtreeFixStationCount);
    connect(this, &cwSurveyNode::removedNodes,
            this, &cwSurveyNode::updateSubtreeFixStationCount);

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

    // An attachment owner's readout reads the fixes its derived subtree lists,
    // and removing a derived node changes no remaining node's rows.
    connect(this, &cwSurveyNode::attachedFixesChanged,
            this, &cwSurveyNode::subtreeAttachedFixesChanged);
    // Coalesced, since a reconcile inserts its rows one at a time.
    connect(this, &cwSurveyNode::subtreeAttachedFixesChanged,
            this, &cwSurveyNode::scheduleGridConvergence);
    connect(this, &cwSurveyNode::subtreeChanged,
            this, &cwSurveyNode::scheduleGridConvergence);

    wireScopeLabelAggregate();

    recomputeGridConvergence();

    m_takesCaves = computeTakesCaves();
    m_isSection = computeIsSection();
}

cwSurveyNode::~cwSurveyNode() {
    Q_ASSERT(thread() == QThread::currentThread() || thread() == nullptr);
}

void cwSurveyNode::updateSubtreeFixStationCount()
{
    int count = fixStationCount();
    for(const cwTrip* trip : std::as_const(m_trips)) {
        count += trip->attachedFixes()->count();
    }
    for(const cwSurveyNode* child : std::as_const(m_childNodes)) {
        count += child->m_subtreeFixStationCount;
    }

    if(m_subtreeFixStationCount == count) {
        return;
    }
    m_subtreeFixStationCount = count;
    emit subtreeFixStationCountChanged();
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
    updateTakesCaves();
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
    updateTakesCaves();
}

void cwSurveyNode::setSourcePath(const QString& sourcePath)
{
    if (m_sourcePath == sourcePath) {
        return;
    }
    m_sourcePath = sourcePath;
    emit sourceChanged();
}

void cwSurveyNode::setExternalStations(const QStringList& stations)
{
    if (m_externalStations == stations) {
        return;
    }
    m_externalStations = stations;
    emit externalStationsChanged();
}

bool cwSurveyNode::canHoldFixes() const
{
    return !isRoot() && (!isSourced() || isSourceRoot());
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
  \brief Appends a batch of trips as ONE undo command

  The trip twin of addNodes(): the batch is deduplicated against the existing
  trips and against itself, and lands as one ranged insert.
  */
void cwSurveyNode::addTrips(const QList<cwTrip*>& trips)
{
    //A trip another node lists is a move, which insertTrip() takes off its old
    //node first, so a batch holding one goes trip by trip inside a macro.
    const bool holdsAMove = std::any_of(trips.begin(), trips.end(),
                                        [](const cwTrip* trip) {
        return trip != nullptr && InsertRemoveTrip::isListed(trip);
    });
    if(holdsAMove) {
        beginUndoMacro(QStringLiteral("Add %1 trips").arg(trips.size()));
        for(cwTrip* trip : trips) {
            if(trip != nullptr) {
                insertTrip(m_trips.size(), trip);
            }
        }
        endUndoMacro();
        return;
    }

    QList<cwTrip*> inserting;
    inserting.reserve(trips.size());

    //One running copy dedupes the whole batch, as in addNodes().
    cwSanitizedNameSet tripNames = m_tripNames;
    for(cwTrip* trip : trips) {
        if(trip == nullptr) {
            continue;
        }
        const QString deduped = tripNames.deduplicateName(trip->name());
        if(deduped != trip->name()) {
            trip->setName(deduped);
        }
        tripNames.insert(trip->name());
        inserting.append(trip);
    }

    if(!inserting.isEmpty()) {
        pushUndo(new InsertTripCommand(this, inserting, m_trips.size()));
    }
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

    //The trip owns solvedStations() but reads this node's scoped view, so the
    //region's pulse reaches it through this node.
    connect(this, &cwSurveyNode::scopedPositionsChanged,
            trip, &cwTrip::solvedStationsChanged, Qt::UniqueConnection);

    //A harvest is one trip's news, but knownStations() is read across the node,
    //so this node is the only object that can say some trip in it learned names.
    connect(trip, &cwTrip::externalStationsChanged,
            this, &cwSurveyNode::tripExternalStationsChanged, Qt::UniqueConnection);

    //A derived trip's attached fixes are this node's to list and validate.
    connect(trip, &cwTrip::attachedFixesChanged,
            this, &cwSurveyNode::attachedFixesChanged, Qt::UniqueConnection);

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
    disconnect(this, &cwSurveyNode::scopedPositionsChanged,
               trip, &cwTrip::solvedStationsChanged);
    disconnect(trip, &cwTrip::externalStationsChanged,
               this, &cwSurveyNode::tripExternalStationsChanged);
    disconnect(trip, &cwTrip::attachedFixesChanged,
               this, &cwSurveyNode::attachedFixesChanged);
    disconnect(this, &cwSurveyNode::externallyBackedChanged,
               trip, &cwTrip::externallyBackedChanged);
}

void cwSurveyNode::connectNode(cwSurveyNode* node)
{
    //Each aggregate travels exactly one hop upward, so a depth-N tree pulses
    //once per level. UniqueConnection because undo/redo re-inserts the same node.
    connect(node, &cwSurveyNode::subtreeChanged,
            this, &cwSurveyNode::subtreeChanged, Qt::UniqueConnection);
    connect(node, &cwSurveyNode::subtreeAttachedFixesChanged,
            this, &cwSurveyNode::subtreeAttachedFixesChanged, Qt::UniqueConnection);
    connect(node, &cwSurveyNode::subtreeFixStationCountChanged,
            this, &cwSurveyNode::updateSubtreeFixStationCount, Qt::UniqueConnection);
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

    //The region's solve reaches every node through these, so a moved node
    //hears it from its new parent with no wiring of its own.
    connect(this, &cwSurveyNode::scopedPositionsChanged,
            node, &cwSurveyNode::scopedPositionsChanged, Qt::UniqueConnection);
    connect(this, &cwSurveyNode::scopedNetworkChanged,
            node, &cwSurveyNode::scopedNetworkChanged, Qt::UniqueConnection);

    //A node's badge counts its whole subtree, the way it counts its trips.
    node->errorModel()->setParentModel(m_errorModel);
}

void cwSurveyNode::disconnectNode(cwSurveyNode* node)
{
    disconnect(node, &cwSurveyNode::subtreeChanged,
               this, &cwSurveyNode::subtreeChanged);
    disconnect(node, &cwSurveyNode::subtreeAttachedFixesChanged,
               this, &cwSurveyNode::subtreeAttachedFixesChanged);
    disconnect(node, &cwSurveyNode::subtreeFixStationCountChanged,
               this, &cwSurveyNode::updateSubtreeFixStationCount);
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
    disconnect(this, &cwSurveyNode::scopedPositionsChanged,
               node, &cwSurveyNode::scopedPositionsChanged);
    disconnect(this, &cwSurveyNode::scopedNetworkChanged,
               node, &cwSurveyNode::scopedNetworkChanged);
    node->errorModel()->setParentModel(nullptr);
}

void cwSurveyNode::setParentNode(cwSurveyNode* node)
{
    if(m_parentNode == node) {
        return;
    }
    m_parentNode = node;
    setParent(node);
    emit parentNodeChanged();
    updateTakesCaves();
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

bool cwSurveyNode::isSelfOrDescendantOf(const cwSurveyNode* node) const
{
    for(const cwSurveyNode* current = this; current != nullptr; current = current->parentNode()) {
        if(current == node) {
            return true;
        }
        if(!current->isListedByParent()) {
            return false;
        }
    }
    return false;
}

QList<const cwSurveyNode*> cwSurveyNode::ancestorsOrSelf() const
{
    QList<const cwSurveyNode*> chain;
    for(const cwSurveyNode* node = this; node != nullptr; node = node->parentNode()) {
        chain.append(node);
    }
    return chain;
}

bool cwSurveyNode::computeTakesCaves() const
{
    if(m_isRoot) {
        return true;
    }
    if(m_kind != Kind::Folder) {
        return false;
    }

    const QList<const cwSurveyNode*> chain = ancestorsOrSelf();
    for(const cwSurveyNode* node : chain) {
        if(node->isRoot()) {
            break;
        }
        if(node->kind() == Kind::Cave || node->isSourced()) {
            return false;
        }
    }
    return true;
}

bool cwSurveyNode::computeIsSection() const
{
    return m_kind == Kind::Folder && !m_takesCaves;
}

void cwSurveyNode::updateTakesCaves()
{
    const bool takesCaves = computeTakesCaves();
    if(m_takesCaves != takesCaves) {
        m_takesCaves = takesCaves;
        emit takesCavesChanged();
    }

    const bool isSection = computeIsSection();
    if(m_isSection != isSection) {
        m_isSection = isSection;
        emit isSectionChanged();
    }

    for(cwSurveyNode* child : std::as_const(m_childNodes)) {
        child->updateTakesCaves();
    }
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

void cwSurveyNode::scheduleGridConvergence()
{
    if (m_gridConvergenceScheduled) {
        return;
    }
    m_gridConvergenceScheduled = true;
    QMetaObject::invokeMethod(this, [this]() {
        if (m_gridConvergenceScheduled) {
            recomputeGridConvergence();
        }
    }, Qt::QueuedConnection);
}

void cwSurveyNode::recomputeGridConvergence()
{
    m_gridConvergenceScheduled = false;
    // The readout owns the PROJ work and change detection; we just feed it the
    // current fix stations and the grid they are plotted in. That grid is the
    // project's local projection — cavern solves under it — so a node with no
    // region has none, and converges to nothing.
    const cwCavingRegion* region = parentRegion();
    const QString frameCS = region == nullptr
        ? QString()
        : region->geoReference()->localCoordinateSystem();
    m_gridConvergence->update(convergenceFixes(), frameCS);
}

QList<cwFixStation> cwSurveyNode::convergenceFixes() const
{
    QList<cwFixStation> fixes = m_fixStations->fixStations();
    if (std::any_of(fixes.cbegin(), fixes.cend(), &cwGridConvergence::placesCave)) {
        return fixes;
    }

    const auto placedIn = [](const cwSurveyNode* holder) -> std::optional<cwFixStation> {
        for (const cwAttachedFix& attached : holder->listedAttachedFixes()) {
            const std::optional<cwAttachedFix::Placement> placement =
                attached.placementIn(attached.solveSystem());
            if (placement.has_value()) {
                cwFixStation fix;
                fix.setStationName(attached.station);
                // The system decides the axis order, so it goes in before the numbers.
                fix.setInputCS(placement->cs);
                fix.setCoordinate(placement->point.x, placement->point.y, placement->point.z);
                return fix;
            }
        }
        return std::nullopt;
    };

    // The attached fixes the driver solves: this node's own rows, then, for a
    // sourced node, those its derived subtree lists below it, pre-order.
    const cwSurveyNode* source = sourceRoot();
    const auto findBelow = [&](const auto& self, const cwSurveyNode* holder) -> std::optional<cwFixStation> {
        if (const std::optional<cwFixStation> fix = placedIn(holder)) {
            return fix;
        }
        if (source == nullptr) {
            return std::nullopt;
        }
        for (const cwSurveyNode* child : holder->m_childNodes) {
            if (child->sourceId() == source->sourceId() && !child->isSourceRoot()) {
                if (const std::optional<cwFixStation> fix = self(self, child)) {
                    return fix;
                }
            }
        }
        return std::nullopt;
    };

    if (const std::optional<cwFixStation> fix = findBelow(findBelow, this)) {
        fixes.append(*fix);
    }
    return fixes;
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
            //A moved trip is removed from its old node and inserted under its
            //new one, so this command can still hold a trip alive in the tree.
            //A node that lists the trip owns it.
            if(!trip.isNull() && !isListed(trip)) {
                trip->deleteLater();
            }
        }
    }
}

bool cwSurveyNode::InsertRemoveTrip::isListed(const cwTrip* trip)
{
    const cwSurveyNode* node = trip->parentNode();
    return node != nullptr && node->m_trips.contains(const_cast<cwTrip*>(trip));
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

        //The subtree may have landed in a different region, whose local
        //projection is the grid its convergence is measured in. The node is the
        //object that knows its region moved.
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

void cwSurveyNode::setAttachedFixes(const QList<cwAttachedFix>& fixes)
{
    if (m_attachedFixes->attachedFixes() == fixes) {
        return;
    }
    m_attachedFixes->setAttachedFixes(fixes);
    emit attachedFixesChanged();
}

QList<cwAttachedFix> cwSurveyNode::listedAttachedFixes() const
{
    QList<cwAttachedFix> fixes = m_attachedFixes->attachedFixes();
    for (const cwTrip* trip : trips()) {
        fixes.append(trip->attachedFixes()->attachedFixes());
    }
    return fixes;
}

void cwSurveyNode::setFileFixedStations(const QHash<QUuid, QStringList>& stationsByOwner)
{
    if (m_fileFixedStations == stationsByOwner) {
        return;
    }
    m_fileFixedStations = stationsByOwner;
    emit attachedFixesChanged();
}

QHash<QString, QString> cwSurveyNode::fileFixedStations() const
{
    QHash<QString, QString> stations;
    const cwStationPositionLookup view = scopedPositions();
    const auto addOwner = [&](const QUuid& ownerId, const QString& scope,
                              const cwExternalCenterline& centerline) {
        const QString fileName = QFileInfo(centerline.entryFile()).fileName();
        for (const QString& station : m_fileFixedStations.value(ownerId)) {
            stations.insert(solvedKeyOf(view, scope + station), fileName);
        }
    };
    addOwner(id(), QString(), m_externalCenterline);
    for (const cwTrip* trip : trips()) {
        addOwner(trip->id(), trip->scopePrefix(), trip->externalCenterline());
    }
    return stations;
}

cwSurveyNode* cwSurveyNode::caveNode()
{
    for(cwSurveyNode* node = this; node != nullptr && !node->isRoot(); node = node->parentNode()) {
        if(node->kind() == Kind::Cave) {
            return node;
        }
    }
    return this;
}

int cwSurveyNode::addFixedEntrance(const QString& stationName)
{
    if (!canHoldFixes()) {
        return -1;
    }

    const int existingRow = m_fixStations->indexOf(stationName);
    if (existingRow >= 0) {
        return existingRow;
    }

    const int row = m_fixStations->addFixStation(stationName);
    if (row >= 0) {
        m_entrances->addEntrance(stationName);
    }
    return row;
}

void cwSurveyNode::removeFixAndEntrance(const QString& stationName)
{
    m_fixStations->removeFixStation(stationName);
    m_entrances->removeEntrance(stationName);
}

void cwSurveyNode::removeFixAndEntranceAt(int fixRow)
{
    if (fixRow < 0 || fixRow >= m_fixStations->count()) {
        return;
    }
    const QString stationName = m_fixStations->fixStationAt(fixRow).stationName();
    m_fixStations->removeAt(fixRow);
    m_entrances->removeEntrance(stationName);
}

bool cwSurveyNode::setFixStationName(int fixRow, const QString& stationName)
{
    if (fixRow < 0 || fixRow >= m_fixStations->count()) {
        return false;
    }

    const QString oldName = m_fixStations->fixStationAt(fixRow).stationName();
    if (!m_fixStations->setData(m_fixStations->index(fixRow), stationName,
                                cwFixStationModel::StationNameRole)) {
        return false;
    }

    const QString trimmedOldName = oldName.trimmed();
    const QString newName = stationName.trimmed();
    const bool sameStation = cwStation::canonicalKey(trimmedOldName) == cwStation::canonicalKey(newName);
    if (newName.isEmpty() || (!sameStation && m_entrances->isEntrance(newName))) {
        return true;
    }

    if (trimmedOldName.isEmpty()) {
        m_entrances->addEntrance(newName);
    } else {
        const int entranceRow = m_entrances->indexOf(oldName);
        if (entranceRow >= 0) {
            m_entrances->setData(m_entrances->index(entranceRow), newName,
                                 cwEntranceModel::StationNameRole);
        }
    }
    return true;
}

cwCaveData cwSurveyNode::data() const
{
    return {
        m_name,
        cwData::toDataList<cwTripData>(m_trips),
        m_id,
        static_cast<cwUnits::LengthUnit>(length()->unit()),
        static_cast<cwUnits::LengthUnit>(depth()->unit()),
        m_fixStations->fixStations(),
        m_entrances->entrances(),
        m_externalCenterline,
        cwData::toDataList<cwCaveData>(m_childNodes),
        m_kind,
        m_readOnly,
        m_sourceId,
        m_sourcePath,
        m_externalStations,
        m_cavernLabel
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
    m_entrances->setEntrances(data.entrances);

    //Each child is filled in before it is inserted, so the whole subtree exists
    //by the time this node says a row appeared — the way the trips are filled
    //in before addTrips() announces them.
    for(const auto& nodeData : data.nodes) {
        auto node = new cwCave();
        node->setData(nodeData);
        addNode(node);
    }

    //One ranged insert pulses tripScopeLabelsChanged once for the whole fill,
    //where a per-trip insert would reach every held trip on each insert.
    QList<cwTrip*> trips;
    trips.reserve(data.trips.size());
    for(const auto& tripData : data.trips) {
        cwTrip* trip = new cwTrip();
        trip->setData(tripData);
        trips.append(trip);
    }
    addTrips(trips);
}

QString cwSurveyNode::scopePrefix() const
{
    QString prefix;
    for (const cwSurveyNode* node : pathNodes()) {
        const cwSurveyNode* parent = node->parentNode();
        const QString label = parent != nullptr ? parent->childScopeLabels().value(node->id())
                                                : QString();
        if (label.isEmpty()) {
            return QString();
        }
        prefix += cwCavernNaming::scopePrefix(label);
    }
    return prefix;
}

cwStationPositionLookup cwSurveyNode::scopedPositions() const
{
    const cwCavingRegion* region = parentRegion();
    if (region == nullptr) {
        return cwStationPositionLookup();
    }
    if (isRoot()) {
        return region->stationPositions();
    }
    if (!isSourceRoot() && sourceRoot() != nullptr) {
        return region->memberPositions(this);
    }
    const QString prefix = scopePrefix();
    return prefix.isEmpty() ? cwStationPositionLookup() : region->scopedPositions(prefix);
}

cwSurveyNetwork cwSurveyNode::scopedNetwork() const
{
    const cwCavingRegion* region = parentRegion();
    if (region == nullptr) {
        return cwSurveyNetwork();
    }
    if (isRoot()) {
        return region->network();
    }
    if (!isSourceRoot() && sourceRoot() != nullptr) {
        return region->memberNetwork(this);
    }
    const QString prefix = scopePrefix();
    return prefix.isEmpty() ? cwSurveyNetwork() : region->scopedNetwork(prefix);
}

QString cwSurveyNode::cavernPath() const
{
    const cwSurveyNode* root = sourceRoot();
    if (root == nullptr) {
        return QString();
    }
    QString path;
    for (const cwSurveyNode* node = this; node != nullptr && node != root;
         node = node->parentNode()) {
        path.prepend(cwCavernNaming::scopePrefix(node->cavernLabel()));
    }
    return path;
}

bool cwSurveyNode::SourceMembers::contains(const QString& key) const
{
    return stations.contains(key)
           || std::any_of(scopePrefixes.cbegin(), scopePrefixes.cend(),
                          [&key](const QString& prefix) {
                              return key.startsWith(prefix);
                          });
}

cwSurveyNode::SourceMembers cwSurveyNode::sourceMembers() const
{
    SourceMembers members;
    const cwSurveyNode* root = sourceRoot();
    if (root == nullptr) {
        return members;
    }

    const QHash<QUuid, QString> nativeLabels = root->sourcedTripScopeLabels();
    const cwStationPositionLookup rootView = root->scopedPositions();
    walk([&](const cwSurveyNode* node) {
        const QString nodePath = cwStation::canonicalKey(node->cavernPath());
        for (const cwTrip* trip : node->trips()) {
            if (!trip->sourcePath().isEmpty()) {
                const QString tripPath = nodePath + cwStation::canonicalKey(trip->scopePrefix());
                for (const QString& station : trip->externalStations()) {
                    members.stations.insert(solvedKeyOf(rootView, tripPath + station));
                }
                continue;
            }
            const QString label = nativeLabels.value(trip->id());
            if (!label.isEmpty()) {
                members.scopePrefixes.append(cwCavernNaming::scopePrefix(label));
            }
        }
    });
    return members;
}

QHash<QUuid, QString> cwSurveyNode::sourcedTripScopeLabels() const
{
    const cwSurveyNode* root = sourceRoot();
    if (root == nullptr) {
        return QHash<QUuid, QString>();
    }

    //The live twin of cwScopeLabels' pool, in the same pre-order and with the
    //same reserved heads, so a native trip's live label is the one the driver
    //writes.
    QList<cwCavernNaming::ScopeEntry> entries;
    QStringList cavernLabels;
    root->walk([&](const cwSurveyNode* node) {
        cavernLabels.append(node->cavernLabel());
        for (const cwTrip* trip : node->trips()) {
            if (trip->sourcePath().isEmpty() && trip->externalCenterline().isEmpty()) {
                entries.append({trip->id(), trip->name()});
            } else {
                cavernLabels.append(trip->cavernLabel());
            }
        }
    });
    if (entries.isEmpty()) {
        return QHash<QUuid, QString>();
    }
    return cwCavernNaming::scopeLabels(
        entries, cwScopeLabels::includedScopeHeads(root->externalStations(), cavernLabels));
}
