#ifndef CWKEYWORDENTITYFILTER_H
#define CWKEYWORDENTITYFILTER_H

//Our include
#include "cwKeywordItemModel.h"
#include "cwGlobals.h"
#include "cwKeyword.h"
#include "cwEntityAndKeywords.h"
class cwKeywordModel;

//Qt includes
#include <Qt3DCore/QEntity>
#include <QList>
#include <QAbstractListModel>
#include <QFuture>

//Std includes
#include <memory>

class CAVEWHERE_LIB_EXPORT cwKeywordItemFilter : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(QVector<cwEntityAndKeywords> entities READ entities WRITE setEntities NOTIFY entitiesChanged)
    Q_PROPERTY(QVector<cwEntityAndKeywords> acceptedEntites READ acceptedEntites NOTIFY acceptedEntitesChanged)
//    Q_PROPERTY(QFuture<void> modelFuture READ modelFuture NOTIFY modelFutureChanged)

    Q_PROPERTY(QString key READ key WRITE setKey NOTIFY keyChanged) //Last key we should filter by
//    Q_PROPERTY(bool inverted READ inverted WRITE setInverted NOTIFY invertedChanged)

//    Q_PROPERTY(QStringList possibleKeys READ possibleKeys NOTIFY possibleKeysChanged)

public:
//    class EntityAndKeywords {
//    public:
//        EntityAndKeywords() = default;
//        EntityAndKeywords(const QModelIndex& entityIndex);

//        QObject* entity = nullptr;
//        QVector<cwKeyword> keywords;
//    };

    enum Role {
        ObjectsRole,
        ObjectCountRole,
        ValueRole,
        AcceptedRole
    };

    Q_ENUM(Role)

    cwKeywordItemFilter(QObject* parent = nullptr);
    ~cwKeywordItemFilter();

    QVector<cwEntityAndKeywords> entities() const;
    void setEntities(QVector<cwEntityAndKeywords> entries);

    QVector<cwEntityAndKeywords> acceptedEntites() const;

    QString key() const;
    void setKey(QString lastKey);

    void invert();

//    bool inverted() const;
//    void setInverted(bool inverted);

//    QFuture<void> modelFuture() const;

//    cwKeywordItemModel* keywordModel() const;
//    void setKeywordModel(cwKeywordItemModel* keywordModel);


//    cwKeywordModel* filterKeywords() const;



//    Q_INVOKABLE void addKeywordFromLastKey(const QString &value);
//    Q_INVOKABLE void removeLastKeyword();


//    QStringList possibleKeys() const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);

    QHash<int, QByteArray> roleNames() const;

    static QString otherCategory();
    QModelIndex otherIndex() const;


signals:
    void entitiesChanged();
    void acceptedEntitesChanged();
    void invertedChanged();
    void keyChanged();
    void possibleKeysChanged();
    void modelFutureChanged();

private:
    class Row {
    public:
        Row() {}
        Row(const QString& value, QVector<QObject*> entities, bool accepted) :
            value(value),
            entities(entities),
            accepted(accepted)
        {}

        QString value;
        QVector<QObject*> entities;
        bool accepted;

        bool operator<(const Row& other) {
            return value < other.value;
        }
    };


    class ModelData {
    public:
        QVector<Row> Rows;
        Row OtherRow = Row(cwKeywordItemFilter::otherCategory(), {}, false);
//        QStringList PossibleKeys;

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
    };

    class Filter {

    public:
        QString lastKey;
        ModelData* data;

        std::function<void (int)> beginInsertFunction;
        std::function<void ()> endInsertFunction;
        std::function<void (int, QVector<int>)> dataChangedFunction;
        std::function<void (int)> beginRemoveFuction;
        std::function<void ()> endRemoveFunction;

        void filterEntity(const cwEntityAndKeywords& pair);
        QSet<QString> entityValues(QObject* entity) const;
        void removeEntity(const QString& value, QObject* entity);

    private:
        void addEntity(const QString& value, QObject* entity);
        void addEntityToOther(QObject* entity);

        void beginInsert(const QVector<Row>::iterator& iter) const;
        void endInsert() const;
        void beginRemove(const QVector<Row>::iterator& iter) const;
        void endRemove() const;
        void dataChanged(const QVector<Row>::iterator& iter) const;
        void dataChanged(int index) const;

        static bool lessThan(const Row& row, const QString& value);
    };

    QVector<cwEntityAndKeywords> mEntries; //Entities passed to this filter
    QVector<cwEntityAndKeywords> mAcceptedEntries; //Enities that have been filtered

    bool mInverted; //!<
    QString mKey; //!<

//    QStringList PossibleKeys; //!<
    ModelData Data;

    Filter mFilter;

//    QFuture<void> LastRun;

//    cwKeywordItemModel* KeywordModel = nullptr; //!<

    void updateAllRows();
    void updateFilterEntries();
//    void waitForFinished();

//    bool isRunning() const;

    Filter createDefaultFilter();

//    static QSet<QString> createPossibleKeys(const QVector<cwKeyword>& keywords, const QVector<EntityAndKeywords> entities);
//    QVector<EntityAndKeywords> entities() const;


};

inline QVector<cwEntityAndKeywords> cwKeywordItemFilter::entities() const {
    return mEntries;
}

/**
* @brief cwKeywordItemFilter::filteredEntries
* @return
*/


///**
//*
//*/
//inline cwKeywordItemModel* cwKeywordItemFilter::keywordModel() const {
//    return KeywordModel;
//}

/**
*
*/
inline QString cwKeywordItemFilter::key() const {
    return mKey;
}


///**
//*
//*/
//inline QStringList cwKeywordItemFilter::possibleKeys() const {
//    return Data.PossibleKeys;
//}

///**
//* @brief cwKeywordItemFilter::keywords
//* @return
//*/
//inline cwKeywordModel* cwKeywordItemFilter::filterKeywords() const {
//    return mFilterKeywords;
//}

//inline bool cwKeywordItemFilter::inverted() const {
//    return mInverted;
//}

//inline QFuture<void> cwKeywordItemFilter::modelFuture() const {
//    return mLastModelData0;
//}


#endif // CWKEYWORDENTITYFILTERMODEL_H
