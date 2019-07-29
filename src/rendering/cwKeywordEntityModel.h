#ifndef CWKEYWORDENTRYMODEL_H
#define CWKEYWORDENTRYMODEL_H

//Qt includes
#include <QAbstractItemModel>
#include <QEntity>

//Our includes
#include "cwGlobals.h"
class cwKeywordComponent;
class cwKeywordModel;

class CAVEWHERE_LIB_EXPORT cwKeywordEntityModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Role {
        EntityRole,
        KeyRole,
        ValueRole,
        KeywordRole
    };

    cwKeywordEntityModel(QObject* parent = nullptr);

    void addComponent(cwKeywordComponent* component);
    void removeComponent(cwKeywordComponent* component);

    QVariant data(const QModelIndex& index, int role) const;
    int rowCount(const QModelIndex& index = QModelIndex()) const;
    int columnCount(const QModelIndex& index = QModelIndex()) const;
    QModelIndex index(int row, int column, const QModelIndex& parent) const;
    QModelIndex parent(const QModelIndex& index) const;
    QModelIndex indexOf(Qt3DCore::QEntity *entity) const;

private:
    class KeywordEntity {
    public:
        KeywordEntity() = default;
        KeywordEntity(cwKeywordComponent* keyword, Qt3DCore::QEntity* entity) :
            keywordComponent(keyword),
            entity(entity)
        {}

        cwKeywordComponent* keywordComponent = nullptr;
        Qt3DCore::QEntity* entity = nullptr;

        bool operator<(const KeywordEntity& other) const {
            return entity < other.entity;
        }

        bool operator==(const KeywordEntity& other) const {
            return entity == other.entity && keywordComponent == other.keywordComponent;
        }
    };

    QVector<KeywordEntity> Entities;

    cwKeywordModel* keywordModel(const QModelIndex& index) const;
    void remove(Qt3DCore::QEntity *entity);

    int indexOf(const QModelIndex& modelIndex) const;


};

#endif // CWKEYWORDENTRYMODEL_H
