#ifndef CWKEYWORDENTITYFILTERMODEL_H
#define CWKEYWORDENTITYFILTERMODEL_H

//Our include
#include "cwKeywordEntityModel.h"
#include "cwGlobals.h"

//Qt includes
#include <Qt3DCore/QEntity>

//Std includes
#include <memory>

class CAVEWHERE_LIB_EXPORT cwKeywordEntityFilterModel : public QAbstractItemModel
{
    Q_OBJECT

    Q_PROPERTY(cwKeywordEntityModel* keywordModel READ keywordModel WRITE setKeywordModel NOTIFY keywordModelChanged)
    Q_PROPERTY(QStringList keys READ keys NOTIFY keysChanged)

public:

    enum Role {
        EntitiesRole,
        EntitiesCountRole,
        ValueRole,
    };

    Q_ENUM(Role)

    cwKeywordEntityFilterModel(QObject* parent = nullptr);

    cwKeywordEntityModel* keywordModel() const;
    void setKeywordModel(cwKeywordEntityModel* keywordModel);

    void addKey(const QString& key);
    void removeKey(int index);
    QStringList keys() const;

    int rowCount(const QModelIndex& parent) const;
    int columnCount(const QModelIndex& parent) const;
    QModelIndex parent(const QModelIndex& index) const;
    QModelIndex index(int row, int column, const QModelIndex& parent) const;
    QVariant data(const QModelIndex& index, int role) const;

signals:
    void keywordModelChanged();
    void keysChanged();

private:

    class List {
    public:
        List(QString value = QString()) :
            value(value)
        {}

        ~List();

        List* parent = nullptr;
        QVector<List*> children; //Sorted by value
        QVector<Qt3DCore::QEntity*> entities;
        QString value;
        int keyIndex = 0;

        void clearChildren();
        void deleteChildren();

        void addChild(const QString& value, Qt3DCore::QEntity* entity);

        static bool lessThan(const List* list, const QString& value) {
            return list->value < value;
        }
    };

    std::unique_ptr<List> RootList;

    cwKeywordEntityModel* KeywordModel = nullptr; //!<
    QStringList Keys;

    void addEntity(const QModelIndex& entityIndex, List *rootList);
    void addEntities(List* rootList);

    List* toList(const QModelIndex& index) const;
};

/**
*
*/
inline QStringList cwKeywordEntityFilterModel::keys() const {
    return Keys;
}

/**
*
*/
inline cwKeywordEntityModel* cwKeywordEntityFilterModel::keywordModel() const {
    return KeywordModel;
}

#endif // CWKEYWORDENTITYFILTERMODEL_H
