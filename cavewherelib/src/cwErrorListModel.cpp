/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#include "cwErrorListModel.h"

cwErrorListModel::cwErrorListModel(QObject *parent)
    : QAbstractListModel(parent)
{

}

int cwErrorListModel::roleForName(const QByteArray &roleName) const
{
    const QHash<int, QByteArray> roles = roleNames();
    return roles.key(roleName, -1);  // Return -1 if the role name isn't found
}

int cwErrorListModel::count() const
{
    return m_errors.size();
}

cwError cwErrorListModel::at(int index) const
{
    if (index >= 0 && index < m_errors.size()) {
        return m_errors.at(index);
    }
    return cwError();  // Return a default constructed cwError if index is invalid
}

int cwErrorListModel::indexOf(const cwError &error) const
{
    return m_errors.indexOf(error);
}

// QVariant cwErrorListModel::get(int index) const
// {
//     return QVariant::fromValue(at(index));
// }

void cwErrorListModel::clear()
{
    if (m_errors.isEmpty()) {
        return;
    }

    beginRemoveRows(QModelIndex(), 0, m_errors.size() - 1);
    m_errors.clear();
    endRemoveRows();

    emit countChanged();
}

void cwErrorListModel::remove(int index)
{
    if (index < 0 || index >= m_errors.size()) {
        return;
    }

    beginRemoveRows(QModelIndex(), index, index);
    m_errors.removeAt(index);
    endRemoveRows();

    emit countChanged();
}

void cwErrorListModel::remove(const cwError &error)
{
    remove(indexOf(error));
}

void cwErrorListModel::insert(int index, const cwError &error)
{
    if (index < 0 || index > m_errors.size()) {
        return;
    }

    beginInsertRows(QModelIndex(), index, index);
    m_errors.insert(index, error);
    endInsertRows();

    emit countChanged();
}

void cwErrorListModel::insert(int index, const QList<cwError> &errors)
{
    if (errors.isEmpty() || index < 0 || index > m_errors.size()) {
        return;
    }

    beginInsertRows(QModelIndex(), index, index + errors.size() - 1);
    for (const cwError &error : errors) {
        m_errors.insert(index++, error);
    }
    endInsertRows();

    emit countChanged();
}


int cwErrorListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;  // When parent is valid, we are dealing with a child model, return 0
    }
    return m_errors.size();
}

QVariant cwErrorListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_errors.size()) {
        return QVariant();  // Return an invalid QVariant if the index is out of bounds
    }

    const cwError &error = m_errors.at(index.row());

    switch (role) {
    case static_cast<int>(ErrorRoles::SuppressedRole):
        return error.suppressed();
    case static_cast<int>(ErrorRoles::ErrorTypeIdRole):
        return error.errorTypeId();
    case static_cast<int>(ErrorRoles::MessageRole):
        return error.message();
    case static_cast<int>(ErrorRoles::ErrorTypeRole):
        return error.type();
    default:
        return QVariant();  // Return an invalid QVariant if the role isn't recognized
    }
}

// void cwErrorListModel::prepend(const QList<cwError> &errors)
// {
//     if (errors.isEmpty()) {
//         return;
//     }

//     beginInsertRows(QModelIndex(), 0, errors.size() - 1);
//     m_errors.prepend(errors);
//     endInsertRows();
// }

void cwErrorListModel::append(const cwError &error)
{
    beginInsertRows(QModelIndex(), m_errors.size(), m_errors.size());
    m_errors.append(error);
    endInsertRows();

    emit countChanged();
}

void cwErrorListModel::append(const QList<cwError> &errors)
{
    if (errors.isEmpty()) {
        return;
    }

    beginInsertRows(QModelIndex(), m_errors.size(), m_errors.size() + errors.size() - 1);
    m_errors.append(errors);
    endInsertRows();

    emit countChanged();
}

QHash<int, QByteArray> cwErrorListModel::roleNames() const
{
    return {
        { static_cast<int>(ErrorRoles::SuppressedRole), "suppressed" },
        { static_cast<int>(ErrorRoles::ErrorTypeIdRole), "errorTypeId" },
        { static_cast<int>(ErrorRoles::MessageRole), "message" },
        { static_cast<int>(ErrorRoles::ErrorTypeRole), "errorType" }
    };
}
