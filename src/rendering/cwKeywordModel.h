#ifndef CWKEYWORDMODEL_H
#define CWKEYWORDMODEL_H

//Qt includes
#include <QAbstractItemModel>
#include <QMultiMap>

//Our includes
#include "cwKeyword.h"
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwKeywordModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role {
        KeyRole,
        ValueRole,
        KeywordRole
    };
    Q_ENUM(Role)

    cwKeywordModel(QObject* parent = nullptr);

    void add(const cwKeyword& keyword);
    void remove(const cwKeyword& keyword);
    void removeAll(QString key);
    QVector<cwKeyword> keywords() const;

    QVariant data(const QModelIndex& index, int role) const;
    bool setData(const QModelIndex& index, const QVariant& value, int role);
    int rowCount(const QModelIndex& parent = QModelIndex()) const;

    QHash<int, QByteArray> roleNames() const;

private:
    QVector<cwKeyword> Keywords;
};

/**
 * Return all the keywords in the model
 */
inline QVector<cwKeyword> cwKeywordModel::keywords() const
{
    return Keywords;
}

/**
 * Returns the number of keywords that are in the model
 */
inline int cwKeywordModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return Keywords.size();
}

#endif // CWKEYWORDMODEL_H
