#ifndef CWSTATIONMODEL_H
#define CWSTATIONMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QList>
#include <QHash>

//Our includes
#include "cwStation.h"

class cwStationModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum StationRoles{
        StationName,
        Left,
        Right,
        Up,
        Down
    };

    explicit cwStationModel(QObject *parent = 0);

    int rowCount(const QModelIndex & parent = QModelIndex()) const;

    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;

    bool setData ( const QModelIndex & index, const QVariant & value, int role = Qt::EditRole );

    Qt::ItemFlags flags ( const QModelIndex & index ) const;

signals:

public slots:
    bool setData(int index, const QString& string, const QVariant& value);

private:
    QList<cwStation*> Stations;
    QHash<QString, int> RoleLookup;

};

#endif // CWSTATIONMODEL_H
