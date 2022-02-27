#ifndef CWUNIQUEVALUEFILTERMODEL_H
#define CWUNIQUEVALUEFILTERMODEL_H

//Qt includes
#include <QAbstractProxyModel>
#include <QDebug>

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

public:
    QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    void setSourceModel(QAbstractItemModel *sourceModel);

    QModelIndex mapToSource(const QModelIndex &proxyIndex) const;
    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const;

signals:
    void uniqueRoleChanged();

private:
    void buildIndex();

    void insertIntoIndex(const QModelIndex& sourceIndex);

    struct Row {
        QPersistentModelIndex index;
        QVariant value;
    };

    QVector<Row> mUniqueIndex;
    int mUniqueRole = -1; //!<
    std::function<bool (const QVariant& left, const QVariant& right)> mLessThan;
    bool isConnected = false;

    QVector<Row>::const_iterator findProxyIndex(const QModelIndex& sourceIndex) const;
    QVector<Row>::iterator findProxyIndex(const QModelIndex& sourceIndex);
    QVector<Row>::const_iterator findProxyIndex(const QVariant& value) const;
    QVector<Row>::iterator findProxyIndex(const QVariant& value);

    inline QVariant uniqueValue(const QModelIndex& index) const {
        return index.data(mUniqueRole);
    }

    inline int iteratorToRow(QVector<Row>::const_iterator iter) const{
        return std::distance(mUniqueIndex.begin(), iter);
    }

    template<typename R, typename C, typename F>
    R findRunAction(const QModelIndex& sourceIndex, C predict, F actionFunc) {
        auto iter = findProxyIndex(sourceIndex);
        if(predict(iter)) {
            return actionFunc(iter);
        }
        return R();
    }

    template<typename R, typename C, typename F>
    R findRunAction(const QModelIndex& sourceIndex, C predict, F actionFunc) const {
        return const_cast<cwUniqueValueFilterModel*>(this)->findRunAction<R>(sourceIndex, predict, actionFunc);
    }


    template<typename T>
    bool contains(T iter, const QModelIndex& sourceIndex) const {
        return iter != mUniqueIndex.end() && iter->value == uniqueValue(sourceIndex);
    }

    inline bool isValid() {
        return sourceModel() && uniqueRole() >= 0 && mLessThan;
    }

    void connectSourceModel();
    void disconnectSourceModel();

    inline Row toRow(const QModelIndex& sourceIndex) {
        return {sourceIndex, uniqueValue(sourceIndex)};
    }

};

inline int cwUniqueValueFilterModel::uniqueRole() const {
    return mUniqueRole;
}

#endif // CWUNIQUEVALUEFILTERMODEL_H
