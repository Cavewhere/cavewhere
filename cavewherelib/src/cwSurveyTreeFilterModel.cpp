/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurveyTreeFilterModel.h"
#include "cwLength.h"
#include "cwSurveyTreeModel.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"
#include "cwUnits.h"

//Qt includes
#include <QDateTime>

//Std includes
#include <optional>

namespace {

    //! The unit every length in this file is compared in, so a cave measured in
    //! feet and a trip measured in meters order by the same number.
    constexpr cwUnits::LengthUnit kCompareUnit = cwUnits::Meters;

    //! True when \a index is a node row rather than a trip row.
    bool isNodeRow(const QModelIndex& index)
    {
        return index.data(cwSurveyTreeModel::RowTypeRole).toInt() == cwSurveyTreeModel::Node;
    }

    //! \a length in kCompareUnit, nothing when the row carries no length.
    std::optional<double> inCompareUnit(const cwLength* length)
    {
        if(length == nullptr) { return std::nullopt; }
        return length->convertTo(kCompareUnit).value;
    }

    //! The surveyed length of \a index's row in kCompareUnit: a node row's own
    //! cwLength, or a trip row's number read in its calibration's distance unit.
    std::optional<double> rowLength(const QModelIndex& index)
    {
        const QVariant length = index.data(cwSurveyTreeModel::LengthRole);
        if(isNodeRow(index)) {
            return inCompareUnit(length.value<cwLength*>());
        }

        auto trip = qobject_cast<cwTrip*>(index.data(cwSurveyTreeModel::ObjectRole).value<QObject*>());
        if(trip == nullptr) { return std::nullopt; }
        return cwUnits::convert(length.toDouble(),
                                trip->calibrations()->distanceUnit(),
                                kCompareUnit);
    }

    //! The depth of \a index's row in kCompareUnit. Only a node is solved for a
    //! depth, so a trip row has none.
    std::optional<double> rowDepth(const QModelIndex& index)
    {
        return inCompareUnit(index.data(cwSurveyTreeModel::DepthValueRole).value<cwLength*>());
    }

    //! The day \a index's row was surveyed. A node row holds trips surveyed on
    //! many days, so it carries none.
    std::optional<QDateTime> rowDate(const QModelIndex& index)
    {
        const QDateTime date = index.data(cwSurveyTreeModel::DateRole).toDateTime();
        if(!date.isValid()) { return std::nullopt; }
        return date;
    }

    //! The declination of \a index's row. A node row holds trips declinated
    //! many ways, so it carries none.
    std::optional<double> rowDeclination(const QModelIndex& index)
    {
        if(isNodeRow(index)) { return std::nullopt; }
        return index.data(cwSurveyTreeModel::DeclinationRole).toDouble();
    }

    //! Orders \a left ahead of \a right by the text at \a role, compared
    //! case-insensitively the way the filter beside it matches and the way
    //! CaveWhere reads survey names everywhere else.
    bool lessThanText(const QModelIndex& left, const QModelIndex& right, int role)
    {
        return QString::compare(left.data(role).toString(),
                                right.data(role).toString(),
                                Qt::CaseInsensitive) < 0;
    }

    //! Orders \a left ahead of \a right by the count at \a role, which every
    //! row carries.
    bool lessThanCount(const QModelIndex& left, const QModelIndex& right, int role)
    {
        return left.data(role).toInt() < right.data(role).toInt();
    }

    //! Orders \a left ahead of \a right, a row with no value ahead of every
    //! row that has one.
    template<typename T>
    bool lessThanValue(const std::optional<T>& left, const std::optional<T>& right)
    {
        if(!left.has_value()) { return right.has_value(); }
        if(!right.has_value()) { return false; }
        return left.value() < right.value();
    }
}

cwSurveyTreeFilterModel::cwSurveyTreeFilterModel(QObject* parent) :
    QSortFilterProxyModel(parent)
{
    //A match keeps its ancestors visible; a child that misses the filter stays
    //hidden even under a parent that matched, which is the settled rule: a row
    //is shown when it or a descendant matches. autoAcceptChildRows() would show
    //those children too, so it stays at its default of false.
    setRecursiveFilteringEnabled(true);

    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setFilterRole(cwSurveyTreeModel::NameRole);
    setFilterKeyColumn(cwSurveyTreeModel::Name);

    //Column -1 hands the rows back in the source model's order, which is the
    //order each node lists its children and trips in.
    sort(-1);
}

/**
  \brief Orders the rows by \a sortColumn, or by each node's own order at -1

  Siblings are ordered within their parent, which is what a QSortFilterProxyModel
  over a tree does by construction, so the tree's shape is untouched. Going back
  to -1 goes back to ascending as well: a descending sort by no column hands the
  rows back reversed, and a node's own order is one order, not two.
  */
void cwSurveyTreeFilterModel::setSortColumn(int sortColumn)
{
    if(this->sortColumn() == sortColumn) { return; }

    const bool orderMoves = sortColumn < 0 && sortOrder() != Qt::AscendingOrder;

    sort(sortColumn, orderMoves ? Qt::AscendingOrder : sortOrder());

    emit sortColumnChanged();
    if(orderMoves) {
        emit sortOrderChanged();
    }
}

/**
  \brief Puts the rows in \a sortOrder, leaving the column they are ordered by

  With no column picked there is nothing to order: the rows stand in each node's
  own order, which reads one way round.
  */
void cwSurveyTreeFilterModel::setSortOrder(Qt::SortOrder sortOrder)
{
    if(this->sortOrder() == sortOrder || sortColumn() < 0) { return; }

    sort(sortColumn(), sortOrder);

    emit sortOrderChanged();
}

/**
  \brief Orders \a left ahead of \a right by what their column means

  Every column is compared by the value behind it rather than by the text a cell
  draws: the counts as numbers, the lengths in one unit, the date as a date. A
  node row and a trip row under one parent are compared the same way, so the two
  kinds of row order together.
  */
bool cwSurveyTreeFilterModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    switch(left.column()) {
    case cwSurveyTreeModel::Name:
        return lessThanText(left, right, cwSurveyTreeModel::NameRole);
    case cwSurveyTreeModel::Kind:
        return lessThanText(left, right, cwSurveyTreeModel::KindLabelRole);
    case cwSurveyTreeModel::Trips:
        return lessThanCount(left, right, cwSurveyTreeModel::TripCountRole);
    case cwSurveyTreeModel::Stations:
        //The count, which every row has, rather than the names only a trip row
        //carries.
        return lessThanCount(left, right, cwSurveyTreeModel::StationCountRole);
    case cwSurveyTreeModel::Length:
        return lessThanValue(rowLength(left), rowLength(right));
    case cwSurveyTreeModel::Depth:
        return lessThanValue(rowDepth(left), rowDepth(right));
    case cwSurveyTreeModel::Date:
        return lessThanValue(rowDate(left), rowDate(right));
    case cwSurveyTreeModel::Decl:
        return lessThanValue(rowDeclination(left), rowDeclination(right));
    default:
        break;
    }

    return QSortFilterProxyModel::lessThan(left, right);
}

QString cwSurveyTreeFilterModel::filterText() const
{
    return m_filterText;
}

/**
  \brief Shows only the rows whose subtree holds \a filterText

  The stock fixed-string filter does the matching and its own invalidation, so
  there is no custom filterAcceptsRow() here and no beginFilterChange() pair to
  drive: those are for a filter parameter Qt does not know about.
  */
void cwSurveyTreeFilterModel::setFilterText(const QString& filterText)
{
    if(m_filterText == filterText) { return; }

    m_filterText = filterText;
    setFilterFixedString(m_filterText);

    emit filterTextChanged();
}
