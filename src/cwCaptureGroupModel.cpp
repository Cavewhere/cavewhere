/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwCaptureGroupModel.h"
#include "cwCaptureGroup.h"
#include "cwViewportCapture.h"

cwCaptureGroupModel::cwCaptureGroupModel(QObject *parent) :
    QAbstractItemModel(parent)
{
}

/**
 * @brief cwCaptureGroupModel::createGroup
 *
 * This creates the capture group. Once a group is created, catpures can be added
 * to it by calling addCapture()
 */
void cwCaptureGroupModel::addGroup()
{
    cwCaptureGroup* group = new cwCaptureGroup(this);
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    Groups.append(group);
    endInsertRows();
}

/**
 * @brief cwCaptureGroupModel::addCapture
 * @param parentGroup
 * @param capture
 *
 * Adds a capture to a parentGroup.
 */
void cwCaptureGroupModel::addCapture(QModelIndex parentGroup, cwViewportCapture *capture)
{
    if(!parentGroup.isValid()) { return; }
    if(capture == nullptr) { return; }

    int groupSize = rowCount(parentGroup);
    int parentIndex = parentGroup.row();

    qDebug() << "Group size: " << groupSize;

    cwCaptureGroup*  group = Groups.at(parentIndex);
    if(group->contains(capture)) {
        return;
    }

    beginInsertRows(parentGroup, groupSize, groupSize);
    group->addCapture(capture);
    endInsertRows();
}

/**
 * @brief cwCaptureGroupModel::rowCount
 * @param parent
 * @return Returns the number of rows based on the parent.
 *
 * If parent is QModelIndex() then this returns number of groups
 * If parent is a group, this will return number of captures in the group
 */
int cwCaptureGroupModel::rowCount(const QModelIndex &parent) const
{
    if(parent.isValid()) {
        int parentIndex = parent.row();
        cwCaptureGroup*  group = Groups.at(parentIndex);
        return group->numberOfCaptures();
    }
    return Groups.size();
}

/**
 * @brief cwCaptureGroupModel::data
 * @param index
 * @param role
 * @return
 */
QVariant cwCaptureGroupModel::data(const QModelIndex &index, int role) const
{
    qDebug() << "Data:" << index << index.isValid() << role;

    if(!index.isValid()) {
        return QVariant();
    }

    if(!index.parent().isValid()) {
        return QVariant();
    }

    int parentIndex = index.parent().row();
    cwViewportCapture* capture = Groups.at(parentIndex)->capture(index.row());

    switch(role) {
    case CaptureNameRole:
        return capture->name();
    case CaptureObjectRole:
        return QVariant::fromValue(capture);
    }
    return QVariant();
}

/**
 * @brief cwCaptureGroupModel::index
 * @param row
 * @param column
 * @param parent
 * @return
 */
QModelIndex cwCaptureGroupModel::index(int row, int column, const QModelIndex &parent) const
{
    qDebug() << "Index:" << row << column << parent;

    Q_UNUSED(column);
    if(!parent.isValid()) {
        if(row >= 0 && row < Groups.size()) {
            return createIndex(row, 0, (quintptr)-1);
        }
        return QModelIndex();
    }

    int parentIndex = parent.row();
    cwCaptureGroup* group = Groups.at(parentIndex);
    if(row >= 0 && row < group->numberOfCaptures()) {
        return createIndex(row, 0, parentIndex);
    }
    return QModelIndex();
}

/**
 * @brief cwCaptureGroupModel::parent
 * @param child
 * @return
 */
QModelIndex cwCaptureGroupModel::parent(const QModelIndex &child) const
{
    if(!child.isValid()) {
        return QModelIndex();
    }

    if(child.internalId() == (quintptr)-1) {
        return QModelIndex();
    }
    return index(child.internalId(), 0, QModelIndex());
}

/**
 * @brief cwCaptureGroupModel::roleNames
 * @return
 */
QHash<int, QByteArray> cwCaptureGroupModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[CaptureNameRole] = "captureNameRole";
    roles[CaptureObjectRole] = "captureObjectRole";
    return roles;
}

