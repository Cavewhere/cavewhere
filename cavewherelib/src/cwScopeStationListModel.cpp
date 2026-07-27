/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwScopeStationListModel.h"
#include "cwStation.h"
#include "cwTrip.h"

//Std includes
#include <algorithm>

namespace {

//! Natural-order compare: walk both strings run-by-run, comparing digit runs by
//! numeric value (so "a2" sorts before "a10") and everything else by case-folded
//! character. Station tails are typically <stationPrefix><number> and may nest
//! ("sidepassage.b1"); the trailing number must ascend numerically, not lexically.
bool naturalLess(const QString& a, const QString& b)
{
    int i = 0;
    int j = 0;
    const int n = a.size();
    const int m = b.size();

    while (i < n && j < m) {
        const QChar ca = a.at(i);
        const QChar cb = b.at(j);

        if (ca.isDigit() && cb.isDigit()) {
            //Span each digit run, then compare by value (skip leading zeros so
            //"007" and "7" compare equal in magnitude, longer run breaking ties).
            int ai = i;
            int bj = j;
            while (ai < n && a.at(ai).isDigit()) { ++ai; }
            while (bj < m && b.at(bj).isDigit()) { ++bj; }

            int as = i;
            int bs = j;
            while (as < ai - 1 && a.at(as) == QLatin1Char('0')) { ++as; }
            while (bs < bj - 1 && b.at(bs) == QLatin1Char('0')) { ++bs; }

            const int aLen = ai - as;
            const int bLen = bj - bs;
            if (aLen != bLen) {
                return aLen < bLen;
            }
            for (int k = 0; k < aLen; ++k) {
                const QChar da = a.at(as + k);
                const QChar db = b.at(bs + k);
                if (da != db) {
                    return da < db;
                }
            }
            //Equal magnitude: shorter original run (fewer leading zeros) first.
            if ((ai - i) != (bj - j)) {
                return (ai - i) < (bj - j);
            }
            i = ai;
            j = bj;
        } else {
            const QChar la = ca.toCaseFolded();
            const QChar lb = cb.toCaseFolded();
            if (la != lb) {
                return la < lb;
            }
            ++i;
            ++j;
        }
    }

    return (n - i) < (m - j);
}

} // namespace

cwScopeStationListModel::cwScopeStationListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int cwScopeStationListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_rows.size();
}

QVariant cwScopeStationListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return QVariant();
    }

    const Row& row = m_rows.at(index.row());
    switch (role) {
    case StationNameRole:
        return row.handle.tail();
    case StationHandleRole:
        return QVariant::fromValue(row.handle);
    case PositionRole:
        return row.position;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> cwScopeStationListModel::roleNames() const
{
    return {
        { StationNameRole, "stationName" },
        { StationHandleRole, "stationHandle" },
        { PositionRole, "stationPosition" }
    };
}

void cwScopeStationListModel::setTrip(cwTrip* trip)
{
    if (m_trip == trip) {
        return;
    }

    if (m_trip != nullptr) {
        disconnect(m_trip, &cwTrip::solvedStationsChanged,
                   this, &cwScopeStationListModel::rebuildRows);
    }

    m_trip = trip;

    if (m_trip != nullptr) {
        connect(m_trip, &cwTrip::solvedStationsChanged,
                this, &cwScopeStationListModel::rebuildRows);
    }

    emit tripChanged();
    rebuildRows();
}

bool cwScopeStationListModel::containsStation(const QString& displayName) const
{
    const QString key = cwStation::canonicalKey(displayName);
    return std::any_of(m_rows.cbegin(), m_rows.cend(), [&key](const Row& row) {
        return cwStation::canonicalKey(row.handle.tail()) == key;
    });
}

QStringList cwScopeStationListModel::matchingStations(const QString& prefix) const
{
    const QString key = cwStation::canonicalKey(prefix);
    QStringList matches;
    for (const Row& row : m_rows) {
        if (cwStation::canonicalKey(row.handle.tail()).startsWith(key)) {
            matches.append(row.handle.tail());
        }
    }
    return matches;
}

void cwScopeStationListModel::rebuildRows()
{
    beginResetModel();
    m_rows.clear();

    if (m_trip != nullptr) {
        const QList<QPair<QString, QVector3D>> solved = m_trip->solvedStations();
        m_rows.reserve(solved.size());
        for (const QPair<QString, QVector3D>& station : solved) {
            m_rows.append({ m_trip->stationHandle(station.first), station.second });
        }
    }

    std::stable_sort(m_rows.begin(), m_rows.end(),
                     [](const Row& left, const Row& right) {
        return naturalLess(left.handle.tail(), right.handle.tail());
    });

    endResetModel();
}
