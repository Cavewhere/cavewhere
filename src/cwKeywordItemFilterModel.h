#ifndef CWKEYWORDENTITYFILTERMODEL_H
#define CWKEYWORDENTITYFILTERMODEL_H

//Our include
#include "cwKeywordItemModel.h"
#include "cwGlobals.h"
#include "cwKeyword.h"

//Qt includes
#include <Qt3DCore/QEntity>
#include <QList>
#include <QAbstractListModel>
#include <QFuture>

//Std includes
#include <memory>

class CAVEWHERE_LIB_EXPORT cwKeywordItemFilterModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(cwKeywordItemModel* keywordModel READ keywordModel WRITE setKeywordModel NOTIFY keywordModelChanged)
    Q_PROPERTY(QList<cwKeyword> keywords READ keywords WRITE setKeywords NOTIFY keywordsChanged)
    Q_PROPERTY(QString lastKey READ lastKey WRITE setLastKey NOTIFY keyLastChanged)
    Q_PROPERTY(QStringList possibleKeys READ possibleKeys NOTIFY possibleKeysChanged)

public:
    enum Role {
        ObjectsRole,
        ObjectCountRole,
        ValueRole,
    };

    Q_ENUM(Role)

    cwKeywordItemFilterModel(QObject* parent = nullptr);
    ~cwKeywordItemFilterModel();

    cwKeywordItemModel* keywordModel() const;
    void setKeywordModel(cwKeywordItemModel* keywordModel);

    QList<cwKeyword> keywords() const;
    void setKeywords(QList<cwKeyword> keywords);

    QString lastKey() const;
    void setLastKey(QString lastKey);

    QStringList possibleKeys() const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role) const;

    QHash<int, QByteArray> roleNames() const;

    static QString otherCategory();

    void waitForFinished();

signals:
    void keywordModelChanged();
    void keywordsChanged();
    void keyLastChanged();
    void possibleKeysChanged();

private:
    class Row {
    public:
        Row() {}
        Row(const QString& value, QList<QObject*> entities) :
            value(value),
            entities(entities)
        {}

        QString value;
        QList<QObject*> entities;

        bool operator<(const Row& other) {
            return value < other.value;
        }
    };

    class EntityAndKeywords {
    public:
        EntityAndKeywords() = default;
        EntityAndKeywords(const QModelIndex& entityIndex);

        QObject* entity = nullptr;
        QVector<cwKeyword> keywords;
    };

    class ModelData {
    public:
        QVector<Row> Rows;
        Row OtherRow = Row(cwKeywordItemFilterModel::otherCategory(), {});
        QStringList PossibleKeys;

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
    };

    class Filter {

    public:
        QList<cwKeyword> keywords;
        QString lastKey;
        ModelData* data;

        std::function<void (int)> beginInsertFunction;
        std::function<void ()> endInsertFunction;
        std::function<void (int, QVector<int>)> dataChangedFunction;
        std::function<void (int)> beginRemoveFuction;
        std::function<void ()> endRemoveFunction;

        void filterEntity(const EntityAndKeywords& pair);
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

    QList<cwKeyword> Keywords; //!<
    QString LastKey; //!<
    QStringList PossibleKeys; //!<
    ModelData Data;

    QFuture<void> LastRun;
    QFuture<ModelData> LastModelData;

    cwKeywordItemModel* KeywordModel = nullptr; //!<

    void updateAllRows();
    bool isRunning() const;

    Filter createDefaultFilter();

    static QSet<QString> createPossibleKeys(const QList<cwKeyword>& keywords, const QVector<EntityAndKeywords> entities);



};

/**
*
*/
inline cwKeywordItemModel* cwKeywordItemFilterModel::keywordModel() const {
    return KeywordModel;
}

/**
*
*/
inline QList<cwKeyword> cwKeywordItemFilterModel::keywords() const {
    return Keywords;
}

/**
*
*/
inline QString cwKeywordItemFilterModel::lastKey() const {
    return LastKey;
}


/**
*
*/
inline QStringList cwKeywordItemFilterModel::possibleKeys() const {
    return Data.PossibleKeys;
}

#endif // CWKEYWORDENTITYFILTERMODEL_H
