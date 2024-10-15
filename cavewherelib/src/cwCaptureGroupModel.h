/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWCAPTUREGROUPMODEL_H
#define CWCAPTUREGROUPMODEL_H

//Qt includes
#include <QAbstractItemModel>
#include <QQmlEngine>

//Our includes
#include "cwCaptureViewport.h"
class cwCaptureViewport;
class cwCaptureGroup;

class cwCaptureGroupModel : public QAbstractItemModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CaptureGroupModel)

public:
    enum Roles {
        CaptureObjectRole,
        CaptureNameRole
    };
    Q_ENUM(Roles)

    explicit cwCaptureGroupModel(QObject *parent = 0);

    Q_INVOKABLE void addGroup();
    Q_INVOKABLE void addCapture(QModelIndex parentGroup, cwCaptureViewport* capture);
    void removeCapture(cwCaptureViewport* capture);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent) const;
    Q_INVOKABLE QVariant data(const QModelIndex &index, int role) const;
    Q_INVOKABLE QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;
    QHash<int, QByteArray> roleNames() const;

signals:

public slots:

private:
    QList<cwCaptureGroup*> Groups;

};

/**
 * @brief cwCaptureGroupModel::columnCount
 * @param parent
 * @return Always returns 0
 */
inline int cwCaptureGroupModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 0;
}

#endif // CWCAPTUREGROUPMODEL_H
