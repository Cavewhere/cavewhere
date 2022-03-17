#ifndef CWKEYWORDGROUPBYKEYMODEL_H
#define CWKEYWORDGROUPBYKEYMODEL_H

//Our include
#include "cwKeywordItemModel.h"
#include "cwGlobals.h"
#include "cwKeyword.h"
#include "cwEntityAndKeywords.h"
#include "cwKeywordFilterModel.h"
class cwKeywordModel;
class cwEntityKeywordsModel;
class cwKeywordFilterModel;

//Qt includes
#include <QList>
#include <QAbstractListModel>
#include <QFuture>

//Std includes
#include <memory>

class CAVEWHERE_LIB_EXPORT cwKeywordGroupByKeyModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(QAbstractItemModel* sourceModel READ sourceModel WRITE setSourceModel NOTIFY sourceChanged)
    Q_PROPERTY(cwKeywordFilterModel* acceptedModel READ acceptedModel CONSTANT)
    Q_PROPERTY(QString key READ key WRITE setKey NOTIFY keyChanged) //Last key we should filter by
    Q_PROPERTY(bool acceptByDefault READ acceptByDefault WRITE setAcceptedByDefault NOTIFY acceptByDefaultChanged)

public:
    enum Role {
        ObjectsRole,
        ObjectCountRole,
        ValueRole,
        AcceptedRole
    };

    Q_ENUM(Role)

    cwKeywordGroupByKeyModel(QObject* parent = nullptr);
    ~cwKeywordGroupByKeyModel();

    QAbstractItemModel* sourceModel() const;
    void setSourceModel(QAbstractItemModel* source);

    cwKeywordFilterModel* acceptedModel() const;

    QString key() const;
    void setKey(QString lastKey);

    bool acceptByDefault() const;
    void setAcceptedByDefault(bool acceptByDefault);

    void invert();

    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);

    QHash<int, QByteArray> roleNames() const;

    static QString otherCategory();
    QModelIndex otherIndex() const;

    static void setVisiblityProperty(QObject* object, bool accepted);

signals:
    void sourceChanged();
    void invertedChanged();
    void keyChanged();
    void modelFutureChanged();
    void acceptByDefaultChanged();

private:
    class Row {
    public:
        Row() {}
        Row(const QString& value, const QVector<QPersistentModelIndex>& indexes, bool accepted) :
            value(value),
            indexes(indexes),
            accepted(accepted)
        {}

        QString value;
        QVector<QPersistentModelIndex> indexes;
        bool accepted = false;

        bool operator<(const Row& other) {
            return value < other.value;
        }
    };


    class ModelData {
    public:
        QVector<Row> Rows;
        Row OtherRow = defaultOther();

        int otherRowIndex() const {
            return size() - 1;
        }

        int size() const {
            return Rows.size() + 1;
        }

        const Row& row(int index) const {
            if(index < Rows.size()) {
                return Rows.at(index);
            }
            return OtherRow;
        }

        Row& row(int index) {
            if(index < Rows.size()) {
                return Rows[index];
            }
            return OtherRow;
        }

        void clear() {
            Rows.clear();
            OtherRow = defaultOther();
        }

    private:
        inline Row defaultOther() {
            return Row(cwKeywordGroupByKeyModel::otherCategory(), {}, true);
        }
    };

    class Filter {

    public:
        QString key;
        cwKeywordGroupByKeyModel* model;

        std::function<void (int)> beginInsertFunction;
        std::function<void ()> endInsertFunction;
        std::function<void (int, QVector<int>)> dataChangedFunction;
        std::function<void (int)> beginRemoveFuction;
        std::function<void ()> endRemoveFunction;

        void filterEntity(const QModelIndex &sourceIndex);
        void removeEntityFromAllRows(const QModelIndex &sourceIndex);
        QSet<QString> entityValues(const QModelIndex &sourceIndex) const;

    private:
        void addEntity(const QString& value, const QModelIndex &sourceIndex);
        void removeEntity(const QString& value, const QModelIndex &sourceIndex);
        bool removeEntityFromRow(QVector<Row>::iterator iter, const QModelIndex &sourceIndex);
        bool removeEntityFromRow(int i, Row& row, const QModelIndex &sourceIndex);
        void addEntityToOther(const QModelIndex &sourceIndex);

        void beginInsert(const QVector<Row>::iterator& iter) const;
        void endInsert() const;
        void beginRemove(const QVector<Row>::iterator& iter) const;
        void endRemove() const;
        void dataChanged(const QVector<Row>::iterator& iter) const;
        void dataChanged(int index) const;

        static bool lessThan(const Row& row, const QString& value);
    };

    QAbstractItemModel* mSourceModel = nullptr; //!<
    cwKeywordFilterModel* mAcceptedModel; //!<

    bool mInverted; //!<
    QString mKey; //!<

    bool mAcceptedByDefault = true; //!<

    ModelData mData;

    Filter mFilter;

    void updateAllRows();

    Filter createDefaultFilter();

    template<typename DataChangeFunc>
    void setAccepted(const QModelIndex& rowIndex, bool accepted, DataChangeFunc func)
    {
        auto& row = mData.row(rowIndex.row());
        if(accepted != row.accepted) {
            row.accepted = accepted;

            for(auto index : row.indexes) {
                setAcceptIndex(index, accepted);
            }

            //Only enable if func isn't nullptr
            if constexpr (!std::is_same<decltype (func), std::nullptr_t>::value) {
                func();
            }
        }
    }

    void setAcceptIndex(const QModelIndex& index, bool accepted);
};

inline QAbstractItemModel* cwKeywordGroupByKeyModel::sourceModel() const {
    return mSourceModel;
}

inline cwKeywordFilterModel* cwKeywordGroupByKeyModel::acceptedModel() const {
    return mAcceptedModel;
}

/**
*
*/
inline QString cwKeywordGroupByKeyModel::key() const {
    return mKey;
}

inline bool cwKeywordGroupByKeyModel::acceptByDefault() const {
    return mAcceptedByDefault;
}



#endif // CWKEYWORDENTITYFILTERMODEL_H
