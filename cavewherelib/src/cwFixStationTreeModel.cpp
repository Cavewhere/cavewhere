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
        if (fixRow >= first && fixRow < first + it.key()->fixStationCount()) {
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
    return createIndex(row, column, parent.isValid() ? nodeAt(parent) : nullptr);
}

QModelIndex cwFixStationTreeModel::parent(const QModelIndex& child) const
{
    const cwSurveyNode* holder = holderOf(child);
    return holder == nullptr ? QModelIndex() : indexOfNode(holder);
}

int cwFixStationTreeModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid()) {
        return m_rootNode.isNull() ? 0 : 1;
    }
    const cwSurveyNode* node = nodeAt(parent);
    return node == nullptr ? 0 : node->fixStationCount() + node->childNodeCount();
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
        const cwSurveyNode* holder = holderOf(index);
        switch (role) {
        case RowKindRole:
            return FixRow;
        case NodeObjectRole:
            return QVariant::fromValue(const_cast<cwSurveyNode*>(holder));
        case FixRowRole:
            if (isLeaving(holder, index.row())) {
                return -1;
            }
            rebuildFixRows();
            return m_firstFixRow.value(holder) + index.row();
        default:
            return holder->fixStationTable()->index(index.row(), 0).data(role);
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
    case NodeIdRole:
        return node->id();
    case NodeKindRole:
        return QVariant::fromValue(node->kind());
    case NodeKindLabelRole:
        return cwSurveyTreeModel::kindLabel(node);
    case NodeObjectRole:
        return QVariant::fromValue(const_cast<cwSurveyNode*>(node));
    case OwnFixCountRole:
        return node->fixStationCount();
    case SubtreeFixCountRole:
        return subtreeFixCount(node);
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
        QAbstractItemModel* table = holderOf(index)->fixStationTable();
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
        const QAbstractItemModel* table = holderOf(index)->fixStationTable();
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
        {SubtreeUnsetFixCountRole,          "subtreeUnsetFixCount"}
    });
    return names;
}

cwSurveyNode* cwFixStationTreeModel::holderOf(const QModelIndex& index)
{
    return static_cast<cwSurveyNode*>(index.internalPointer());
}

cwSurveyNode* cwFixStationTreeModel::nodeAt(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return nullptr;
    }
    const cwSurveyNode* holder = holderOf(index);
    if (holder == nullptr) {
        return m_rootNode;
    }
    const int childRow = index.row() - holder->fixStationCount();
    return childRow >= 0 ? holder->childNode(childRow) : nullptr;
}

bool cwFixStationTreeModel::isFixRow(const QModelIndex& index)
{
    const cwSurveyNode* holder = holderOf(index);
    return index.isValid() && holder != nullptr && index.row() < holder->fixStationCount();
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
    return createIndex(holder->fixStationCount() + childRow, 0, holder);
}

QModelIndex cwFixStationTreeModel::fixIndex(const cwSurveyNode* node, int row) const
{
    return createIndex(row, 0, const_cast<cwSurveyNode*>(node));
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
        const int first = node->fixStationCount();
        beginInsertRows(indexOfNode(node), first + begin, first + end);
    });
    connect(node, &cwSurveyNode::insertedNodes, this, [this, node](int begin, int end) {
        endInsertRows();
        for (int i = begin; i <= end; ++i) {
            connectSubtree(node->childNode(i));
        }
        fixRowsMoved();
        emitCountsChanged(node);
    });
    connect(node, &cwSurveyNode::beginRemoveNodes, this, [this, node](int begin, int end) {
        for (int i = begin; i <= end; ++i) {
            cwSurveyNode* child = node->childNode(i);
            child->walk([this](const cwSurveyNode* leaving) {
                m_leavingNodes.insert(leaving);
            });
            disconnectSubtree(child);
        }
        for (const cwSurveyNode* leaving : std::as_const(m_leavingNodes)) {
            emitFixRowChanged(leaving, 0, leaving->fixStationCount() - 1);
        }
        const int first = node->fixStationCount();
        beginRemoveRows(indexOfNode(node), first + begin, first + end);
    });
    connect(node, &cwSurveyNode::removedNodes, this, [this, node]() {
        m_leavingNodes.clear();
        endRemoveRows();
        fixRowsMoved();
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
    // An owner's bare-fix count reaches over its derived nodes' rows.
    connect(node, &cwSurveyNode::attachedFixesChanged, this, [this, node]() {
        emitCountsChanged(node);
        if (const cwSurveyNode* owner = attachmentOwnerAbove(node)) {
            emitCountsChanged(owner);
        }
    });

    connect(node, &QObject::destroyed, this, [this, node]() {
        m_connected.remove(node);
        if (m_rootNode.isNull()) {
            beginResetModel();
            disconnectAll();
            m_fixRowsDirty = true;
            endResetModel();
            emit fixRowCountChanged();
        }
    });
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
}

void cwFixStationTreeModel::disconnectNode(cwSurveyNode* node)
{
    disconnect(node, nullptr, this, nullptr);
    disconnect(node->fixStationTable(), nullptr, this, nullptr);
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
        emitFixRowChanged(it.key(), 0, it.key()->fixStationCount() - 1);
    }

    if (m_fixRowCount != oldCount) {
        emit fixRowCountChanged();
    }
}

bool cwFixStationTreeModel::isLeaving(const cwSurveyNode* holder, int row) const
{
    return m_leavingNodes.contains(holder)
           || (holder == m_leavingRows.node && row >= m_leavingRows.first && row <= m_leavingRows.last);
}

void cwFixStationTreeModel::emitFixRowChanged(const cwSurveyNode* node, int first, int last)
{
    if (first <= last) {
        emit dataChanged(fixIndex(node, first), fixIndex(node, last), {FixRowRole});
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
    });
}

void cwFixStationTreeModel::emitNodeRowChanged(const cwSurveyNode* node, const QList<int>& roles)
{
    const QModelIndex index = indexOfNode(node);
    if (index.isValid()) {
        emit dataChanged(index, index, roles);
    }
}

int cwFixStationTreeModel::subtreeFixCount(const cwSurveyNode* node)
{
    int count = 0;
    node->walk([&count](const cwSurveyNode* current) {
        count += current->fixStationCount();
    });
    return count;
}

int cwFixStationTreeModel::subtreeBareFixCount(const cwSurveyNode* node)
{
    int count = 0;
    node->walk([&count](const cwSurveyNode* current) {
        const QList<cwAttachedFix>& fixes = current->attachedFixes()->attachedFixes();
        count += static_cast<int>(std::count_if(fixes.cbegin(), fixes.cend(), isBare));
    });
    return count;
}

int cwFixStationTreeModel::subtreeUnsetFixCount(const cwSurveyNode* node)
{
    int count = 0;
    node->walk([&count](const cwSurveyNode* current) {
        const QList<cwAttachedFix>& fixes = current->attachedFixes()->attachedFixes();
        count += static_cast<int>(std::count_if(fixes.cbegin(), fixes.cend(), isUnset));
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
    });
    return count;
}

cwSurveyNode* cwFixStationTreeModel::attachmentOwnerAbove(const cwSurveyNode* node)
{
    for (cwSurveyNode* current = node->parentNode(); current != nullptr;
         current = current->parentNode()) {
        if (!current->externalCenterline().isEmpty()) {
            return current;
        }
    }
    return nullptr;
}
