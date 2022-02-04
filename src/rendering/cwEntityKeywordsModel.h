#ifndef CWENTITYKEYWORDSMODEL_H
#define CWENTITYKEYWORDSMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QVector>

//Our includes
#include "cwEntityAndKeywords.h"
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwEntityKeywordsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        EntityKeywordsRole,
        EntityRole,
        KeywordsRole
    };

    explicit cwEntityKeywordsModel(QObject *parent = nullptr);

    void insert(const cwEntityAndKeywords& row);
    void remove(QObject* entity);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

private:
    QVector<cwEntityAndKeywords> mRows; //Sorted rows by pointer value

    static bool entityKeywordsLessThan(const cwEntityAndKeywords& a, const cwEntityAndKeywords& b);
    static bool entityLessThan(const cwEntityAndKeywords& a, QObject* b);
};

#endif // CWENTITYKEYWORDSMODEL_H
