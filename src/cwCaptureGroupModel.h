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

//Our includes
class cwViewportCapture;
class cwCaptureGroup;

class cwCaptureGroupModel : public QAbstractItemModel
{
    Q_OBJECT

    Q_ENUMS(Roles)
public:
    enum Roles {
        CaptureObjectRole,
        CaptureNameRole
    };

    explicit cwCaptureGroupModel(QObject *parent = 0);

    Q_INVOKABLE void addGroup();
    Q_INVOKABLE void addCapture(QModelIndex parentGroup, cwViewportCapture* capture);
    void removeCapture(cwViewportCapture* capture);

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
