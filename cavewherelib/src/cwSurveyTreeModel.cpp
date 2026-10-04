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
#include "cwTripCalibration.h"
#include "cwTripStatsWatcher.h"
#include "cwUnitValue.h"

//Qt includes
#include <QList>

namespace {
    constexpr int kColumnCount = cwSurveyTreeModel::Decl + 1;

    const QList<int> kNameRoles = {cwSurveyTreeModel::NameRole, Qt::DisplayRole};
    const QList<int> kKindRoles = {cwSurveyTreeModel::KindRole,
                                   cwSurveyTreeModel::KindLabelRole,
                                   cwSurveyTreeModel::MutedRole};
    const QList<int> kSourceRoles = {cwSurveyTreeModel::IsSourcedRole,
                                     cwSurveyTreeModel::IsSourceRootRole};
    const QList<int> kAggregateRoles = {cwSurveyTreeModel::TripCountRole,
                                        cwSurveyTreeModel::StationCountRole};
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

    cancelMove();

    if(m_region) {
        disconnect(m_region, nullptr, this, nullptr);
        disconnectSubtree(m_region->rootNode());
    }

    beginResetModel();
    m_region = region;
    m_connected.clear();
    qDeleteAll(m_tripStats);
    m_tripStats.clear();
    endResetModel();

    if(m_region) {
        connect(m_region, &QObject::destroyed, this, [this]() {
            beginResetModel();
            m_connected.clear();
            qDeleteAll(m_tripStats);
            m_tripStats.clear();
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
    case IsSourceRootRole:
        return node != nullptr ? node->isSourceRoot() : false;
    case TripCountRole:
        return node != nullptr ? subtreeTripCount(node) : 0;
    case StationCountRole:
        return node != nullptr ? subtreeStationCount(node) : tripStationCount(trip);
    case UsedStationsRole:
        //A node holds the stations of every trip under it, far more than a cell
        //can name, so only a trip row names its own.
        return node != nullptr ? QString() : tripUsedStations(trip);
    case LengthRole:
        //A node carries a solved cwLength object the cell reads unit and all; a
        //trip has only the number its length task adds up, in the unit the
        //trip's own calibration is surveyed in.
        return node != nullptr
                ? QVariant::fromValue(node->length())
                : QVariant::fromValue(tripLength(trip));
    case DepthValueRole:
        return QVariant::fromValue(node != nullptr ? node->depth() : nullptr);
    case DateRole:
        //A node holds trips surveyed on many days, so only a trip row names a
        //date.
        return node != nullptr ? QDateTime() : trip->date();
    case DeclinationRole:
        //A node row states 0.0 rather than nothing, so the delegate's real
        //property is always handed a number.
        return node != nullptr ? 0.0 : trip->calibrations()->declination();
    case AutoDeclinationRole:
        return node != nullptr
                ? false
                : trip->calibrations()->autoDeclination()
                      && trip->calibrations()->autoDeclinationAvailable();
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
    case Stations:
        return QStringLiteral("Stations");
    case Length:
        return QStringLiteral("Length");
    case Depth:
        return QStringLiteral("Depth");
    case Date:
        return QStringLiteral("Date");
    case Decl:
        return QStringLiteral("Declination");
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
        //Qt::DisplayRole is named as well as numbered: HorizontalHeaderView
        //looks the column titles up by the role name "display".
        {Qt::DisplayRole, "display"},
        {ObjectRole, "object"},
        {RowTypeRole, "rowType"},
        {KindRole, "kind"},
        {KindLabelRole, "kindLabel"},
        {IsSourcedRole, "isSourced"},
        {IsSourceRootRole, "isSourceRoot"},
        {TripCountRole, "tripCount"},
        {StationCountRole, "stationCount"},
        {UsedStationsRole, "usedStations"},
        {LengthRole, "length"},
        //depthValue and dateValue carry the names their roles hold because
        //"depth" and "date" are already taken in a delegate: one by TreeView's
        //own depth property, the other by QML's date type.
        {DepthValueRole, "depthValue"},
        {DateRole, "dateValue"},
        {DeclinationRole, "declination"},
        {AutoDeclinationRole, "autoDeclination"},
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

bool cwSurveyTreeModel::isMovableIndex(const QModelIndex& index) const
{
    return cwSurveyNode::isMovable(objectFor(index));
}

void cwSurveyTreeModel::startMove(const QModelIndex& index)
{
    cancelMove();

    QObject* subject = objectFor(index);
    if(!cwSurveyNode::isMovable(subject)) { return; }

    m_moveSubject = subject;

    //A subject destroyed while armed leaves the QPointer null with nothing
    //said, and the banner would keep asking about it.
    m_moveSubjectDestroyed = connect(subject, &QObject::destroyed, this, [this]() {
        m_moveSubject = nullptr;
        emit moveChanged();
    }, Qt::SingleShotConnection);

    emit moveChanged();
}

void cwSurveyTreeModel::cancelMove()
{
    if(m_moveSubject.isNull()) { return; }

    disconnect(m_moveSubjectDestroyed);
    m_moveSubject = nullptr;
    emit moveChanged();
}

bool cwSurveyTreeModel::commitMove(const QModelIndex& targetIndex)
{
    if(!isMoveTarget(targetIndex)) { return false; }

    QObject* subject = m_moveSubject;
    cwSurveyNode* destination = nodeForIndex(targetIndex);

    //Disarmed first, so the rows the move re-announces draw unarmed.
    cancelMove();

    return destination->moveHere(subject);
}

bool cwSurveyTreeModel::isMoveTarget(const QModelIndex& index) const
{
    if(!moveActive()) { return false; }

    //The invalid index is the region's root, and a trip row is no node at all.
    const cwSurveyNode* destination = nodeForIndex(index);
    return destination != nullptr && destination->moveRefusal(m_moveSubject).isEmpty();
}

bool cwSurveyTreeModel::isMoveSource(const QModelIndex& index) const
{
    if(!moveActive() || !index.isValid()) { return false; }

    QObject* object = objectFor(index);
    if(object == m_moveSubject) { return true; }

    const auto* movingNode = qobject_cast<const cwSurveyNode*>(m_moveSubject);
    if(movingNode == nullptr) { return false; }

    const cwTrip* trip = qobject_cast<const cwTrip*>(object);
    const cwSurveyNode* node = trip != nullptr ? trip->parentNode()
                                               : qobject_cast<const cwSurveyNode*>(object);
    for(; node != nullptr; node = node->parentNode()) {
        if(node == movingNode) { return true; }
    }
    return false;
}

QString cwSurveyTreeModel::moveTargetReason(const QModelIndex& index) const
{
    if(!moveActive()) { return QString(); }

    if(objectFor(index) == m_moveSubject) {
        return tr("This is what's moving");
    }

    const cwSurveyNode* destination = nodeForIndex(index);
    if(destination == nullptr) {
        return tr("A trip holds no other survey");
    }
    return destination->moveRefusal(m_moveSubject);
}

QString cwSurveyTreeModel::moveConfirmation(const QModelIndex& targetIndex) const
{
    if(!isMoveTarget(targetIndex)) { return QString(); }
    return nodeForIndex(targetIndex)->moveConsequences(m_moveSubject);
}

QString cwSurveyTreeModel::moveSubjectName() const
{
    if(const auto* node = qobject_cast<const cwSurveyNode*>(m_moveSubject)) {
        return node->name();
    }
    if(const auto* trip = qobject_cast<const cwTrip*>(m_moveSubject)) {
        return trip->name();
    }
    return QString();
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
        removeTripStats(dead);
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
        //Trips, Stations, Length and Depth cells as well as the Kind chip.
        emitRowDataChanged(indexOfNode(node), Kind, Depth, kKindRoles);
    });
    connect(node, &cwSurveyNode::sourceChanged, this, [this, node]() {
        //The Kind chip reads the source as well as the Name cell's badge.
        emitRowDataChanged(indexOfNode(node), Name, Kind, kSourceRoles);
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
        emitRowDataChanged(indexOfTrip(trip), Date, Date, {DateRole});
    });

    cwTripCalibration* calibration = trip->calibrations();
    connect(calibration, &cwTripCalibration::declinationChanged, this, [this, trip]() {
        emitRowDataChanged(indexOfTrip(trip), Decl, Decl, {DeclinationRole});
    });

    const auto emitAutoDeclinationChanged = [this, trip]() {
        emitRowDataChanged(indexOfTrip(trip), Decl, Decl, {AutoDeclinationRole});
    };
    connect(calibration, &cwTripCalibration::autoDeclinationChanged, this, emitAutoDeclinationChanged);
    connect(calibration, &cwTripCalibration::autoDeclinationAvailableChanged, this, emitAutoDeclinationChanged);

    auto stats = new cwTripStatsWatcher(trip, this);
    m_tripStats.insert(trip, stats);

    connect(stats, &cwTripStatsWatcher::lengthChanged, this, [this, trip]() {
        emitRowDataChanged(indexOfTrip(trip), Length, Length, {LengthRole});
    });
    connect(stats, &cwTripStatsWatcher::usedStationsChanged, this, [this, trip]() {
        emitRowDataChanged(indexOfTrip(trip), Stations, Stations,
                           {StationCountRole, UsedStationsRole});
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
        disconnectTrip(node->trip(i));
    }

    disconnect(node->length(), nullptr, this, nullptr);
    disconnect(node->depth(), nullptr, this, nullptr);
    disconnectObject(node);
}

/**
  \brief Unwires \a trip, its calibration and its statistics watcher
  */
void cwSurveyTreeModel::disconnectTrip(cwTrip* trip)
{
    if(trip == nullptr) { return; }

    disconnect(trip->calibrations(), nullptr, this, nullptr);
    removeTripStats(trip);
    disconnectObject(trip);
}

void cwSurveyTreeModel::disconnectObject(QObject* object)
{
    if(object == nullptr) { return; }
    m_connected.remove(object);
    disconnect(object, nullptr, this, nullptr);
}

void cwSurveyTreeModel::removeTripStats(QObject* object)
{
    cwTripStatsWatcher* stats = m_tripStats.take(object);
    if(stats == nullptr) { return; }

    stats->disconnect(this);
    stats->deleteLater();
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
        disconnectTrip(parentNode->trip(i));
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
        emitRowDataChanged(indexOfNode(current), Trips, Stations, kAggregateRoles);
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

int cwSurveyTreeModel::subtreeStationCount(const cwSurveyNode* node) const
{
    int count = 0;

    //A walk keeps a repaint free of the list allTrips() would build for every
    //visible row.
    node->walk([this, &count](const cwSurveyNode* current) {
        for(int i = 0; i < current->tripCount(); i++) {
            count += tripStationCount(current->trip(i));
        }
    });

    return count;
}

int cwSurveyTreeModel::tripStationCount(cwTrip* trip) const
{
    const cwTripStatsWatcher* stats = m_tripStats.value(trip);
    return stats != nullptr ? stats->stationCount() : 0;
}

QString cwSurveyTreeModel::tripUsedStations(cwTrip* trip) const
{
    const cwTripStatsWatcher* stats = m_tripStats.value(trip);
    //The ranges read as one line of text, the way the cave page's trip table
    //drew them.
    return stats != nullptr ? stats->usedStations().join(QStringLiteral(", "))
                            : QString();
}

double cwSurveyTreeModel::tripLength(cwTrip* trip) const
{
    const cwTripStatsWatcher* stats = m_tripStats.value(trip);
    return stats != nullptr ? stats->length() : 0.0;
}
