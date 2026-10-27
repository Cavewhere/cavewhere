/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwFixStationTreeFilterModel.h"
#include "cwFixStationModel.h"
#include "cwFixStationTreeModel.h"

//Std includes
#include <cmath>
#include <tuple>

namespace {

bool isFixRow(const QModelIndex& index)
{
    return index.data(cwFixStationTreeModel::RowKindRole).toInt() == cwFixStationTreeModel::FixRow;
}

struct Coordinate
{
    double easting;
    double northing;
    double elevation;

    bool isFinite() const
    {
        return std::isfinite(easting) && std::isfinite(northing) && std::isfinite(elevation);
    }

    bool operator<(const Coordinate& other) const
    {
        return std::tie(easting, northing, elevation)
               < std::tie(other.easting, other.northing, other.elevation);
    }
};

//! Compares \a left and \a right ignoring case, with each run of digits read
//! as one number, so "st2" comes before "st10". QCollator's numeric mode does
//! the same only outside the C locale, which the application sets as default.
int naturalCompare(QStringView left, QStringView right)
{
    const auto digitRun = [](QStringView text, qsizetype start) {
        qsizetype end = start;
        while (end < text.size() && text.at(end).isDigit()) {
            ++end;
        }
        return end;
    };
    const auto withoutLeadingZeros = [](QStringView digits) {
        qsizetype first = 0;
        while (first < digits.size() - 1 && digits.at(first) == u'0') {
            ++first;
        }
        return digits.mid(first);
    };

    qsizetype leftAt = 0;
    qsizetype rightAt = 0;
    while (leftAt < left.size() && rightAt < right.size()) {
        if (left.at(leftAt).isDigit() && right.at(rightAt).isDigit()) {
            const qsizetype leftEnd = digitRun(left, leftAt);
            const qsizetype rightEnd = digitRun(right, rightAt);
            const QStringView leftNumber = withoutLeadingZeros(left.mid(leftAt, leftEnd - leftAt));
            const QStringView rightNumber = withoutLeadingZeros(right.mid(rightAt, rightEnd - rightAt));
            if (leftNumber.size() != rightNumber.size()) {
                return leftNumber.size() < rightNumber.size() ? -1 : 1;
            }
            const int numberOrder = leftNumber.compare(rightNumber);
            if (numberOrder != 0) {
                return numberOrder;
            }
            leftAt = leftEnd;
            rightAt = rightEnd;
        } else {
            const int charOrder = left.mid(leftAt, 1).compare(right.mid(rightAt, 1), Qt::CaseInsensitive);
            if (charOrder != 0) {
                return charOrder;
            }
            ++leftAt;
            ++rightAt;
        }
    }
    const qsizetype leftRest = left.size() - leftAt;
    const qsizetype rightRest = right.size() - rightAt;
    if (leftRest == rightRest) {
        return 0;
    }
    return leftRest < rightRest ? -1 : 1;
}

bool naturalLessThan(const QVariant& left, const QVariant& right)
{
    return naturalCompare(left.toString(), right.toString()) < 0;
}

Coordinate coordinateOf(const QModelIndex& index)
{
    return {index.data(cwFixStationModel::EastingRole).toDouble(),
            index.data(cwFixStationModel::NorthingRole).toDouble(),
            index.data(cwFixStationModel::ElevationRole).toDouble()};
}

// Rows with every component come first by (easting, northing, elevation);
// rows without them (a file fix whose text does not parse, or a lat/long one)
// follow, ordered by that text.
bool coordinateLessThan(const QModelIndex& left, const QModelIndex& right)
{
    const Coordinate leftCoordinate = coordinateOf(left);
    const Coordinate rightCoordinate = coordinateOf(right);
    const bool leftIsFinite = leftCoordinate.isFinite();
    const bool rightIsFinite = rightCoordinate.isFinite();
    if (leftIsFinite && rightIsFinite) {
        return leftCoordinate < rightCoordinate;
    }
    if (leftIsFinite != rightIsFinite) {
        return leftIsFinite;
    }
    return naturalLessThan(left.data(cwFixStationModel::CoordinateTextRole),
                           right.data(cwFixStationModel::CoordinateTextRole));
}

} // namespace

cwFixStationTreeFilterModel::cwFixStationTreeFilterModel(QObject* parent) :
    cwSortFilterProxyModel(parent)
{
    setRecursiveFilteringEnabled(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    // cwSortFilterProxyModel's setFilterRole(QByteArray) hides the role-id one.
    QSortFilterProxyModel::setFilterRole(cwFixStationModel::StationNameRole);

    const auto recount = [this]() {
        if (!m_isRefiltering) {
            recountVisibleFixRows();
        }
    };
    connect(this, &QAbstractItemModel::rowsInserted, this, recount);
    connect(this, &QAbstractItemModel::rowsRemoved, this, recount);
    connect(this, &QAbstractItemModel::modelReset, this, recount);
    connect(this, &QAbstractProxyModel::sourceModelChanged, this, [this]() {
        connectSourceFixRowCount();
        recountVisibleFixRows();
    });
}

void cwFixStationTreeFilterModel::setFilterText(const QString& filterText)
{
    if (m_filterText == filterText) {
        return;
    }
    m_filterText = filterText;
    m_isRefiltering = true;
    setFilterFixedString(m_filterText);
    m_isRefiltering = false;
    emit filterTextChanged();
    recountVisibleFixRows();
}

void cwFixStationTreeFilterModel::announceVisibleFixCounts()
{
    announceVisibleFixCountsUnder(QModelIndex());
}

QVariant cwFixStationTreeFilterModel::data(const QModelIndex& index, int role) const
{
    if (role == VisibleFixCountRole) {
        if (!index.isValid() || isFixRow(index)) {
            return 0;
        }
        const int rows = rowCount(index);
        int count = 0;
        for (int row = 0; row < rows; ++row) {
            if (isFixRow(this->index(row, 0, index))) {
                ++count;
            }
        }
        return count;
    }
    return cwSortFilterProxyModel::data(index, role);
}

QHash<int, QByteArray> cwFixStationTreeFilterModel::roleNames() const
{
    QHash<int, QByteArray> names = cwSortFilterProxyModel::roleNames();
    names.insert(VisibleFixCountRole, QByteArrayLiteral("visibleFixCount"));
    return names;
}

// Unfiltered, every fix row shows, so the source's own count answers without
// a walk; the source announces that count after its rows have moved.
void cwFixStationTreeFilterModel::connectSourceFixRowCount()
{
    disconnect(m_sourceFixRowCountConnection);
    if (auto* treeModel = qobject_cast<cwFixStationTreeModel*>(sourceModel())) {
        m_sourceFixRowCountConnection = connect(treeModel, &cwFixStationTreeModel::fixRowCountChanged,
                                                this, [this]() {
                                                    if (m_filterText.isEmpty()) {
                                                        recountVisibleFixRows();
                                                    }
                                                });
    }
}

void cwFixStationTreeFilterModel::recountVisibleFixRows()
{
    const auto* treeModel = qobject_cast<const cwFixStationTreeModel*>(sourceModel());
    const int count = m_filterText.isEmpty() && treeModel != nullptr ? treeModel->fixRowCount()
                                                                     : countFixRowsUnder(QModelIndex());
    if (m_visibleFixRowCount == count) {
        return;
    }
    m_visibleFixRowCount = count;
    emit visibleFixRowCountChanged();
}

int cwFixStationTreeFilterModel::countFixRowsUnder(const QModelIndex& parent) const
{
    const int rows = rowCount(parent);
    int count = 0;
    for (int row = 0; row < rows; ++row) {
        const QModelIndex child = index(row, 0, parent);
        count += isFixRow(child) ? 1 : countFixRowsUnder(child);
    }
    return count;
}

void cwFixStationTreeFilterModel::announceVisibleFixCountsUnder(const QModelIndex& parent)
{
    const int rows = rowCount(parent);
    for (int row = 0; row < rows; ++row) {
        const QModelIndex child = index(row, 0, parent);
        if (!isFixRow(child)) {
            emit dataChanged(child, child, {VisibleFixCountRole});
            announceVisibleFixCountsUnder(child);
        }
    }
}

bool cwFixStationTreeFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (!sourceParent.isValid()) {
        return true;
    }
    // Skips cwSortFilterProxyModel's override, which reads filterRole by name.
    return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}

bool cwFixStationTreeFilterModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    if (isFixRow(left) && isFixRow(right)) {
        const int role = sortRole();
        if (role == cwFixStationModel::EastingRole) {
            return coordinateLessThan(left, right);
        }
        return naturalLessThan(left.data(role), right.data(role));
    }

    // A node row follows its parent's fixes in source order. A descending
    // sort asks lessThan(right, left), so the row test turns around with it.
    return sortOrder() == Qt::AscendingOrder ? left.row() < right.row()
                                             : left.row() > right.row();
}
