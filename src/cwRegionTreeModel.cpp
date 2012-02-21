//Our includes
#include "cwRegionTreeModel.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwGlobalIcons.h"

//Qt include
#include <QUrl>
#include <QDebug>

cwRegionTreeModel::cwRegionTreeModel(QObject *parent) :
    QAbstractItemModel(parent),
    Region(NULL)
{

    QHash<int, QByteArray> roles;
    roles[TypeRole] = "indexType";
    roles[NameRole] = "name";
    roles[DateRole] = "date"; //Only valid for trips
    roles[ObjectRole] = "object";
    roles[IconSourceRole] = "iconSource";
    setRoleNames(roles);

}

void cwRegionTreeModel::setCavingRegion(cwCavingRegion* region) {
   //Remove all the connections
    removeCaveConnections(0, rowCount() - 1);

    //Reset the model
    beginResetModel();
    Region = region;
    connect(Region, SIGNAL(beginInsertCaves(int,int)), SLOT(beginInsertCaves(int,int)));
    connect(Region, SIGNAL(insertedCaves(int,int)), SLOT(endInsertCaves(int,int)));
    connect(Region, SIGNAL(beginRemoveCaves(int,int)), SLOT(beginRemoveCaves(int,int)));
    connect(Region, SIGNAL(removedCaves(int,int)), SLOT(endRemoveCaves(int,int)));
    endResetModel();

    addCaveConnections(0, rowCount() - 1);
}


QModelIndex cwRegionTreeModel::index ( int row, int column, const QModelIndex & parent) const {
    if(column != 0) { return QModelIndex(); }
    if(Region == NULL) { return QModelIndex(); }
    if(row < 0) { return QModelIndex(); }

    if(!parent.isValid()) {
        //Try to get a cave
        if(row >= Region->caveCount()) {
            return QModelIndex();
        }
        return createIndex(row, column, Region->cave(row));
    }

    cwCave* parentCave = qobject_cast<cwCave*>((QObject*)parent.internalPointer());
    if(parentCave != NULL) {
        if(row >= parentCave->tripCount()) {
            return QModelIndex();
        }

        return createIndex(row, column, parentCave->trip(row));
    }

    return QModelIndex();
}

/**
  \brief Gets the cave index of the model

  If the cave doesn't exist in the model this return QModelIndex()
  */
QModelIndex cwRegionTreeModel::index (cwCave* cave) const {
    if(Region == NULL) { return QModelIndex(); }
    if(cave == NULL) { return QModelIndex(); }
    int caveIndex = Region->indexOf(cave);
    if(caveIndex < 0) { QModelIndex(); }
    return index(caveIndex, 0, QModelIndex());
}

/**
  \brief Gets the cave index of the model

  If the cave doesn't exist in the model this return QModelIndex()
  */
QModelIndex cwRegionTreeModel::index (cwTrip* trip) const {
    cwCave* parentCave = trip->parentCave();
    if(parentCave == NULL) { return QModelIndex(); }
    int tripIndex = parentCave->indexOf(trip);

    QModelIndex parentIndex = index(parentCave);
    if(!parentIndex.isValid()) { return QModelIndex(); }

    if(tripIndex < 0) { return QModelIndex(); }

    return index(tripIndex, 0, parentIndex);
}


QModelIndex cwRegionTreeModel::parent ( const QModelIndex & index ) const {
    cwCave* cave = qobject_cast<cwCave*>((QObject*)index.internalPointer());
    if(cave != NULL) {
        return QModelIndex(); //Caves don't have parents
    }

    cwTrip* trip = qobject_cast<cwTrip*>((QObject*)index.internalPointer());
    if(trip != NULL) {
        cwCave* parentCave = qobject_cast<cwCave*>(((QObject*)trip)->parent());
        int row = Region->indexOf(parentCave);
        Q_ASSERT(row >= 0); //This should always return with a valid index

        return createIndex(row, 0, parentCave);
    }

    return QModelIndex();
}

int cwRegionTreeModel::rowCount ( const QModelIndex & parent ) const {
    if(Region == NULL) { return 0; }

    if(!parent.isValid()) {
        return Region->caveCount();
    }

    cwCave* parentCave = qobject_cast<cwCave*>((QObject*)parent.internalPointer());
    if(parentCave != NULL) {
        return parentCave->tripCount();
    }

    return 0;
}

int cwRegionTreeModel::columnCount ( const QModelIndex & /*parent*/) const {
    if(Region == NULL) { return 0; }

    return 1; //Always one column
}

QVariant cwRegionTreeModel::data ( const QModelIndex & index, int role ) const {
    if(!index.isValid()) {     //Root item selected
        switch(role) {
        case TypeRole:
            return RegionType;
        }
        return QVariant();
    }

    cwCave* currentCave = qobject_cast<cwCave*>((QObject*)index.internalPointer());
    if(currentCave != NULL) {
        switch(role) {
        case NameRole:
        case Qt::DisplayRole:
            return QVariant(currentCave->name());
        case Qt::DecorationRole: {
            QPixmap icon;
            if(!QPixmapCache::find(cwGlobalIcons::Cave, &icon)) {
                icon = QPixmap(cwGlobalIcons::CaveFilename);
                cwGlobalIcons::Cave = QPixmapCache::insert(icon);
            }
            return QVariant(icon);
        }
        case ObjectRole:
            return QVariant::fromValue<QObject*>(static_cast<QObject*>(currentCave));
        case TypeRole:
            return CaveType;
        case DateRole:
            return QDate(); //Caves dont have a date
        case IconSourceRole:
            return QUrl::fromLocalFile(":icons/cave.png");
        default:
            return QVariant();
        }
    }

    cwTrip* currentTrip = qobject_cast<cwTrip*>((QObject*)index.internalPointer());
    if(currentTrip != NULL) {
        switch(role) {
        case NameRole:
        case Qt::DisplayRole:
            return QVariant(currentTrip->name());
        case Qt::DecorationRole: {
            QPixmap icon;
            if(!QPixmapCache::find(cwGlobalIcons::Trip, &icon)) {
                icon = QPixmap(cwGlobalIcons::TripFilename);
                cwGlobalIcons::Trip = QPixmapCache::insert(icon);
            }
            return QVariant(icon);
        }
        case ObjectRole:
            return QVariant::fromValue<QObject*>(static_cast<QObject*>(currentTrip));
        case TypeRole:
            return TripType;
        case DateRole:
            return QVariant(currentTrip->date());
        case IconSourceRole:
            return QUrl::fromLocalFile(":icons/trip.png");
        default:
            return QVariant();
        }
    }



    return QVariant();
}

/**
  \brief Sets the data for the model
  */
bool cwRegionTreeModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if(!index.isValid()) { return false; }

    cwCave* currentCave = qobject_cast<cwCave*>((QObject*)index.internalPointer());
    if(currentCave != NULL) {
        switch(role) {
        case NameRole:
        case Qt::DisplayRole:
            currentCave->setName(value.toString());
            emit dataChanged(index, index);
            return true;
        default:
            return false;
        }
    }

    cwTrip* currentTrip = qobject_cast<cwTrip*>((QObject*)index.internalPointer());
    if(currentTrip != NULL) {
        switch(role) {
        case NameRole:
        case Qt::DisplayRole:
            currentTrip->setName(value.toString());
            emit dataChanged(index, index);
            return true;
        case DateRole:
            currentTrip->setDate(value.toDate());
            emit dataChanged(index, index);
            return true;
        default:
            return false;
        }
    }
    return false;
}

/**
  \brief This removes a item from the model

  If the item is a cave then it removes the cave from the index
  If the item is a trip then it removes the trip from the parent cave
  */
void cwRegionTreeModel::removeIndex(QModelIndex item) {
    if(!item.isValid()) { return; } //Can't remove an invalid

    QModelIndex parentItem = parent(item);

    if(item.data(TypeRole) == CaveType) {
        //item is a cave remove it from the region
        Region->removeCave(item.row());
    } else if(item.data(TypeRole) == TripType) {
        //item is a trip remove it from the parent cave
        cwCave* parentCave = cave(parentItem);
        Q_ASSERT(parentCave != NULL);

        parentCave->removeTrip(item.row());
    }
}

Qt::ItemFlags cwRegionTreeModel::flags ( const QModelIndex & /*index*/) {
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

/**
  \brief Gets the trip at index

  If index isn't a trip, then this returns null
  */
cwTrip* cwRegionTreeModel::trip(const QModelIndex& index) const {
    QVariant tripVariant = data(index, ObjectRole);
    cwTrip* trip = qobject_cast<cwTrip*>(tripVariant.value<QObject*>());
    return trip;
}

/**
  \brief Gets the cave at inde
  */
cwCave* cwRegionTreeModel::cave(const QModelIndex& index) const {
    QVariant caveVariant = data(index, ObjectRole);
    cwCave* cave = qobject_cast<cwCave*>(caveVariant.value<QObject*>());
    return cave;
}

/**
  \brief New caves inserted
  */
void cwRegionTreeModel::endInsertCaves(int beginIndex, int endIndex) {
    endInsertRows();

    addCaveConnections(beginIndex, endIndex);
}

/**
  \brief Caled when a cave is about to be
  */
void cwRegionTreeModel::beginRemoveCaves(int beginIndex, int endIndex) {
    removeCaveConnections(beginIndex, endIndex);

    beginRemoveRows(QModelIndex(), beginIndex, endIndex);
}

/**
  \brief The cave has begun inserting trips into itself.

  This alerts the views of this model, that it's changing
  */
void cwRegionTreeModel::beginInsertTrip(int beginIndex, int endIndex) {
    cwCave* cave = qobject_cast<cwCave*>(sender());

    QModelIndex caveIndex = index(cave);
    Q_ASSERT(caveIndex.isValid());

    beginInsertRows(caveIndex, beginIndex, endIndex);
}

/**
  \brief The cave has inserted trips into itself

  This update the trip connections
  */
void cwRegionTreeModel::endInsertTrip(int beginIndex, int endIndex) {
    cwCave* cave = qobject_cast<cwCave*>(sender());
    Q_ASSERT(cave != NULL);

    endInsertRows();

    addTripConnections(cave, beginIndex, endIndex);
}

/**
  \brief The cave has begun to removed trips from itself

  This alerts the views of this model, to update themselfs
  */
void cwRegionTreeModel::beginRemoveTrip(int beginIndex, int endIndex) {
    cwCave* cave = qobject_cast<cwCave*>(sender());
    Q_ASSERT(cave != NULL);

    QModelIndex caveIndex = index(cave);
    Q_ASSERT(caveIndex.isValid());

    removeTripConnections(cave, beginIndex, endIndex);

    beginRemoveRows(caveIndex, beginIndex, endIndex);
}

/**
  \brief The cave has removed the trips from itself

  This alerts the views to update themselves
  */
void cwRegionTreeModel::endRemoveTrip(int /*beginIndex*/, int /*endIndex*/) {
    cwCave* cave = qobject_cast<cwCave*>(sender());
    Q_ASSERT(cave != NULL);

    endRemoveRows();
}


/**
  \brief calls dataChanged() when the cave data has changed
  */
 void cwRegionTreeModel::caveDataChanged() {
     Q_ASSERT(qobject_cast<cwCave*>(sender()) != NULL);
     cwCave* cave = static_cast<cwCave*>(sender());
     QModelIndex modelIndex = index(cave);
     emit dataChanged(modelIndex, modelIndex);
 }

 /**
   \brief calls dataChanged() when the trip data has changed
   */
 void cwRegionTreeModel::tripDataChanged() {
     Q_ASSERT(qobject_cast<cwTrip*>(sender()) != NULL);
     cwTrip* trip = static_cast<cwTrip*>(sender());
     QModelIndex modelIndexOfTrip = index(trip);
     emit dataChanged(modelIndexOfTrip, modelIndexOfTrip);
 }

/**
  \brief Adds all the connection for a cave
  */
void cwRegionTreeModel::addCaveConnections(int beginIndex, int endIndex) {

    for(int i = beginIndex; i <= endIndex; i++) {
        cwCave* cave = Region->cave(i);
        connect(cave, SIGNAL(nameChanged(QString)), SLOT(caveDataChanged()));

        connect(cave, SIGNAL(beginInsertTrips(int,int)), SLOT(beginInsertTrip(int,int)));
        connect(cave, SIGNAL(insertedTrips(int,int)), SLOT(endInsertTrip(int,int)));
        connect(cave, SIGNAL(beginRemoveTrips(int,int)), SLOT(beginRemoveTrip(int,int)));
        connect(cave, SIGNAL(removedTrips(int,int)), SLOT(endRemoveTrip(int,int)));

        addTripConnections(cave, 0, cave->tripCount() - 1);
    }
}

/**
  \brief Removes all the connection for a cave
  */
void cwRegionTreeModel::removeCaveConnections(int beginIndex, int endIndex) {
    for(int i = beginIndex; i <= endIndex; i++) {
        cwCave* cave = Region->cave(i);
        disconnect(cave, 0, this, 0); //disconnect signals and slots to this object
    }
}

/**
  \brief Adds connection for the trips between beginIndex and endIndex
  */
void cwRegionTreeModel::addTripConnections(cwCave* parentCave, int beginIndex, int endIndex) {
    for(int i = beginIndex; i <= endIndex; i++) {
        cwTrip* currentTrip = parentCave->trip(i);
        connect(currentTrip, SIGNAL(nameChanged(QString)), SLOT(tripDataChanged()));
        connect(currentTrip, SIGNAL(dateChanged(QDate)), SLOT(tripDataChanged()));
    }
}

/**
  \brief Removes the connections for a trips between beginIndex and endIndex
  */
void cwRegionTreeModel::removeTripConnections(cwCave* parentCave, int beginIndex, int endIndex) {

    for(int i = beginIndex; i <= endIndex; i++) {
        cwTrip* trip = parentCave->trip(i);
        disconnect(trip, 0, this, 0); //disconnect signals and slots to this object
    }

}



