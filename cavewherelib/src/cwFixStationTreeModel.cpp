/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwFixStationTreeModel.h"
#include "cwAttachedFixModel.h"
#include "cwExternalCenterline.h"
#include "cwSurveyTreeModel.h"
#include "cwTrip.h"

//Qt includes
#include <QConcatenateTablesProxyModel>

//Std includes
#include <algorithm>

namespace {

const QList<int> kCountRoles = {
    cwFixStationTreeModel::OwnFixCountRole,
    cwFixStationTreeModel::SubtreeFixCountRole,
    cwFixStationTreeModel::SubtreeBareFixCountRole,
    cwFixStationTreeModel::SubtreeUnsetFixCountRole,
    cwFixStationTreeModel::AttachmentBareFixCountRole
};

const QList<int> kAttachmentRoles = {
    cwFixStationTreeModel::IsAttachmentOwnerRole,
    cwFixStationTreeModel::FixCoordinateSystemRole,
    cwFixStationTreeModel::InheritedFixCoordinateSystemRole,
    cwFixStationTreeModel::InheritedFromNodeIdRole,
    cwFixStationTreeModel::InheritedFromNodeRole,
    cwFixStationTreeModel::AttachmentBareFixCountRole
};

//! The role names every node's fixStationTable() answers. Every table carries
//! the same set, so one node's names stand for all of them.
const QHash<int, QByteArray>& fixTableRoleNames()
{
    static const QHash<int, QByteArray> names = []() {
        const cwSurveyNode node;
        return node.fixStationTable()->roleNames();
    }();
    return names;
}

bool isBare(const cwAttachedFix& fix)
{
    return fix.coordinateSystem.isEmpty();
}

bool isUnset(const cwAttachedFix& fix)
{
    return isBare(fix) && fix.readInSystem.isEmpty();
}

//! How a trip row's kind is drawn.
const QString kTripKindLabel = QStringLiteral("Trip");

int bareCount(const QList<cwAttachedFix>& fixes)
{
    return static_cast<int>(std::count_if(fixes.cbegin(), fixes.cend(), isBare));
}

int unsetCount(const QList<cwAttachedFix>& fixes)
{
    return static_cast<int>(std::count_if(fixes.cbegin(), fixes.cend(), isUnset));
}

//! The nearest node at or above \a node with an attachment of its own.
cwSurveyNode* attachmentOwnerAtOrAbove(const cwSurveyNode* node)
{
    for (const cwSurveyNode* current = node; current != nullptr; current = current->parentNode()) {
        if (!current->externalCenterline().isEmpty()) {
            return const_cast<cwSurveyNode*>(current);
        }
    }
    return nullptr;
}

} // namespace

cwFixStationTreeModel::cwFixStationTreeModel(QObject* parent) :
    QAbstractItemModel(parent)
{
}

void cwFixStationTreeModel::setRootNode(cwSurveyNode* rootNode)
{
    if (m_rootNode == rootNode) {
        return;
    }

    beginResetModel();
    disconnectAll();
    m_rootNode = rootNode;
    connectSubtree(rootNode);
    m_fixRowsDirty = true;
    endResetModel();

    emit rootNodeChanged();
    emit fixRowCountChanged();
}

int cwFixStationTreeModel::fixRowCount() const
{
    rebuildFixRows();
    return m_fixRowCount;
}

QModelIndex cwFixStationTreeModel::indexOfFixRow(int fixRow) const
{
    rebuildFixRows();
    for (auto it = m_firstFixRow.constBegin(); it != m_firstFixRow.constEnd(); ++it) {
        const int first = it.value();
        if (fixRow >= first && fixRow < first + fixRowsOf(it.key())) {
            return fixIndex(it.key(), fixRow - first);
        }
    }
    return QModelIndex();
}

QModelIndex cwFixStationTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (column != 0 || row < 0 || row >= rowCount(parent)) {
        return QModelIndex();
    }
    if (!parent.isValid()) {
        return createIndex(row, column, nullptr);
    }
    QObject* holder = nodeAt(parent);
    if (holder == nullptr) {
        holder = tripAt(parent);
    }
    return createIndex(row, column, holder);
}

QModelIndex cwFixStationTreeModel::parent(const QModelIndex& child) const
{
    const QObject* holder = holderOf(child);
    if (holder == nullptr) {
        return QModelIndex();
    }
    if (const auto* trip = qobject_cast<const cwTrip*>(holder)) {
        return indexOfTrip(trip);
    }
    return indexOfNode(static_cast<const cwSurveyNode*>(holder));
}

int cwFixStationTreeModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid()) {
        return m_rootNode.isNull() ? 0 : 1;
    }
    if (const cwSurveyNode* node = nodeAt(parent)) {
        return node->fixStationCount() + tripRowCount(node) + node->childNodeCount();
    }
    if (const cwTrip* trip = tripAt(parent)) {
        return m_tripFixRows.value(trip);
    }
    return 0;
}

int cwFixStationTreeModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant cwFixStationTreeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (isFixRow(index)) {
        const QObject* holder = holderOf(index);
        const auto* trip = qobject_cast<const cwTrip*>(holder);
        cwSurveyNode* owner = trip != nullptr
                                  ? trip->parentNode()
                                  : const_cast<cwSurveyNode*>(static_cast<const cwSurveyNode*>(holder));
        switch (role) {
        case RowKindRole:
            return FixRow;
        case NodeObjectRole:
            return QVariant::fromValue(owner);
        case TripObjectRole:
            return QVariant::fromValue(const_cast<cwTrip*>(trip));
        case CanHoldFixesRole:
            return owner != nullptr && owner->canHoldFixes();
        case DelegateKindRole: {
            if (trip != nullptr) {
                return ReadOnlyFix;
            }
            const bool readOnly = static_cast<const cwSurveyNode*>(holder)
                ->fixStationTable()->index(index.row(), 0).data(cwAttachedFixModel::ReadOnlyRole).toBool();
            return readOnly ? ReadOnlyFix : EditableFix;
        }
        case FixRowRole:
            if (isLeaving(holder, index.row())) {
                return -1;
            }
            rebuildFixRows();
            return m_firstFixRow.value(holder) + index.row();
        default:
            if (trip != nullptr) {
                return trip->attachedFixes()->index(index.row()).data(role);
            }
            return static_cast<const cwSurveyNode*>(holder)
                ->fixStationTable()->index(index.row(), 0).data(role);
        }
    }

    if (const cwTrip* trip = tripAt(index)) {
        const QList<cwAttachedFix>& fixes = trip->attachedFixes()->attachedFixes();
        const auto owner = [trip]() { return attachmentOwnerAtOrAbove(trip->parentNode()); };
        switch (role) {
        case Qt::DisplayRole:
        case NodeNameRole:
            return trip->name();
        case RowKindRole:
            return TripRow;
        case DelegateKindRole:
            return TripDelegate;
        case NodeIdRole:
            return trip->id();
        case NodeKindLabelRole:
            return kTripKindLabel;
        case NodeObjectRole:
            return QVariant::fromValue(trip->parentNode());
        case TripObjectRole:
            return QVariant::fromValue(const_cast<cwTrip*>(trip));
        case CanHoldFixesRole:
            return false;
        case OwnFixCountRole:
        case SubtreeFixCountRole:
            return static_cast<int>(fixes.size());
        case SubtreeBareFixCountRole:
            return bareCount(fixes);
        case SubtreeUnsetFixCountRole:
            return unsetCount(fixes);
        case AttachmentBareFixCountRole:
            return 0;
        case IsAttachmentOwnerRole:
            return false;
        case FixCoordinateSystemRole:
            return QString();
        case InheritedFixCoordinateSystemRole: {
            const cwSurveyNode* inheritedFrom = owner();
            return inheritedFrom == nullptr ? QString() : inheritedFrom->externalCenterline().fixCoordinateSystem();
        }
        case InheritedFromNodeIdRole: {
            const cwSurveyNode* inheritedFrom = owner();
            return inheritedFrom == nullptr ? QUuid() : inheritedFrom->id();
        }
        case InheritedFromNodeRole:
            return QVariant::fromValue(owner());
        case FixRowRole:
            return -1;
        case BareFixOwnerTripsRole:
            return QVariant::fromValue(QList<cwTrip*>());
        default:
            return QVariant();
        }
    }

    const cwSurveyNode* node = nodeAt(index);
    if (node == nullptr) {
        return QVariant();
    }

    switch (role) {
    case Qt::DisplayRole:
    case NodeNameRole:
        return node->name();
    case RowKindRole:
        return NodeRow;
    case DelegateKindRole:
        return NodeDelegate;
    case NodeIdRole:
        return node->id();
    case NodeKindRole:
        return QVariant::fromValue(node->kind());
    case NodeKindLabelRole:
        return cwSurveyTreeModel::kindLabel(node);
    case NodeObjectRole:
        return QVariant::fromValue(const_cast<cwSurveyNode*>(node));
    case TripObjectRole:
        return QVariant::fromValue(static_cast<cwTrip*>(nullptr));
    case CanHoldFixesRole:
        return node->canHoldFixes();
    case OwnFixCountRole:
        return node->fixStationCount();
    case SubtreeFixCountRole:
        return node->subtreeFixStationCount();
    case SubtreeBareFixCountRole:
        return subtreeBareFixCount(node);
    case SubtreeUnsetFixCountRole:
        return subtreeUnsetFixCount(node);
    case AttachmentBareFixCountRole:
        return attachmentBareFixCount(node);
    case IsAttachmentOwnerRole:
        return !node->externalCenterline().isEmpty();
    case FixCoordinateSystemRole:
        return node->externalCenterline().fixCoordinateSystem();
    case InheritedFixCoordinateSystemRole: {
        const cwSurveyNode* owner = attachmentOwnerAbove(node);
        return owner == nullptr ? QString() : owner->externalCenterline().fixCoordinateSystem();
    }
    case InheritedFromNodeIdRole: {
        const cwSurveyNode* owner = attachmentOwnerAbove(node);
        return owner == nullptr ? QUuid() : owner->id();
    }
    case InheritedFromNodeRole:
        return QVariant::fromValue(attachmentOwnerAbove(node));
    case FixRowRole:
        return -1;
    case BareFixOwnerTripsRole:
        return QVariant::fromValue(m_bareFixOwnerTrips.value(node));
    default:
        return QVariant();
    }
}

bool cwFixStationTreeModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid()) {
        return false;
    }

    if (isFixRow(index)) {
        const QObject* holder = holderOf(index);
        if (const auto* trip = qobject_cast<const cwTrip*>(holder)) {
            cwAttachedFixModel* fixes = trip->attachedFixes();
            return fixes->setData(fixes->index(index.row()), value, role);
        }
        QAbstractItemModel* table = static_cast<const cwSurveyNode*>(holder)->fixStationTable();
        return table->setData(table->index(index.row(), 0), value, role);
    }

    cwSurveyNode* node = nodeAt(index);
    if (node == nullptr || role != FixCoordinateSystemRole || node->externalCenterline().isEmpty()) {
        return false;
    }
    cwExternalCenterline centerline = node->externalCenterline();
    centerline.setFixCoordinateSystem(value.toString());
    node->setExternalCenterline(centerline);
    return true;
}

Qt::ItemFlags cwFixStationTreeModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    if (isFixRow(index)) {
        const QObject* holder = holderOf(index);
        if (const auto* trip = qobject_cast<const cwTrip*>(holder)) {
            const cwAttachedFixModel* fixes = trip->attachedFixes();
            return fixes->flags(fixes->index(index.row()));
        }
        const QAbstractItemModel* table = static_cast<const cwSurveyNode*>(holder)->fixStationTable();
        return table->flags(table->index(index.row(), 0));
    }
    const cwSurveyNode* node = nodeAt(index);
    if (node != nullptr && !node->externalCenterline().isEmpty()) {
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QHash<int, QByteArray> cwFixStationTreeModel::roleNames() const
{
    QHash<int, QByteArray> names = fixTableRoleNames();
    names.insert({
        {RowKindRole,                       "rowKind"},
        {NodeNameRole,                      "nodeName"},
        {NodeIdRole,                        "nodeId"},
        {NodeKindRole,                      "nodeKind"},
        {OwnFixCountRole,                   "ownFixCount"},
        {SubtreeFixCountRole,               "subtreeFixCount"},
        {SubtreeBareFixCountRole,           "subtreeBareFixCount"},
        {IsAttachmentOwnerRole,             "isAttachmentOwner"},
        {FixCoordinateSystemRole,           "fixCoordinateSystem"},
        {InheritedFixCoordinateSystemRole,  "inheritedFixCoordinateSystem"},
        {InheritedFromNodeIdRole,           "inheritedFromNodeId"},
        {NodeObjectRole,                    "node"},
        {NodeKindLabelRole,                 "nodeKindLabel"},
        {AttachmentBareFixCountRole,        "attachmentBareFixCount"},
        {InheritedFromNodeRole,             "inheritedFromNode"},
        {FixRowRole,                        "fixRow"},
        {SubtreeUnsetFixCountRole,          "subtreeUnsetFixCount"},
        {TripObjectRole,                    "trip"},
        {BareFixOwnerTripsRole,             "bareFixOwnerTrips"},
        {DelegateKindRole,                  "delegateKind"},
        {CanHoldFixesRole,                  "canHoldFixes"}
    });
    return names;
}

QObject* cwFixStationTreeModel::holderOf(const QModelIndex& index)
{
    return static_cast<QObject*>(index.internalPointer());
}

cwSurveyNode* cwFixStationTreeModel::nodeAt(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return nullptr;
    }
    const QObject* holder = holderOf(index);
    if (holder == nullptr) {
        return m_rootNode;
    }
    const auto* node = qobject_cast<const cwSurveyNode*>(holder);
    if (node == nullptr) {
        return nullptr;
    }
    const int childRow = index.row() - node->fixStationCount() - tripRowCount(node);
    return childRow >= 0 ? node->childNode(childRow) : nullptr;
}

cwTrip* cwFixStationTreeModel::tripAt(const QModelIndex& index) const
{
    const auto* node = qobject_cast<const cwSurveyNode*>(holderOf(index));
    if (!index.isValid() || node == nullptr) {
        return nullptr;
    }
    const int tripRow = index.row() - node->fixStationCount();
    const QList<cwTrip*> trips = m_tripRows.value(node);
    return tripRow >= 0 && tripRow < trips.size() ? trips.at(tripRow) : nullptr;
}

bool cwFixStationTreeModel::isFixRow(const QModelIndex& index) const
{
    const QObject* holder = holderOf(index);
    if (!index.isValid() || holder == nullptr) {
        return false;
    }
    if (qobject_cast<const cwTrip*>(holder) != nullptr) {
        return true;
    }
    return index.row() < static_cast<const cwSurveyNode*>(holder)->fixStationCount();
}

int cwFixStationTreeModel::fixRowsOf(const QObject* holder) const
{
    if (const auto* trip = qobject_cast<const cwTrip*>(holder)) {
        return m_tripFixRows.value(trip);
    }
    return static_cast<const cwSurveyNode*>(holder)->fixStationCount();
}

int cwFixStationTreeModel::tripRowCount(const cwSurveyNode* node) const
{
    const auto trips = m_tripRows.constFind(node);
    return trips == m_tripRows.constEnd() ? 0 : static_cast<int>(trips->size());
}

QModelIndex cwFixStationTreeModel::indexOfNode(const cwSurveyNode* node) const
{
    if (node == nullptr || m_rootNode.isNull()) {
        return QModelIndex();
    }
    if (node == m_rootNode.data()) {
        return createIndex(0, 0, nullptr);
    }
    cwSurveyNode* holder = node->parentNode();
    if (holder == nullptr || !isInTree(holder)) {
        return QModelIndex();
    }
    const int childRow = holder->indexOfNode(const_cast<cwSurveyNode*>(node));
    if (childRow < 0) {
        return QModelIndex();
    }
    return createIndex(holder->fixStationCount() + tripRowCount(holder) + childRow, 0,
                       static_cast<QObject*>(holder));
}

QModelIndex cwFixStationTreeModel::indexOfTrip(const cwTrip* trip) const
{
    for (auto it = m_tripRows.constBegin(); it != m_tripRows.constEnd(); ++it) {
        const int tripRow = it->indexOf(const_cast<cwTrip*>(trip));
        if (tripRow >= 0) {
            const cwSurveyNode* node = it.key();
            return createIndex(node->fixStationCount() + tripRow, 0,
                               static_cast<QObject*>(const_cast<cwSurveyNode*>(node)));
        }
    }
    return QModelIndex();
}

QModelIndex cwFixStationTreeModel::fixIndex(const QObject* holder, int row) const
{
    return createIndex(row, 0, const_cast<QObject*>(holder));
}

bool cwFixStationTreeModel::isInTree(const cwSurveyNode* node) const
{
    for (const cwSurveyNode* current = node; current != nullptr; current = current->parentNode()) {
        if (current == m_rootNode.data()) {
            return true;
        }
    }
    return false;
}

void cwFixStationTreeModel::connectSubtree(cwSurveyNode* node)
{
    if (node == nullptr) {
        return;
    }
    connectNode(node);
    for (cwSurveyNode* child : node->childNodes()) {
        connectSubtree(child);
    }
}

void cwFixStationTreeModel::connectNode(cwSurveyNode* node)
{
    if (m_connected.contains(node)) {
        return;
    }
    m_connected.insert(node);

    QAbstractItemModel* table = node->fixStationTable();
    connect(table, &QAbstractItemModel::rowsAboutToBeInserted, this,
            [this, node](const QModelIndex&, int first, int last) {
        beginInsertRows(indexOfNode(node), first, last);
    });
    connect(table, &QAbstractItemModel::rowsInserted, this, [this, node]() {
        endInsertRows();
        fixRowsMoved();
        emitCountsChanged(node);
    });
    connect(table, &QAbstractItemModel::rowsAboutToBeRemoved, this,
            [this, node](const QModelIndex&, int first, int last) {
        m_leavingRows = {node, first, last};
        emitFixRowChanged(node, first, last);
        beginRemoveRows(indexOfNode(node), first, last);
    });
    connect(table, &QAbstractItemModel::rowsRemoved, this, [this, node]() {
        m_leavingRows = LeavingRows();
        endRemoveRows();
        fixRowsMoved();
        emitCountsChanged(node);
    });
    // A table only resets when a source's column count moves, which none of
    // the fix tables' sources does; the whole tree follows if one ever does.
    connect(table, &QAbstractItemModel::modelAboutToBeReset, this, [this]() {
        beginResetModel();
    });
    connect(table, &QAbstractItemModel::modelReset, this, [this]() {
        m_fixRowsDirty = true;
        endResetModel();
        emit fixRowCountChanged();
    });
    connect(table, &QAbstractItemModel::dataChanged, this,
            [this, node](const QModelIndex& topLeft, const QModelIndex& bottomRight,
                         const QList<int>& roles) {
        emit dataChanged(fixIndex(node, topLeft.row()), fixIndex(node, bottomRight.row()), roles);
        emitCountsChanged(node);
    });

    connect(node, &cwSurveyNode::beginInsertNodes, this, [this, node](int begin, int end) {
        const int first = node->fixStationCount() + tripRowCount(node);
        beginInsertRows(indexOfNode(node), first + begin, first + end);
    });
    connect(node, &cwSurveyNode::insertedNodes, this, [this, node](int begin, int end) {
        // The new rows arrive with their own trip rows, so those are counted
        // before the insert ends.
        for (int i = begin; i <= end; ++i) {
            connectSubtree(node->childNode(i));
        }
        endInsertRows();
        fixRowsMoved();
        emitCountsChanged(node);
    });
    connect(node, &cwSurveyNode::beginRemoveNodes, this, [this, node](int begin, int end) {
        for (int i = begin; i <= end; ++i) {
            cwSurveyNode* child = node->childNode(i);
            child->walk([this](const cwSurveyNode* leaving) {
                m_leavingHolders.insert(leaving);
                for (cwTrip* trip : m_tripRows.value(leaving)) {
                    m_leavingHolders.insert(trip);
                }
            });
            disconnectSubtree(child);
        }
        for (const QObject* leaving : std::as_const(m_leavingHolders)) {
            emitFixRowChanged(leaving, 0, fixRowsOf(leaving) - 1);
        }
        const int first = node->fixStationCount() + tripRowCount(node);
        beginRemoveRows(indexOfNode(node), first + begin, first + end);
    });
    connect(node, &cwSurveyNode::removedNodes, this, [this, node]() {
        endRemoveRows();
        forgetLeavingNodes();
        fixRowsMoved();
        emitCountsChanged(node);
    });

    connect(node, &cwSurveyNode::insertedTrips, this, [this, node](int begin, int end) {
        for (int i = begin; i <= end; ++i) {
            cwTrip* trip = node->trip(i);
            connectTrip(node, trip);
            const int fixRows = trip->attachedFixes()->count();
            if (fixRows > 0) {
                insertTripRow(node, trip, fixRows);
            }
        }
        refreshBareFixOwnerTrips(node);
        emitCountsChanged(node);
    });
    // The leaving trips drop out of the owners while they still exist.
    connect(node, &cwSurveyNode::beginRemoveTrips, this, [this, node](int begin, int end) {
        QList<cwTrip*> owners = m_bareFixOwnerTrips.value(node);
        for (int i = begin; i <= end; ++i) {
            cwTrip* trip = node->trip(i);
            owners.removeAll(trip);
            removeTripRow(node, trip);
            disconnectTrip(trip);
        }
        setBareFixOwnerTrips(node, owners);
    });
    connect(node, &cwSurveyNode::removedTrips, this, [this, node]() {
        emitCountsChanged(node);
    });

    connect(node, &cwSurveyNode::nameChanged, this, [this, node]() {
        emitNodeRowChanged(node, {Qt::DisplayRole, NodeNameRole});
    });
    connect(node, &cwSurveyNode::kindChanged, this, [this, node]() {
        emitNodeRowChanged(node, {NodeKindRole, NodeKindLabelRole});
    });
    connect(node, &cwSurveyNode::isSectionChanged, this, [this, node]() {
        emitNodeRowChanged(node, {NodeKindLabelRole});
    });
    connect(node, &cwSurveyNode::externalCenterlineChanged, this, [this, node]() {
        emitAttachmentChanged(node);
    });
    connect(node, &cwSurveyNode::sourceChanged, this, [this, node]() {
        emitNodeRowChanged(node, {CanHoldFixesRole});
        emitFixRowChanged(node, 0, fixRowsOf(node) - 1, {CanHoldFixesRole});
        for (const cwTrip* trip : m_tripRows.value(node)) {
            emitFixRowChanged(trip, 0, fixRowsOf(trip) - 1, {CanHoldFixesRole});
        }
    });
    // An owner's bare-fix count reaches over its derived nodes' rows.
    connect(node, &cwSurveyNode::attachedFixesChanged, this, [this, node]() {
        refreshBareFixOwnerTrips(node);
        emitCountsChanged(node);
        if (const cwSurveyNode* owner = attachmentOwnerAbove(node)) {
            emitCountsChanged(owner);
        }
    });

    connect(node, &QObject::destroyed, this, [this, node]() {
        m_connected.remove(node);
        m_bareFixOwnerTrips.remove(node);
        if (m_rootNode.isNull()) {
            beginResetModel();
            disconnectAll();
            m_fixRowsDirty = true;
            endResetModel();
            emit fixRowCountChanged();
        }
    });

    QList<cwTrip*>& tripRows = m_tripRows[node];
    tripRows.clear();
    for (cwTrip* trip : node->trips()) {
        connectTrip(node, trip);
        const int fixRows = trip->attachedFixes()->count();
        if (fixRows > 0) {
            tripRows.append(trip);
            m_tripFixRows.insert(trip, fixRows);
        }
    }
    m_bareFixOwnerTrips.insert(node, bareFixOwnerTrips(node));
}

void cwFixStationTreeModel::connectTrip(cwSurveyNode* node, cwTrip* trip)
{
    if (m_connectedTrips.contains(trip)) {
        return;
    }
    m_connectedTrips.insert(trip);

    const cwAttachedFixModel* fixes = trip->attachedFixes();
    connect(fixes, &QAbstractItemModel::modelAboutToBeReset, this, [this, trip]() {
        tripFixesAboutToReset(trip);
    });
    connect(fixes, &QAbstractItemModel::modelReset, this, [this, node, trip]() {
        tripFixesReset(node, trip);
    });
    connect(fixes, &QAbstractItemModel::dataChanged, this,
            [this, trip](const QModelIndex& topLeft, const QModelIndex& bottomRight,
                         const QList<int>& roles) {
        if (indexOfTrip(trip).isValid()) {
            emit dataChanged(fixIndex(trip, topLeft.row()), fixIndex(trip, bottomRight.row()), roles);
            emitTripRowChanged(trip, kCountRoles);
        }
    });
    connect(trip, &cwTrip::nameChanged, this, [this, trip]() {
        emitTripRowChanged(trip, {Qt::DisplayRole, NodeNameRole});
    });
    connect(trip, &cwTrip::externalCenterlineChanged, this, [this, node]() {
        refreshBareFixOwnerTrips(node);
    });
    connect(trip, &QObject::destroyed, this, [this, trip]() {
        m_connectedTrips.remove(trip);
    });
}

void cwFixStationTreeModel::insertTripRow(cwSurveyNode* node, cwTrip* trip, int fixRows)
{
    const QList<cwTrip*> shown = m_tripRows.value(node);
    int tripRow = 0;
    for (const cwTrip* sibling : node->trips()) {
        if (sibling == trip) {
            break;
        }
        if (shown.contains(sibling)) {
            ++tripRow;
        }
    }

    const int row = node->fixStationCount() + tripRow;
    beginInsertRows(indexOfNode(node), row, row);
    m_tripRows[node].insert(tripRow, trip);
    m_tripFixRows.insert(trip, fixRows);
    endInsertRows();
    fixRowsMoved();
}

void cwFixStationTreeModel::removeTripRow(const cwSurveyNode* node, cwTrip* trip)
{
    const int tripRow = m_tripRows.value(node).indexOf(trip);
    if (tripRow < 0) {
        return;
    }

    m_leavingHolders.insert(trip);
    emitFixRowChanged(trip, 0, m_tripFixRows.value(trip) - 1);
    const int row = node->fixStationCount() + tripRow;
    beginRemoveRows(indexOfNode(node), row, row);
    m_tripRows[node].removeAt(tripRow);
    m_tripFixRows.remove(trip);
    endRemoveRows();
    m_leavingHolders.remove(trip);
    fixRowsMoved();
}

void cwFixStationTreeModel::tripFixesAboutToReset(cwTrip* trip)
{
    const int fixRows = m_tripFixRows.value(trip);
    const QModelIndex tripIndex = indexOfTrip(trip);
    if (!tripIndex.isValid() || fixRows == 0) {
        return;
    }

    m_leavingRows = {trip, 0, fixRows - 1};
    emitFixRowChanged(trip, 0, fixRows - 1);
    beginRemoveRows(tripIndex, 0, fixRows - 1);
    m_tripFixRows.insert(trip, 0);
    endRemoveRows();
    m_leavingRows = LeavingRows();
}

void cwFixStationTreeModel::tripFixesReset(cwSurveyNode* node, cwTrip* trip)
{
    const int fixRows = trip->attachedFixes()->count();
    const QModelIndex tripIndex = indexOfTrip(trip);
    if (!tripIndex.isValid()) {
        if (fixRows > 0) {
            insertTripRow(node, trip, fixRows);
        }
    } else if (fixRows == 0) {
        removeTripRow(node, trip);
    } else {
        beginInsertRows(tripIndex, 0, fixRows - 1);
        m_tripFixRows.insert(trip, fixRows);
        endInsertRows();
        fixRowsMoved();
    }
    emitTripRowChanged(trip, kCountRoles);
}

void cwFixStationTreeModel::forgetLeavingNodes()
{
    for (auto it = m_tripRows.begin(); it != m_tripRows.end();) {
        if (!m_leavingHolders.contains(it.key())) {
            ++it;
            continue;
        }
        for (const cwTrip* trip : std::as_const(*it)) {
            m_tripFixRows.remove(trip);
        }
        it = m_tripRows.erase(it);
    }
    m_leavingHolders.clear();
}

void cwFixStationTreeModel::disconnectSubtree(cwSurveyNode* node)
{
    if (node == nullptr) {
        return;
    }
    for (cwSurveyNode* child : node->childNodes()) {
        disconnectSubtree(child);
    }
    m_connected.remove(node);
    disconnectNode(node);
}

void cwFixStationTreeModel::disconnectAll()
{
    for (cwSurveyNode* node : std::as_const(m_connected)) {
        disconnectNode(node);
    }
    m_connected.clear();
    m_connectedTrips.clear();
    m_bareFixOwnerTrips.clear();
    m_tripRows.clear();
    m_tripFixRows.clear();
}

void cwFixStationTreeModel::disconnectNode(cwSurveyNode* node)
{
    disconnect(node, nullptr, this, nullptr);
    disconnect(node->fixStationTable(), nullptr, this, nullptr);
    for (cwTrip* trip : node->trips()) {
        disconnectTrip(trip);
    }
    m_bareFixOwnerTrips.remove(node);
}

void cwFixStationTreeModel::disconnectTrip(cwTrip* trip)
{
    if (!m_connectedTrips.remove(trip)) {
        return;
    }
    disconnect(trip, nullptr, this, nullptr);
    disconnect(trip->attachedFixes(), nullptr, this, nullptr);
}

void cwFixStationTreeModel::rebuildFixRows() const
{
    if (!m_fixRowsDirty) {
        return;
    }
    m_firstFixRow.clear();
    int next = 0;
    if (!m_rootNode.isNull()) {
        m_rootNode->walk([this, &next](const cwSurveyNode* node) {
            m_firstFixRow.insert(node, next);
            next += node->fixStationCount();
            for (const cwTrip* trip : m_tripRows.value(node)) {
                m_firstFixRow.insert(trip, next);
                next += m_tripFixRows.value(trip);
            }
        });
    }
    m_fixRowCount = next;
    m_fixRowsDirty = false;
}

void cwFixStationTreeModel::fixRowsMoved()
{
    const int oldCount = m_fixRowCount;
    m_fixRowsDirty = true;
    rebuildFixRows();

    for (auto it = m_firstFixRow.constBegin(); it != m_firstFixRow.constEnd(); ++it) {
        emitFixRowChanged(it.key(), 0, fixRowsOf(it.key()) - 1);
    }

    if (m_fixRowCount != oldCount) {
        emit fixRowCountChanged();
    }
}

QModelIndex cwFixStationTreeModel::nodeRowIndex(cwSurveyNode* node) const
{
    return indexOfNode(node);
}

cwSurveyNode* cwFixStationTreeModel::addTargetAt(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return m_rootNode;
    }
    return index.data(NodeObjectRole).value<cwSurveyNode*>();
}

int cwFixStationTreeModel::addFixAt(const QModelIndex& index)
{
    cwSurveyNode* target = addTargetAt(index);
    if (target == nullptr || !target->canHoldFixes()) {
        return -1;
    }
    cwFixStationModel* fixes = target->fixStations();
    fixes->addFixStation();
    return fixIndex(target, fixes->count() - 1).data(FixRowRole).toInt();
}

void cwFixStationTreeModel::announceFixRows()
{
    rebuildFixRows();
    for (auto it = m_firstFixRow.constBegin(); it != m_firstFixRow.constEnd(); ++it) {
        const int count = fixRowsOf(it.key());
        if (count > 0) {
            emit dataChanged(fixIndex(it.key(), 0), fixIndex(it.key(), count - 1), {});
        }
    }
}

bool cwFixStationTreeModel::isLeaving(const QObject* holder, int row) const
{
    return m_leavingHolders.contains(holder)
           || (holder == m_leavingRows.holder && row >= m_leavingRows.first
               && row <= m_leavingRows.last);
}

void cwFixStationTreeModel::emitFixRowChanged(const QObject* holder, int first, int last,
                                              const QList<int>& roles)
{
    if (first <= last) {
        emit dataChanged(fixIndex(holder, first), fixIndex(holder, last), roles);
    }
}

void cwFixStationTreeModel::emitCountsChanged(const cwSurveyNode* node)
{
    for (const cwSurveyNode* current = node; current != nullptr && isInTree(current);
         current = current == m_rootNode.data() ? nullptr : current->parentNode()) {
        emitNodeRowChanged(current, kCountRoles);
    }
}

void cwFixStationTreeModel::emitAttachmentChanged(const cwSurveyNode* node)
{
    node->walk([this](const cwSurveyNode* current) {
        emitNodeRowChanged(current, kAttachmentRoles);
        for (const cwTrip* trip : m_tripRows.value(current)) {
            emitTripRowChanged(trip, kAttachmentRoles);
        }
    });
}

void cwFixStationTreeModel::emitNodeRowChanged(const cwSurveyNode* node, const QList<int>& roles)
{
    const QModelIndex index = indexOfNode(node);
    if (index.isValid()) {
        emit dataChanged(index, index, roles);
    }
}

void cwFixStationTreeModel::refreshBareFixOwnerTrips(const cwSurveyNode* node)
{
    setBareFixOwnerTrips(node, bareFixOwnerTrips(node));
}

void cwFixStationTreeModel::setBareFixOwnerTrips(const cwSurveyNode* node, const QList<cwTrip*>& owners)
{
    auto stored = m_bareFixOwnerTrips.find(node);
    if (stored == m_bareFixOwnerTrips.end() || *stored == owners) {
        return;
    }
    *stored = owners;
    emitNodeRowChanged(node, {BareFixOwnerTripsRole});
}

QList<cwTrip*> cwFixStationTreeModel::bareFixOwnerTrips(const cwSurveyNode* node)
{
    const cwAttachedFixModel* fixes = node->attachedFixes();
    QList<cwTrip*> owners;
    for (cwTrip* trip : node->trips()) {
        if (!trip->externalCenterline().isEmpty() && fixes->bareFixCount(trip->id()) > 0) {
            owners.append(trip);
        }
    }
    return owners;
}

void cwFixStationTreeModel::emitTripRowChanged(const cwTrip* trip, const QList<int>& roles)
{
    const QModelIndex index = indexOfTrip(trip);
    if (index.isValid()) {
        emit dataChanged(index, index, roles);
    }
}

int cwFixStationTreeModel::subtreeBareFixCount(const cwSurveyNode* node)
{
    int count = 0;
    node->walk([&count](const cwSurveyNode* current) {
        count += bareCount(current->listedAttachedFixes());
    });
    return count;
}

int cwFixStationTreeModel::subtreeUnsetFixCount(const cwSurveyNode* node)
{
    int count = 0;
    node->walk([&count](const cwSurveyNode* current) {
        count += unsetCount(current->listedAttachedFixes());
    });
    return count;
}

int cwFixStationTreeModel::attachmentBareFixCount(const cwSurveyNode* owner)
{
    if (owner->externalCenterline().isEmpty()) {
        return 0;
    }
    int count = 0;
    owner->walk([&count, owner](const cwSurveyNode* current) {
        count += current->attachedFixes()->bareFixCount(owner->id());
        for (const cwTrip* trip : current->trips()) {
            count += trip->attachedFixes()->bareFixCount(owner->id());
        }
    });
    return count;
}

cwSurveyNode* cwFixStationTreeModel::attachmentOwnerAbove(const cwSurveyNode* node)
{
    return node->parentNode() == nullptr ? nullptr : attachmentOwnerAtOrAbove(node->parentNode());
}
