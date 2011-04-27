//Our includes
#include "cwRegionTreeModel.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwGlobalIcons.h"

//Qt include
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
    setRoleNames(roles);

}

void cwRegionTreeModel::setCavingRegion(cwCavingRegion* region) {
    beginResetModel();
    Region = region;
    connect(Region, SIGNAL(insertedCaves(int,int)), SLOT(insertCaves(int,int)));
    endResetModel();
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
    if(!index.isValid()) { return QVariant(); }

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
            return Cave;
        case DateRole:
            return QDate(); //Caves dont have a date
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
            return Trip;
        case DateRole:
            return QVariant(currentTrip->date());
        default:
            return QVariant();
        }
    }

    return QVariant();
}

/**
  \brief New caves inserted
  */
void cwRegionTreeModel::insertCaves(int beginIndex, int endIndex) {
    beginInsertRows(QModelIndex(), beginIndex, endIndex);
    endInsertRows();
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

