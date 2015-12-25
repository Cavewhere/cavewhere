/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwSplayModel.h"

cwSplayModel::cwSplayModel(QObject *parent) : QAbstractItemModel(parent)
{

}

/**
 * @brief cwSplayModel::splays
 * @param name
 * @return Returns all the splays with station name
 */
QList<cwShot> cwSplayModel::splays(const QString& stationName) const
{
    QString stationNameUpper = stationName.toUpper();
    if(StationsIndexLookup.contains(stationNameUpper)) {
        auto node = StationsIndexLookup.value(stationNameUpper);
        return node->Splays;
    }

    return QList<cwShot>();
}

/**
 * @brief cwSplayModel::appendSplay
 * @param name - Station that a shot will be appended to
 * @param shot - The shot that will be appended to name
 */
void cwSplayModel::appendSplay(const QString& stationName, const cwShot &shot)
{
    QString stationNameUpper = stationName.toUpper();
    if(StationsIndexLookup.contains(stationNameUpper)) {
        //Add to existing
        auto node = StationsIndexLookup.value(stationNameUpper);
        QList<cwShot>& shots = node->Splays;
        QModelIndex parentIndex = nodeToIndex(node);

        beginInsertRows(parentIndex, shots.size(), shots.size());
        shots.append(shot);
        endInsertRows();

    } else {
        //Add a completely new station
        int newIndex = Nodes.size();

        Node* node = new Node(stationNameUpper);

        beginInsertRows(QModelIndex(), newIndex, newIndex);
        Nodes.append(node);
        StationsIndexLookup.insert(stationNameUpper, node);
        endInsertRows();

        //Recusive call, to append the child shot
        appendSplay(stationNameUpper, shot);;
    }
}

/**
 * @brief cwSplayModel::removeSplay
 * Removes the shot at index with station name
 */
void cwSplayModel::removeSplay(const QString& stationName, int index)
{
    QString stationNameUpper = stationName.toUpper();
    if(StationsIndexLookup.contains(stationNameUpper)) {
        auto node = StationsIndexLookup.value(stationNameUpper);
        QList<cwShot>& shots = node->Splays;
        QModelIndex parentIndex = nodeToIndex(node);

        beginRemoveRows(parentIndex, index, index);
        shots.removeAt(index);
        endRemoveRows();

        if(shots.isEmpty()) {
            clear(stationNameUpper);
        }
    }
}

/**
 * @brief cwSplayModel::clear
 *
 * Clears all the stations and shots from the model
 */
void cwSplayModel::clear()
{
    beginResetModel();
    StationsIndexLookup.clear();

    foreach(Node* node, Nodes) {
        delete node;
    }
    Nodes.clear();

    endResetModel();
}

/**
 * @brief cwSplayModel::clear
 * @param name - Clears all the splay shots at station name
 */
void cwSplayModel::clear(const QString& stationName)
{
    QString stationNameUpper = stationName.toUpper();
    if(StationsIndexLookup.contains(stationNameUpper)) {
        auto node = StationsIndexLookup.value(stationNameUpper);
        QModelIndex nodeIndex = nodeToIndex(node);

        //Remove the station and the empty shot list
        beginRemoveRows(QModelIndex(), nodeIndex.row(), nodeIndex.row());

        //Remove the station
        delete Nodes.at(nodeIndex.row());
        Nodes.removeAt(nodeIndex.row());

        endRemoveRows();
    }
}

/**
 * @brief cwSplayModel::rowCount
 * @param parent
 * @return Returns to total count of all the stations and
 */
int cwSplayModel::rowCount(const QModelIndex &parent) const
{
    if(parent == QModelIndex()) {
        return Nodes.size();
    } else {
        Node* node = Nodes.at(parent.row());
        return node->Splays.size();
    }
}

/**
 * @brief cwSplayModel::columnCount
 * @param parent
 * @return The column count
 */
int cwSplayModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 0;
}

/**
 * @brief cwSplayModel::data
 * @param index
 * @param role
 * @return Returns the data of the splay shot at index
 */
QVariant cwSplayModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid()) {
        return QVariant();
    }

    if(index.parent() == QModelIndex()) {
        switch(role) {
        case cwSurveyChunk::StationNameRole:
            return Nodes.at(index.row())->Name;
        default:
            return QVariant();
        }
    } else {
        Q_ASSERT(index.internalPointer() >= 0);
        Node* node = indexToNode(index);
        const QList<cwShot>& shots = node->Splays;
        const cwShot& shot = shots[index.row()];
        switch(role) {
        case cwSurveyChunk::ShotDistanceRole:
            return shot.distance();
        case cwSurveyChunk::ShotCompassRole:
            return shot.compass();
        case cwSurveyChunk::ShotBackCompassRole:
            return shot.backCompass();
        case cwSurveyChunk::ShotClinoRole:
            return shot.clino();
        case cwSurveyChunk::ShotBackClinoRole:
            return shot.backClino();
        default:
            return QVariant();
        }
    }
}

/**
 * @brief cwSplayModel::setData
 * @param index
 * @param value
 * @param role
 * @return
 *
 * Set the data for the splay model
 */
bool cwSplayModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if(!index.isValid()) {
        return false;
    }

    QVector<int> rolesChanged;
    rolesChanged.reserve(1);
    rolesChanged.append(role);

    if(index.parent() == QModelIndex()) {
        switch(role) {
        case cwSurveyChunk::StationNameRole: {
            QString stationName = value.toString().toUpper();
            Node* node = Nodes.at(index.row());
            StationsIndexLookup.remove(stationName);
            node->Name = stationName;
            StationsIndexLookup.insert(stationName, node);
            emit dataChanged(index, index, rolesChanged);
            break;
        }
        default:
            return false;
        }
    } else {
        Q_ASSERT(index.internalPointer() >= 0);
        Node* node = indexToNode(index);
        QList<cwShot>& shots = node->Splays;
        cwShot& shot = shots[index.row()];
        switch(role) {
        case cwSurveyChunk::ShotDistanceRole:
            shot.setDistance(value.toString());
            emit dataChanged(index, index, rolesChanged);
            break;
        case cwSurveyChunk::ShotCompassRole:
            shot.setCompass(value.toString());
            emit dataChanged(index, index, rolesChanged);
            break;
        case cwSurveyChunk::ShotBackCompassRole:
            shot.setBackCompass(value.toString());
            emit dataChanged(index, index, rolesChanged);
            break;
        case cwSurveyChunk::ShotClinoRole:
            shot.setClino(value.toString());
            emit dataChanged(index, index, rolesChanged);
            break;
        case cwSurveyChunk::ShotBackClinoRole:
            shot.setBackClino(value.toString());
            emit dataChanged(index, index, rolesChanged);
            break;
        default:
            return false;
        }
    }
    return true;
}

/**
 * @brief cwSplayModel::roleNames
 * @return Returns the role names of the roleNames
 */
QHash<int, QByteArray> cwSplayModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(cwSurveyChunk::StationNameRole, "nameRole");
    roles.insert(cwSurveyChunk::ShotDistanceRole, "distanceRole");
    roles.insert(cwSurveyChunk::ShotCompassRole, "compassRole");
    roles.insert(cwSurveyChunk::ShotBackCompassRole, "backCompassRole");
    roles.insert(cwSurveyChunk::ShotClinoRole, "clinoRole");
    roles.insert(cwSurveyChunk::ShotBackClinoRole, "backClinoRole");
    return roles;
}

/**
 * @brief cwSplayModel::index
 * @param row
 * @param column
 * @param parent
 * @return
 */
QModelIndex cwSplayModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(column);
    if(row < 0) {
        return QModelIndex();
    }

    if(parent.isValid()) {
        Node* node = Nodes.at(parent.row());
        return createIndex(row, 0, node);
    } else if(parent == QModelIndex() && row < Nodes.size()) {
        return createIndex(row, 0, nullptr);
    }
    return QModelIndex();
}

/**
 * @brief cwSplayModel::parent
 * @param child
 * @return The parent of the child. See QAbstractItemModel docs for more details
 */
QModelIndex cwSplayModel::parent(const QModelIndex &child) const
{
    if(child.internalId() == 0) {
        //Child is station, doesn't have a parent
        return QModelIndex();
    } else {
        //Child is a splay shot, parent is a station
        return nodeToIndex(indexToNode(child));
    }
}

///**
// * @brief cwSplayModel::appendSplay
// * @param name
// * @param shots
// */
//void cwSplayModel::appendNewSplays(QString stationName, const QList<cwShot> &shots)
//{
//    Q_ASSERT(!StationsIndexLookup.contains(stationName));

//    //Add a completely new station
//    int newIndex = Stations.size();

//    Stations.append(stationName);
//    StationsIndexLookup.insert(stationName, newIndex);

//    Q_ASSERT(!Splays.contains(newIndex));
//    Splays.insert(newIndex, shots);
//}

/**
 * @brief cwSplayModel::nodeToIndex
 * @param node
 * @return
 */
QModelIndex cwSplayModel::nodeToIndex(cwSplayModel::Node *node) const
{
    return index(Nodes.indexOf(node));
}

cwSplayModel::Node *cwSplayModel::indexToNode(const QModelIndex &index) const
{
    return static_cast<Node*>(index.internalPointer());
}

