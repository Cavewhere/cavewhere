//Our includes
#include "cwStationModel.h"

//Qt includes
#include <QDebug>

cwStationModel::cwStationModel(QObject *parent) :
    QAbstractListModel(parent)
{

    Stations.push_back(new cwStation("B1", 1, 2, 3, 4));
    Stations.push_back(new cwStation("B2", 4, 5, 6, 7));
    Stations.push_back(new cwStation("B3", 7, 8, 9, 10));
    Stations.push_back(new cwStation("B4", 11, 12, 13, 14));

    QHash<int, QByteArray> roles;
    roles[StationName] = "name";
    roles[Left] = "leftDistance";
    roles[Right] = "rightDistance";
    roles[Up] = "upDistance";
    roles[Down] = "downDistance";
    setRoleNames(roles);

    //Populate the Role lookup hash
    foreach(int role, roles.keys()) {
        RoleLookup[roles[role]] = role;
    }
}

int cwStationModel::rowCount(const QModelIndex & /*parent*/) const {
    return Stations.size();
}

QVariant cwStationModel::data(const QModelIndex & index, int role) const {
    if(index.row() < 0 || index.row() >= Stations.size()) { return QVariant(); }

    cwStation* station = Stations.at(index.row());

    switch(role) {
    case StationName:
        return station->GetName();
    case Left:
        return station->GetLeft();
    case Right:
        return station->GetRight();
    case Up:
        return station->GetUp();
    case Down:
        return station->GetDown();
    }

    return QVariant();
}

bool cwStationModel::setData (int row, const QString& string, const QVariant& value) {
    return setData(index(row), value, RoleLookup[string] );
}

bool cwStationModel::setData ( const QModelIndex & index, const QVariant & value, int role) {
    if(index.row() < 0 || index.row() >= Stations.size()) { return false; }

    qDebug() << "I'm changing row:" << index << " Value: " << value << "Role:" << role;

    cwStation* station = Stations.at(index.row());

    bool okay;
    float distance;
    switch(role) {
    case StationName:
        station->SetName(value.toString());
        return true;
    case Left:
        distance = value.toFloat(&okay);
        if(okay) {
            station->SetLeft(distance);
        }
        return okay;
    case Right:
        distance = value.toFloat(&okay);
        if(okay) {
            station->SetRight(distance);
        }
        return okay;
    case Up:
        distance = value.toFloat(&okay);
        if(okay) {
            station->SetUp(distance);
        }
        return okay;
    case Down:
        distance = value.toFloat(&okay);
        if(okay) {
            station->SetDown(distance);
        }
        return okay;
    }

    return false;
}

Qt::ItemFlags cwStationModel::flags ( const QModelIndex & /*index*/ ) const {
    return Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;
}
