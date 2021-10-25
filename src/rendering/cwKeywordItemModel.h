#ifndef CWKEYWORDITEMMODEL_H
#define CWKEYWORDITEMMODEL_H

//Qt includes
#include <QAbstractItemModel>

//Our includes
#include "cwGlobals.h"
class cwKeywordItem;
class cwKeywordModel;

class CAVEWHERE_LIB_EXPORT cwKeywordItemModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Role {
        ItemRole,
        ObjectRole,
        KeywordModelRole,
        KeywordsRole,
        KeyRole,
        ValueRole,
        KeywordRole
    };

    cwKeywordItemModel(QObject* parent = nullptr);

    Q_INVOKABLE void addItem(cwKeywordItem* item);
    Q_INVOKABLE void removeItem(cwKeywordItem* item);

    QVariant data(const QModelIndex& index, int role) const;
    int rowCount(const QModelIndex& index = QModelIndex()) const;
    int columnCount(const QModelIndex& index = QModelIndex()) const;
    QModelIndex index(int row, int column, const QModelIndex& parent) const;
    QModelIndex parent(const QModelIndex& index) const;
    QModelIndex indexOf(cwKeywordItem *item) const;

private:

    enum DisconnectionType {
        DisconnectAll,
        DisconnectNone
    };

    QVector<cwKeywordItem*> Items;

    cwKeywordModel* keywordModel(const QModelIndex& index) const;
    void remove(cwKeywordItem* item, DisconnectionType type = DisconnectAll);

    int indexOf(const QModelIndex& modelIndex) const;


};

#endif // CWKEYWORDITEMMODEL_H
