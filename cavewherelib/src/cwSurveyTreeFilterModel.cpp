/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurveyTreeFilterModel.h"
#include "cwSurveyTreeModel.h"

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
