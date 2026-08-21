/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwExternalSourceStatusModel.h"

//Qt includes
#include <QByteArray>
#include <QVariant>

//Std includes
#include <algorithm>

cwExternalSourceStatusModel::cwExternalSourceStatusModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int cwExternalSourceStatusModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_rows.size());
}

QVariant cwExternalSourceStatusModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return QVariant();
    }

    const Row& row = m_rows.at(index.row());
    switch (role) {
    case OwnerIdRole:
        return QVariant::fromValue(row.ownerId);
    case SourcePathRole:
        return row.sourcePath;
    case StatusRole:
        return QVariant::fromValue(row.status);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> cwExternalSourceStatusModel::roleNames() const
{
    return {
        { OwnerIdRole, QByteArrayLiteral("ownerId") },
        { SourcePathRole, QByteArrayLiteral("sourcePath") },
        { StatusRole, QByteArrayLiteral("status") }
    };
}

// A full reset rather than a diff: the sweep produces a handful of rows,
// and every surface reading them re-reads the whole set anyway.
void cwExternalSourceStatusModel::setRows(QVector<Row> rows)
{
    if (rows == m_rows) {
        return;
    }

    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();

    emit statusesChanged();
}

const cwExternalSourceStatusModel::Row*
cwExternalSourceStatusModel::findRow(const QUuid& ownerId) const
{
    const auto found = std::find_if(m_rows.cbegin(), m_rows.cend(),
                                    [&ownerId](const Row& row) {
        return row.ownerId == ownerId;
    });
    return found == m_rows.cend() ? nullptr : &(*found);
}

cwExternalSourceStatusModel::Status
cwExternalSourceStatusModel::statusFor(const QUuid& ownerId) const
{
    const Row* row = findRow(ownerId);
    return row == nullptr ? Status::NoBreadcrumb : row->status;
}

QString cwExternalSourceStatusModel::sourcePathFor(const QUuid& ownerId) const
{
    const Row* row = findRow(ownerId);
    return row == nullptr ? QString() : row->sourcePath;
}
