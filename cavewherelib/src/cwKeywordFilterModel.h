#ifndef CWKEYWORDFILTERMODEL_H
#define CWKEYWORDFILTERMODEL_H

#include <QtCore/qabstractproxymodel.h>
#include <QObject>
#include <QDebug>

//Our includes
#include "cwKeywordItemModel.h"
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwKeywordFilterModel : public QAbstractProxyModel
{
    Q_OBJECT
public:
    explicit cwKeywordFilterModel(QObject *parent = nullptr);

public:
    QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent) const;

    void setSourceModel(QAbstractItemModel *sourceModel);
    QModelIndex mapToSource(const QModelIndex &proxyIndex) const;
    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const;

    //Filter by objects
    void insert(const QModelIndex& sourceIndex);
    void remove(const QModelIndex& sourceIndex);
    void clear();

private:
    struct Row {
        QObject* object = nullptr;
        QPersistentModelIndex sourceIndex;
    };

    // Sorted by QObject* for fast lookup
    QVector<Row> mRows;

    QMetaObject::Connection mDataChanged;
    QMetaObject::Connection mRowsAboutToBeRemoved;

    QVector<Row>::iterator lowerBound(QObject* object);
    QVector<Row>::const_iterator lowerBound(QObject* object) const;
    void removeByObject(QObject* object);
    void removeBySourceIndex(const QModelIndex& sourceIndex);
    void disconnectSource();

    static QObject* toObject(const QModelIndex& sourceIndex)
    {
        return sourceIndex.data(cwKeywordItemModel::ObjectRole).value<QObject*>();
    }
};

#endif // CWKEYWORDFILTERMODEL_H
