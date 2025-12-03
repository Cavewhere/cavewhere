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

    void setCheckForInvalidRows(bool shouldCheck) { mCheckForInvalidRows = shouldCheck; }

    // //For debugging
    // void dump() const {
    //     qDebug() << "cwKeywordFilterModel::dump:" << this << mAcceptedSourceIndexes;
    // }

private:
    //Sorted list of indexes based on object, the list is added or remove via accept() / remove() functions
    QVector<QPersistentModelIndex> mAcceptedSourceIndexes;

    QMetaObject::Connection mDataChanged;
    QMetaObject::Connection mRowsAboutToBeRemoved;

    QVector<QPersistentModelIndex>::iterator findAcceptedObject(QObject* object);
    QVector<QPersistentModelIndex>::const_iterator findAcceptedObject(QObject* object) const;

    /**
     * This check might slow preformance down, but prevents invalid rows from being
     * held in the filter.
     */
    bool mCheckForInvalidRows = false;

    void removeInvalidRows();

    static QObject* toObject(const QModelIndex& sourceIndex)
    {
        return sourceIndex.data(cwKeywordItemModel::ObjectRole).value<QObject*>();
    }

    template<typename R, typename C, typename F>
    R findElementRunAction(const QModelIndex& sourceIndex, C objectCompareFunc, F actionFunc) {
        if(!sourceIndex.isValid()) {
            return R();
        }

        const auto acceptedObject = toObject(sourceIndex);

        if(acceptedObject) {
            const auto iter = findAcceptedObject(acceptedObject);

            auto toIndex = [this](const auto iter)->QModelIndex {
                if(iter == mAcceptedSourceIndexes.end()) {
                    return QModelIndex();
                } else {
                    return *iter;
                }
            };

            if(objectCompareFunc(toObject(toIndex(iter)), acceptedObject))
            {
                int row = std::distance(mAcceptedSourceIndexes.begin(), iter);
                return actionFunc(row, iter);
            }
        }

        return R();
    }

    template<typename R, typename C, typename F>
    R findElementRunAction(const QModelIndex& sourceIndex, C objectCompareFunc, F actionFunc) const {
        return const_cast<cwKeywordFilterModel*>(this)->findElementRunAction<R,C,F>(sourceIndex, objectCompareFunc, actionFunc);
    }


};

#endif // CWKEYWORDFILTERMODEL_H
