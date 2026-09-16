/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEYTREEFILTERMODEL_H
#define CWSURVEYTREEFILTERMODEL_H

//Our includes
#include "cwGlobals.h"

//Qt includes
#include <QQmlEngine>
#include <QSortFilterProxyModel>
#include <QString>

/**
 * The name filter and the column sort over cwSurveyTreeModel: a row survives
 * when its own name, or some descendant's name, holds filterText() as a
 * case-insensitive substring, and the rows that survive are ordered by
 * sortColumn(). Rows keep the order their node lists them in until a column is
 * picked, and sortColumn() == -1 puts them back in it.
 */
class CAVEWHERE_LIB_EXPORT cwSurveyTreeFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SurveyTreeFilterModel)

    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

    //! The column the rows are ordered by: a cwSurveyTreeModel::Column, or -1
    //! for the order each node lists its children and trips in.
    Q_PROPERTY(int sortColumn READ sortColumn WRITE setSortColumn NOTIFY sortColumnChanged)

    //! Which end of sortColumn()'s values comes first, ascending while no
    //! column is picked. A row with no value for the column — a node with no
    //! date, a trip with no depth — sorts ahead of the rows that have one while
    //! ascending.
    Q_PROPERTY(Qt::SortOrder sortOrder READ sortOrder WRITE setSortOrder NOTIFY sortOrderChanged)

public:
    explicit cwSurveyTreeFilterModel(QObject* parent = nullptr);

    QString filterText() const;
    void setFilterText(const QString& filterText);

    void setSortColumn(int sortColumn);
    void setSortOrder(Qt::SortOrder sortOrder);

signals:
    void filterTextChanged();
    void sortColumnChanged();
    void sortOrderChanged();

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    QString m_filterText;
};

#endif // CWSURVEYTREEFILTERMODEL_H
