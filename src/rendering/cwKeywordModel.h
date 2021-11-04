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

    Q_INVOKABLE void add(const cwKeyword& keyword);
    void addKeywords(const QVector<cwKeyword>& keywords);
    Q_INVOKABLE void remove(const cwKeyword& keyword);
    void removeAll(QString key);
    Q_INVOKABLE void replace(const cwKeyword& keyword);
    QVector<cwKeyword> keywords() const;

    QVariant data(const QModelIndex& index, int role) const;
    bool setData(const QModelIndex& index, const QVariant& value, int role);
    int rowCount(const QModelIndex& parent = QModelIndex()) const;

    void addExtension(cwKeywordModel* model);
    void removeExtension(cwKeywordModel* model);

    QHash<int, QByteArray> roleNames() const;

    static const QString TypeKey;
    static const QString TripNameKey;
    static const QString YearKey;
    static const QString DateKey;
    static const QString CaverKey;

    Q_INVOKABLE cwKeyword createKeyword(const QString key, QString value) const;

private:
    QVector<cwKeywordModel*> Extentions;
    QVector<cwKeyword> Keywords;
    int CachedRowCount = 0;

    int firstIndex(cwKeywordModel* model) const;
    int localIndex(cwKeywordModel* model, int index) const;
    std::pair<cwKeywordModel *, int> modelAt(const QModelIndex& index) const;
    void updateRowCount();
    bool canKeywordBeAdded(const cwKeyword& keyword);
};





#endif // CWKEYWORDMODEL_H
