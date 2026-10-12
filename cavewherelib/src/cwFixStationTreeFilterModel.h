/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWFIXSTATIONTREEFILTERMODEL_H
#define CWFIXSTATIONTREEFILTERMODEL_H

//Our includes
#include "cwGlobals.h"
#include "cwSortFilterProxyModel.h"

//Qt includes
#include <QQmlEngine>
#include <QString>

/**
 * The Fix Stations page's sort and filter over cwFixStationTreeModel.
 *
 * The sort role picks the column: StationNameRole, InputCSRole, or EastingRole
 * for the coordinate. Sorting orders the fix rows under each node and keeps
 * the node rows after them in their own order, in both directions.
 *
 * The filter keeps the fixes whose station name holds filterText, ignoring
 * case, and the node rows above them. The top row, the page's own node,
 * always stays.
 */
class CAVEWHERE_LIB_EXPORT cwFixStationTreeFilterModel : public cwSortFilterProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(FixStationTreeFilterModel)

    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

public:
    explicit cwFixStationTreeFilterModel(QObject* parent = nullptr);

    QString filterText() const { return m_filterText; }
    void setFilterText(const QString& filterText);

signals:
    void filterTextChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    QString m_filterText;
};

#endif // CWFIXSTATIONTREEFILTERMODEL_H
