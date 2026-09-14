/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurveyTreeModel.h"
#include "cwCavingRegion.h"
#include "cwLength.h"
#include "cwSurveyNode.h"
#include "cwTrip.h"
#include "cwUnitValue.h"

//Qt includes
#include <QList>

namespace {
    constexpr int kColumnCount = cwSurveyTreeModel::Actions + 1;

    const QList<int> kNameRoles = {cwSurveyTreeModel::NameRole, Qt::DisplayRole};
    const QList<int> kKindRoles = {cwSurveyTreeModel::KindRole,
                                   cwSurveyTreeModel::KindLabelRole,
                                   cwSurveyTreeModel::MutedRole};
    const QList<int> kSourceRoles = {cwSurveyTreeModel::IsSourcedRole,
                                     cwSurveyTreeModel::IsReadOnlyRole,
                                     cwSurveyTreeModel::IsSourceRootRole};
    const QList<int> kAggregateRoles = {cwSurveyTreeModel::TripCountRole,
                                        cwSurveyTreeModel::LastSurveyRole};
}

cwSurveyTreeModel::cwSurveyTreeModel(QObject* parent) :
    QAbstractItemModel(parent)
{
}

cwSurveyTreeModel::~cwSurveyTreeModel()
{
}

cwCavingRegion* cwSurveyTreeModel::region() const
{
    return m_region;
}

/**
  \brief Shows \a region's tree, dropping every connection the old one held

  A still-wired node left behind would resolve its row against the new region's
  root, so the old tree is unwired before the reset rather than a level at a
  time the way a row removal does it.
  */
void cwSurveyTreeModel::setRegion(cwCavingRegion* region)
{
    if(m_region == region) { return; }

    if(m_region) {
        disconnect(m_region, nullptr, this, nullptr);
        disconnectSubtree(m_region->rootNode());
    }

    beginResetModel();
    m_region = region;
    m_connected.clear();
    endResetModel();

    if(m_region) {
        connect(m_region, &QObject::destroyed, this, [this]() {
            beginResetModel();
            m_connected.clear();
            endResetModel();
        });

        connectSubtree(m_region->rootNode());
    }

    emit regionChanged();
}

QString cwSurveyTreeModel::kindLabel(cwSurveyNode::Kind kind)
{
    switch(kind) {
    case cwSurveyNode::Kind::Cave:
        return QStringLiteral("Cave");
    case cwSurveyNode::Kind::Folder:
        return QStringLiteral("Folder");
    case cwSurveyNode::Kind::CompassProject:
        return QStringLiteral("Compass project");
    case cwSurveyNode::Kind::CompassFile:
        return QStringLiteral("Compass file");
    case cwSurveyNode::Kind::WallsBook:
        return QStringLiteral("Walls book");
    case cwSurveyNode::Kind::SurvexFile:
        return QStringLiteral("Survex file");
    case cwSurveyNode::Kind::SurvexBlock:
        return QStringLiteral("Survex block");
    }
    return QString();
}

QModelIndex cwSurveyTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if(row < 0 || column < 0 || column >= kColumnCount) { return QModelIndex(); }
    if(parent.isValid() && parent.column() > 0) { return QModelIndex(); }

    cwSurveyNode* parentNode = nodeForIndex(parent);
    if(parentNode == nullptr) { return QModelIndex(); }

    if(row < parentNode->childNodeCount()) {
        return createIndex(row, column, parentNode->childNode(row));
    }

    const int tripRow = row - firstTripRow(parentNode);
    if(tripRow < parentNode->tripCount()) {
        return createIndex(row, column, parentNode->trip(tripRow));
    }

    return QModelIndex();
}

QModelIndex cwSurveyTreeModel::parent(const QModelIndex& index) const
{
    if(!index.isValid()) { return QModelIndex(); }

    QObject* object = static_cast<QObject*>(index.internalPointer());

    if(cwSurveyNode* node = qobject_cast<cwSurveyNode*>(object)) {
        return indexOfNode(node->parentNode());
    }

    if(cwTrip* trip = qobject_cast<cwTrip*>(object)) {
        return indexOfNode(trip->parentNode());
    }

    return QModelIndex();
}

int cwSurveyTreeModel::rowCount(const QModelIndex& parent) const
{
    if(parent.isValid() && parent.column() > 0) { return 0; }

    cwSurveyNode* node = nodeForIndex(parent);
    if(node == nullptr) { return 0; }

    return node->childNodeCount() + node->tripCount();
}

int cwSurveyTreeModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return kColumnCount;
}

QVariant cwSurveyTreeModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid()) { return QVariant(); }

    QObject* object = static_cast<QObject*>(index.internalPointer());
    cwSurveyNode* node = qobject_cast<cwSurveyNode*>(object);
    cwTrip* trip = qobject_cast<cwTrip*>(object);

    if(node == nullptr && trip == nullptr) { return QVariant(); }

    //Only the Name column carries display text, so the stock header, the filter,
    //and QAbstractItemModelTester read the name where they expect it.
    if(role == Qt::DisplayRole && index.column() != Name) { return QVariant(); }

    switch(role) {
    case Qt::DisplayRole:
    case NameRole:
        return node != nullptr ? node->name() : trip->name();
    case ObjectRole:
        return QVariant::fromValue(object);
    case RowTypeRole:
        return node != nullptr ? Node : Trip;
    case KindRole:
        return node != nullptr ? QVariant::fromValue(node->kind()) : QVariant();
    case KindLabelRole:
        return node != nullptr ? kindLabel(node->kind()) : QString();
    case IsSourcedRole:
        return node != nullptr ? node->isSourced() : false;
    case IsReadOnlyRole:
        return node != nullptr ? node->isReadOnly() : false;
    case IsSourceRootRole:
        return node != nullptr ? node->isSourceRoot() : false;
    case TripCountRole:
        return node != nullptr ? subtreeTripCount(node) : 0;
    case LengthRole:
        return QVariant::fromValue(node != nullptr ? node->length() : nullptr);
    case DepthValueRole:
        return QVariant::fromValue(node != nullptr ? node->depth() : nullptr);
    case LastSurveyRole:
        return node != nullptr ? lastSurvey(node) : trip->date();
    case MutedRole:
        //A trip reads as a leaf of the node above it, and a Folder's stats are
        //its children's rather than its own.
        return node != nullptr ? node->kind() == cwSurveyNode::Kind::Folder : true;
    default:
        break;
    }

    return QVariant();
}

QVariant cwSurveyTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractItemModel::headerData(section, orientation, role);
    }

    switch(section) {
    case Name:
        return QStringLiteral("Name");
    case Kind:
        return QStringLiteral("Kind");
    case Trips:
        return QStringLiteral("Trips");
    case Length:
        return QStringLiteral("Length");
    case Depth:
        return QStringLiteral("Depth");
    case LastSurvey:
        return QStringLiteral("Last survey");
    case Actions:
        //The row's ⋯ button names itself; a title over it would only add noise.
        return QString();
    default:
        break;
    }

    return QVariant();
}

Qt::ItemFlags cwSurveyTreeModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) { return Qt::NoItemFlags; }

    //Renaming is the delegate's inline editor, not an item-model edit.
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QHash<int, QByteArray> cwSurveyTreeModel::roleNames() const
{
    return {
        {ObjectRole, "object"},
        {RowTypeRole, "rowType"},
        {KindRole, "kind"},
        {KindLabelRole, "kindLabel"},
        {IsSourcedRole, "isSourced"},
        {IsReadOnlyRole, "isReadOnly"},
        {IsSourceRootRole, "isSourceRoot"},
        {TripCountRole, "tripCount"},
        {LengthRole, "length"},
        {DepthValueRole, "depthValue"},
        {LastSurveyRole, "lastSurvey"},
        {MutedRole, "muted"},
        {NameRole, "name"}
    };
}

QModelIndex cwSurveyTreeModel::indexOf(QObject* object) const
{
    if(cwSurveyNode* node = qobject_cast<cwSurveyNode*>(object)) {
        return indexOfNode(node);
    }

    if(cwTrip* trip = qobject_cast<cwTrip*>(object)) {
        return indexOfTrip(trip);
    }

    return QModelIndex();
}

QObject* cwSurveyTreeModel::objectFor(const QModelIndex& index) const
{
    if(!index.isValid()) { return nullptr; }
    return static_cast<QObject*>(index.internalPointer());
}

bool cwSurveyTreeModel::isSourceRootIndex(const QModelIndex& index) const
{
    cwSurveyNode* node = nodeAt(index);
    return node != nullptr && node->isSourceRoot();
}

cwSurveyNode* cwSurveyTreeModel::nodeForIndex(const QModelIndex& index) const
{
    if(m_region.isNull()) { return nullptr; }
    if(index.isValid()) { return nodeAt(index); }
    return m_region->rootNode();
}

cwSurveyNode* cwSurveyTreeModel::nodeAt(const QModelIndex& index) const
{
    if(!index.isValid()) { return nullptr; }
    return qobject_cast<cwSurveyNode*>(static_cast<QObject*>(index.internalPointer()));
}

bool cwSurveyTreeModel::isInTree(const cwSurveyNode* node) const
{
    if(m_region.isNull() || node == nullptr) { return false; }

    const cwSurveyNode* root = m_region->rootNode();
    for(const cwSurveyNode* current = node; current != nullptr; current = current->parentNode()) {
        if(current == root) { return true; }

        //A removed node keeps pointing at the parent it came from so undo can
        //put it back. Only a node its parent still lists owns a row here.
        if(!current->isListedByParent()) { return false; }
    }

    return false;
}

int cwSurveyTreeModel::firstTripRow(const cwSurveyNode* node) const
{
    return node->childNodeCount();
}

QModelIndex cwSurveyTreeModel::indexOfNode(cwSurveyNode* node) const
{
    if(node == nullptr) { return QModelIndex(); }

    //The region's root owns no row of its own — its children are the top-level
    //rows — so the root's index is the invalid one.
    if(node->isRoot()) { return QModelIndex(); }
    if(!isInTree(node)) { return QModelIndex(); }

    cwSurveyNode* parentNode = node->parentNode();
    const int row = parentNode->indexOfNode(node);
    if(row < 0) { return QModelIndex(); }

    return createIndex(row, Name, node);
}

QModelIndex cwSurveyTreeModel::indexOfTrip(cwTrip* trip) const
{
    if(trip == nullptr) { return QModelIndex(); }

    cwSurveyNode* parentNode = trip->parentNode();
    if(!isInTree(parentNode)) { return QModelIndex(); }

    const int tripIndex = parentNode->indexOf(trip);
    if(tripIndex < 0) { return QModelIndex(); }

    return createIndex(firstTripRow(parentNode) + tripIndex, Name, trip);
}

/**
  \brief Wires \a node, its trips, and everything below it
  */
void cwSurveyTreeModel::connectSubtree(cwSurveyNode* node)
{
    if(node == nullptr) { return; }

    connectNode(node);

    for(int i = 0; i < node->tripCount(); i++) {
        connectTrip(node->trip(i));
    }

    const QList<cwSurveyNode*> children = node->childNodes();
    for(cwSurveyNode* child : children) {
        connectSubtree(child);
    }
}

bool cwSurveyTreeModel::trackObject(QObject* object)
{
    if(object == nullptr || m_connected.contains(object)) { return false; }
    m_connected.insert(object);

    connect(object, &QObject::destroyed, this, [this](QObject* dead) {
        m_connected.remove(dead);
    });

    return true;
}

void cwSurveyTreeModel::connectNode(cwSurveyNode* node)
{
    if(!trackObject(node)) { return; }

    connect(node, &cwSurveyNode::beginInsertNodes, this, [this, node](int begin, int end) {
        beginInsertNodeRows(node, begin, end);
    });
    connect(node, &cwSurveyNode::insertedNodes, this, [this, node](int begin, int end) {
        insertedNodeRows(node, begin, end);
    });
    connect(node, &cwSurveyNode::beginRemoveNodes, this, [this, node](int begin, int end) {
        beginRemoveNodeRows(node, begin, end);
    });
    connect(node, &cwSurveyNode::removedNodes, this, [this](int, int) {
        endRemoveRows();
    });

    connect(node, &cwSurveyNode::beginInsertTrips, this, [this, node](int begin, int end) {
        beginInsertTripRows(node, begin, end);
    });
    connect(node, &cwSurveyNode::insertedTrips, this, [this, node](int begin, int end) {
        insertedTripRows(node, begin, end);
    });
    connect(node, &cwSurveyNode::beginRemoveTrips, this, [this, node](int begin, int end) {
        beginRemoveTripRows(node, begin, end);
    });
    connect(node, &cwSurveyNode::removedTrips, this, [this](int, int) {
        endRemoveRows();
    });

    connect(node, &cwSurveyNode::nameChanged, this, [this, node]() {
        emitRowDataChanged(indexOfNode(node), Name, Name, kNameRoles);
    });
    connect(node, &cwSurveyNode::kindChanged, this, [this, node]() {
        //A Folder draws its aggregated stats muted, so MutedRole reaches the
        //Trips, Length, Depth and Last survey cells as well as the Kind chip.
        emitRowDataChanged(indexOfNode(node), Kind, LastSurvey, kKindRoles);
    });
    connect(node, &cwSurveyNode::sourceChanged, this, [this, node]() {
        //The Actions menu greys Rename for a read-only row, so it reads these
        //roles too.
        emitRowDataChanged(indexOfNode(node), Name, Actions, kSourceRoles);
    });
    connect(node, &cwSurveyNode::childNodeCountChanged, this, [this, node]() {
        emitAggregateChanged(node);
    });
    connect(node, &cwSurveyNode::tripCountChanged, this, [this, node]() {
        emitAggregateChanged(node);
    });
    connect(node->length(), &cwUnitValue::valueChanged, this, [this, node]() {
        emitRowDataChanged(indexOfNode(node), Length, Length, {LengthRole});
    });
    connect(node->depth(), &cwUnitValue::valueChanged, this, [this, node]() {
        emitRowDataChanged(indexOfNode(node), Depth, Depth, {DepthValueRole});
    });
    connect(node, &cwSurveyNode::externalCenterlineChanged, this, [this, node]() {
        //The attachment badge the Name cell draws reads the row's object rather
        //than a role, so the whole cell is announced.
        emitRowDataChanged(indexOfNode(node), Name, Name, {});
    });
}

void cwSurveyTreeModel::connectTrip(cwTrip* trip)
{
    if(!trackObject(trip)) { return; }

    connect(trip, &cwTrip::nameChanged, this, [this, trip]() {
        emitRowDataChanged(indexOfTrip(trip), Name, Name, kNameRoles);
    });
    connect(trip, &cwTrip::dateChanged, this, [this, trip]() {
        emitRowDataChanged(indexOfTrip(trip), LastSurvey, LastSurvey, {LastSurveyRole});
        emitAggregateChanged(trip->parentNode());
    });
}

void cwSurveyTreeModel::disconnectSubtree(cwSurveyNode* node)
{
    if(node == nullptr) { return; }

    const QList<cwSurveyNode*> children = node->childNodes();
    for(cwSurveyNode* child : children) {
        disconnectSubtree(child);
    }

    for(int i = 0; i < node->tripCount(); i++) {
        disconnectObject(node->trip(i));
    }

    disconnect(node->length(), nullptr, this, nullptr);
    disconnect(node->depth(), nullptr, this, nullptr);
    disconnectObject(node);
}

void cwSurveyTreeModel::disconnectObject(QObject* object)
{
    if(object == nullptr) { return; }
    m_connected.remove(object);
    disconnect(object, nullptr, this, nullptr);
}

void cwSurveyTreeModel::beginInsertNodeRows(cwSurveyNode* parentNode, int begin, int end)
{
    Q_ASSERT(begin <= end);
    beginInsertRows(indexOfNode(parentNode), begin, end);
}

/**
  \brief Closes the insert, then wires the subtree each new node brought with it

  The rows are announced once, on the parent index, because a node arrives with
  its children already in place. The connections come after endInsertRows(), so
  index(), parent(), and rowCount() are consistent the moment the view reads
  them.
  */
void cwSurveyTreeModel::insertedNodeRows(cwSurveyNode* parentNode, int begin, int end)
{
    Q_ASSERT(begin <= end);

    endInsertRows();

    for(int i = begin; i <= end; i++) {
        connectSubtree(parentNode->childNode(i));
    }
}

void cwSurveyTreeModel::beginRemoveNodeRows(cwSurveyNode* parentNode, int begin, int end)
{
    Q_ASSERT(begin <= end);

    for(int i = begin; i <= end; i++) {
        disconnectSubtree(parentNode->childNode(i));
    }

    beginRemoveRows(indexOfNode(parentNode), begin, end);
}

void cwSurveyTreeModel::beginInsertTripRows(cwSurveyNode* parentNode, int begin, int end)
{
    Q_ASSERT(begin <= end);
    const int first = firstTripRow(parentNode);
    beginInsertRows(indexOfNode(parentNode), begin + first, end + first);
}

void cwSurveyTreeModel::insertedTripRows(cwSurveyNode* parentNode, int begin, int end)
{
    Q_ASSERT(begin <= end);

    endInsertRows();

    for(int i = begin; i <= end; i++) {
        connectTrip(parentNode->trip(i));
    }
}

void cwSurveyTreeModel::beginRemoveTripRows(cwSurveyNode* parentNode, int begin, int end)
{
    Q_ASSERT(begin <= end);

    for(int i = begin; i <= end; i++) {
        disconnectObject(parentNode->trip(i));
    }

    const int first = firstTripRow(parentNode);
    beginRemoveRows(indexOfNode(parentNode), begin + first, end + first);
}

void cwSurveyTreeModel::emitRowDataChanged(const QModelIndex& rowIndex,
                                           Column first,
                                           Column last,
                                           const QList<int>& roles)
{
    if(!rowIndex.isValid()) { return; }

    emit dataChanged(rowIndex.siblingAtColumn(first), rowIndex.siblingAtColumn(last), roles);
}

void cwSurveyTreeModel::emitAggregateChanged(cwSurveyNode* node)
{
    for(cwSurveyNode* current = node; current != nullptr; current = current->parentNode()) {
        emitRowDataChanged(indexOfNode(current), Trips, LastSurvey, kAggregateRoles);
    }
}

int cwSurveyTreeModel::subtreeTripCount(const cwSurveyNode* node)
{
    int count = 0;
    node->walk([&count](const cwSurveyNode* current) {
        count += current->tripCount();
    });
    return count;
}

QDateTime cwSurveyTreeModel::lastSurvey(const cwSurveyNode* node)
{
    QDateTime latest;

    //A walk keeps a repaint free of the list allTrips() would build for every
    //visible row.
    node->walk([&latest](const cwSurveyNode* current) {
        for(int i = 0; i < current->tripCount(); i++) {
            const QDateTime date = current->trip(i)->date();
            if(date.isValid() && (!latest.isValid() || date > latest)) {
                latest = date;
            }
        }
    });

    return latest;
}
