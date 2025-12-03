/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwLinkBarModel.h"
#include "cwPageSelectionModel.h"

cwLinkBarModel::cwLinkBarModel()
{

}

cwLinkBarModel::~cwLinkBarModel()
{

}

/**
 * @brief cwLinkBarModel::roleNames
 * @return
 */
QHash<int, QByteArray> cwLinkBarModel::roleNames() const
{
    QHash<int, QByteArray> names;
    names.insert(FullPath, "fullPathRole");
    names.insert(Name, "nameRole");
    return names;
}

/**
 * @brief cwLinkBarModel::rowCount
 * @param parent
 * @return
 */
int cwLinkBarModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return Names.size();
}

/**
 * @brief cwLinkBarModel::data
 * @param index
 * @param role
 * @return
 */
QVariant cwLinkBarModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid()) { return QVariant(); }
    switch(role) {
    case Name:
        return Names.at(index.row());
    case FullPath:
        return fullPath(index.row());
    default:
        return QVariant();
    }
}


/**
* @brief cwLinkBarModel::setAddress
* @param address - The address, this should be the current address of cwPageSelectionModel
*/
void cwLinkBarModel::setAddress(QString address) {
    if(Address != address) {
        Address = address;
        emit addressChanged();
        updateNames();
    }
}

/**
 * @brief cwLinkBarModel::updateNames
 *
 * This will update the model with the name from the address
 */
void cwLinkBarModel::updateNames()
{
    QStringList names = Address.split(cwPageSelectionModel::seperator());

    for(int i = 0; i < names.size() && i < Names.size(); i++) {
        if(names.at(i) != Names.at(i)) {
            for(int j = i; j < names.size() && j < Names.size(); j++) {
                //Update all the names
                Names[i] = names.at(i);
            }

            dataChanged(index(i), index(Names.size() - 1));
            break;
        }
    }

    //Remove extra names
    if(Names.size() > names.size()) {
        beginRemoveRows(QModelIndex(), names.size(), Names.size() - 1);
        Names.erase(Names.begin() + names.size(), Names.end());
        endRemoveRows();
    }

    //Insert extra names
    if(Names.size() < names.size()) {
        if(!Names.isEmpty()) {
            names.erase(names.begin(), names.begin() + Names.size());
        }
        beginInsertRows(QModelIndex(), Names.size(), Names.size() + names.size() - 1);
        Names.append(names);
        endInsertRows();
    }

#ifdef QT_DEBUG
    QStringList names2 = Address.split(cwPageSelectionModel::seperator());
    Q_ASSERT(Names.size() == names2.size());
    for(int i = 0; i < names2.size(); i++) {
        Q_ASSERT(Names.at(i) == names2.at(i));
    }
#endif /* QT_DEBUG */
}

/**
 * @brief cwLinkBarModel::fullPath
 * @param index
 * @return The full path at index
 */
QString cwLinkBarModel::fullPath(int index) const
{
    QString fullPath;
    for(int i = 0; i <= index; i++) {
        fullPath += Names.at(i);
        if(i != index) {
            fullPath += cwPageSelectionModel::seperator();
        }
    }
    return fullPath;
}

