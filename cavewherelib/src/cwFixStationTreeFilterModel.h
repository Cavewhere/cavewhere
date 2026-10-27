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
    Q_PROPERTY(int visibleFixRowCount READ visibleFixRowCount NOTIFY visibleFixRowCountChanged)

public:
    //! Numbered clear of cwFixStationTreeModel's Qt::UserRole + 500 block.
    enum Roles {
        //! The fix rows the filter keeps directly under a node or trip row;
        //! zero on a fix row.
        VisibleFixCountRole = Qt::UserRole + 600
    };
    Q_ENUM(Roles)

    explicit cwFixStationTreeFilterModel(QObject* parent = nullptr);

    QString filterText() const { return m_filterText; }
    void setFilterText(const QString& filterText);

    //! The number of fix rows the filter keeps in the whole tree.
    int visibleFixRowCount() const { return m_visibleFixRowCount; }

    //! Announces VisibleFixCountRole on every node and trip row. A TreeView
    //! that reuses its delegates applies a dataChanged sent while rows come
    //! and go to the delegate the row left, so the view calls this once it
    //! has laid the new rows out.
    Q_INVOKABLE void announceVisibleFixCounts();

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void filterTextChanged();
    void visibleFixRowCountChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    QString m_filterText;
    int m_visibleFixRowCount = 0;
    //! Set while a filter change refilters, so its row signals recount once.
    bool m_isRefiltering = false;
    QMetaObject::Connection m_sourceFixRowCountConnection;

    void connectSourceFixRowCount();
    void recountVisibleFixRows();
    int countFixRowsUnder(const QModelIndex& parent) const;
    void announceVisibleFixCountsUnder(const QModelIndex& parent);
};

#endif // CWFIXSTATIONTREEFILTERMODEL_H
