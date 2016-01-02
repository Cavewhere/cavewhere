/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWSPLAYMODEL_H
#define CWSPLAYMODEL_H

//Qt includes
#include <QObject>
#include <QAbstractListModel>

//Our includes
#include "cwShot.h"
#include "cwSurveyChunk.h"

class cwSplayModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit cwSplayModel(QObject *parent = 0);

    QList<cwShot> splays(const QString& stationName) const;
    void appendSplay(const QString &name, const cwShot& shot);
    void removeSplay(const QString &name, int index);
    void clear();
    void clear(const QString& name);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);
    QHash<int, QByteArray> roleNames() const;
    QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;

signals:

public slots:

private:
    class Node {

    public:
        Node(QString name) :
            Name(name)
        {}

        QString Name;
        QList<cwShot> Splays;

    };

    //Database of splay
    QList<Node*> Nodes;
    QHash<QString, Node*> StationsIndexLookup;

//    QHash<QString, int> StationsIndexLookup;
//    QStringList Stations;
//    QHash<int, QList<cwShot>> Splays;

//    void updateLookup();
//    void appendNewSplays(QString name, const QList<cwShot>& shots);

    QModelIndex nodeToIndex(Node* node) const;
    Node* indexToNode(const QModelIndex& index) const;

};

#endif // CWSPLAYMODEL_H
