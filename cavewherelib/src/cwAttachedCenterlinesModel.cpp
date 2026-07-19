/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwAttachedCenterlinesModel.h"

//Std includes
#include <algorithm>

namespace {

QVector<QUuid> ownerIds(const QVector<cwAttachedCenterlinesModel::Row>& rows)
{
    QVector<QUuid> ids;
    ids.reserve(rows.size());
    for (const auto& row : rows) {
        ids.append(row.ownerId);
    }
    return ids;
}

// True when `sub` appears in `super` in order (classic subsequence walk).
bool isSubsequence(const QVector<QUuid>& sub, const QVector<QUuid>& super)
{
    int superIndex = 0;
    for (const QUuid& id : sub) {
        while (superIndex < super.size() && super.at(superIndex) != id) {
            ++superIndex;
        }
        if (superIndex == super.size()) {
            return false;
        }
        ++superIndex;
    }
    return true;
}

} // namespace

cwAttachedCenterlinesModel::cwAttachedCenterlinesModel(QObject* parent) :
    QAbstractListModel(parent)
{
}

int cwAttachedCenterlinesModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_rows.size();
}

QVariant cwAttachedCenterlinesModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return QVariant();
    }
    const Row& row = m_rows.at(index.row());
    switch (role) {
    case OwnerNameRole:
        return row.ownerName;
    case OwnerKindRole:
        return row.ownerKind;
    case EntryFileRole:
        return row.entryFile;
    case DepCountRole:
        return row.depCount;
    case WarningCountRole:
        return row.warningCount;
    case LastSolvedRole:
        return row.lastSolved;
    }
    return QVariant();
}

QHash<int, QByteArray> cwAttachedCenterlinesModel::roleNames() const
{
    return {
        { OwnerNameRole, QByteArrayLiteral("ownerName") },
        { OwnerKindRole, QByteArrayLiteral("ownerKind") },
        { EntryFileRole, QByteArrayLiteral("entryFile") },
        { DepCountRole, QByteArrayLiteral("depCount") },
        { WarningCountRole, QByteArrayLiteral("warningCount") },
        { LastSolvedRole, QByteArrayLiteral("lastSolved") }
    };
}

bool cwAttachedCenterlinesModel::rowDataEqual(const Row& left, const Row& right)
{
    return left.ownerId == right.ownerId
        && left.caveName == right.caveName
        && left.ownerName == right.ownerName
        && left.ownerKind == right.ownerKind
        && left.entryFile == right.entryFile
        && left.depCount == right.depCount
        && left.warningCount == right.warningCount
        && left.lastSolved == right.lastSolved;
}

void cwAttachedCenterlinesModel::setRows(QVector<Row> rows)
{
    // Rebuilds don't know about solve history — carry it across by owner.
    for (Row& row : rows) {
        const auto existing = std::find_if(m_rows.cbegin(), m_rows.cend(),
                                           [&row](const Row& candidate) {
            return candidate.ownerId == row.ownerId;
        });
        if (existing != m_rows.cend()) {
            row.lastSolved = existing->lastSolved;
        }
    }

    const QVector<QUuid> oldIds = ownerIds(m_rows);
    const QVector<QUuid> newIds = ownerIds(rows);

    if (newIds == oldIds) {
        // Same owners, same order — per-row dataChanged where content moved.
        syncRowData(rows);
        return;
    }

    if (isSubsequence(newIds, oldIds)) {
        // Pure removal(s): drop rows whose owner left, back to front so
        // indices stay valid, then refresh any surviving rows that changed.
        for (int i = m_rows.size() - 1; i >= 0; --i) {
            if (!newIds.contains(m_rows.at(i).ownerId)) {
                beginRemoveRows(QModelIndex(), i, i);
                m_rows.removeAt(i);
                endRemoveRows();
            }
        }
        syncRowData(rows);
        return;
    }

    if (isSubsequence(oldIds, newIds)) {
        // Pure insertion(s): walk the new list, inserting owners that
        // weren't present, then refresh survivors in place.
        for (int i = 0; i < rows.size(); ++i) {
            if (i >= m_rows.size() || m_rows.at(i).ownerId != rows.at(i).ownerId) {
                beginInsertRows(QModelIndex(), i, i);
                m_rows.insert(i, rows.at(i));
                endInsertRows();
            } else if (!rowDataEqual(m_rows.at(i), rows.at(i))) {
                m_rows[i] = rows.at(i);
                const QModelIndex changed = index(i, 0);
                emit dataChanged(changed, changed);
            }
        }
        return;
    }

    // Owners were reordered (or added and removed at once) — a rename can
    // resort the list. Fine-grained moves aren't worth modeling here.
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
}

void cwAttachedCenterlinesModel::syncRowData(const QVector<Row>& rows)
{
    for (int i = 0; i < rows.size(); ++i) {
        if (!rowDataEqual(m_rows.at(i), rows.at(i))) {
            m_rows[i] = rows.at(i);
            const QModelIndex changed = index(i, 0);
            emit dataChanged(changed, changed);
        }
    }
}

void cwAttachedCenterlinesModel::markSolved(const QDateTime& when)
{
    if (m_rows.isEmpty()) {
        return;
    }
    for (Row& row : m_rows) {
        row.lastSolved = when;
    }
    emit dataChanged(index(0, 0), index(m_rows.size() - 1, 0), { LastSolvedRole });
}
