#ifndef CWUNIQUEVALUEFILTERMODEL_H
#define CWUNIQUEVALUEFILTERMODEL_H

//Qt includes
#include <QtCore/qabstractproxymodel.h>
#include <QPersistentModelIndex>

//Std includes
#include <functional>
#include <QVector>

//Our includes
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwUniqueValueFilterModel : public QAbstractProxyModel
{
    Q_OBJECT

    Q_PROPERTY(int uniqueRole READ uniqueRole WRITE setUniqueRole NOTIFY uniqueRoleChanged)

public:
    explicit cwUniqueValueFilterModel(QObject *parent = nullptr);

    int uniqueRole() const;
    void setUniqueRole(int uniqueRole);

    inline void setLessThan(std::function<bool (const QVariant& left, const QVariant& right)> lessThan);

    bool contains(const QVariant& key) const;

    void printRows() {
        qDebug() << "cwUniqueValueFilterModel:";
        for(int i = 0; i < mUniqueIndex.size(); i++) {
            qDebug() << "\trow:" << i << mUniqueIndex.at(i).index << uniqueValue(mUniqueIndex.at(i).index) << mUniqueIndex.at(i).value;
        }
    }

public:
    QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    void setSourceModel(QAbstractItemModel *sourceModel) override;

    QModelIndex mapToSource(const QModelIndex &proxyIndex) const override;
    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const override;

signals:
    void uniqueRoleChanged();

private:
    void buildIndex();
    void rebuildRows();

    struct Row {
        QPersistentModelIndex index;
        QVariant value;
    };

    // Rows are in insertion order, one per unique value; mSortedRows holds
    // their row numbers ordered by mLessThan on the value, for lookup.
    QVector<Row> mUniqueIndex;
    QVector<int> mSortedRows;
    int mUniqueRole = -1; //!<
    std::function<bool (const QVariant& left, const QVariant& right)> mLessThan;
    bool isConnected = false;

    QVector<int>::const_iterator lowerBound(const QVariant& value) const;
    int findRow(const QVariant& value) const;
    void appendRow(const Row& row);
    void eraseRow(int row);

    void insertSourceRows(int first, int last);
    void releaseRow(int row, const QModelIndex& sourceIndex, int skipFirst, int skipLast);
    // Duplicate source rows are hidden behind their value's representative, so they emit nothing.
    void forwardDataChanged(int first, int last, const QList<int>& roles);

    inline QVariant uniqueValue(const QModelIndex& index) const {
        return index.data(mUniqueRole);
    }

    inline bool isValid() {
        return sourceModel() && uniqueRole() >= 0 && mLessThan;
    }

    void connectSourceModel();
    void disconnectSourceModel();

};

inline int cwUniqueValueFilterModel::uniqueRole() const {
    return mUniqueRole;
}

inline void cwUniqueValueFilterModel::setLessThan(std::function<bool(const QVariant &, const QVariant &)> lessThan)
{
    mLessThan = std::move(lessThan);
    buildIndex();
}

#endif // CWUNIQUEVALUEFILTERMODEL_H
