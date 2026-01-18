/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTABLESTATICCOLUMNMODEL_H
#define CWTABLESTATICCOLUMNMODEL_H

//Our includes
#include "cwTableStaticColumn.h"

//Qt includes
#include <QAbstractListModel>
#include <QQmlListProperty>
#include <QQmlEngine>

class cwTableStaticColumnModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(TableStaticColumnModel)
    Q_CLASSINFO("DefaultProperty", "columns")

    Q_PROPERTY(QQmlListProperty<cwTableStaticColumn> columns READ columns)
    Q_PROPERTY(int totalWidth READ totalWidth NOTIFY totalWidthChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        TextRole = Qt::UserRole + 1,
        ColumnWidthRole,
        SortRole
    };
    Q_ENUM(Roles)

    explicit cwTableStaticColumnModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QHash<int, QByteArray> roleNames() const override;

    QQmlListProperty<cwTableStaticColumn> columns();
    int count() const;
    int totalWidth() const;

signals:
    void countChanged();
    void totalWidthChanged();

private:
    static void appendColumn(QQmlListProperty<cwTableStaticColumn>* list, cwTableStaticColumn* column);
    static qsizetype columnCount(QQmlListProperty<cwTableStaticColumn>* list);
    static cwTableStaticColumn* columnAt(QQmlListProperty<cwTableStaticColumn>* list, qsizetype index);
    static void clearColumns(QQmlListProperty<cwTableStaticColumn>* list);

    void insertColumn(int index, cwTableStaticColumn* column);
    void clearAllColumns();
    void connectColumn(cwTableStaticColumn* column);
    void updateTotalWidth();
    int columnIndex(const cwTableStaticColumn* column) const;

    QList<cwTableStaticColumn*> m_columns;
    int m_totalWidth = 0;
};

#endif // CWTABLESTATICCOLUMNMODEL_H
