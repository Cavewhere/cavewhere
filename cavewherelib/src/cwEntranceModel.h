/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWENTRANCEMODEL_H
#define CWENTRANCEMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QList>
#include <QQmlEngine>

//Our includes
#include "cwEntrance.h"
#include "cwGlobals.h"

/**
 * QAbstractListModel of the cwEntrance rows a survey node marks, keyed by
 * station name the way cwFixStationModel keys its fixes.
 *
 * Every role is persisted data, so dataChanged() means "save this". The model
 * itself never touches the node's fixes: cwSurveyNode's user verbs are the one
 * place a fix and an entrance change together.
 */
class CAVEWHERE_LIB_EXPORT cwEntranceModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(EntranceModel)

    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        StationNameRole = Qt::UserRole + 1,
        //! The raw label; empty when the user gave none.
        LabelRole,
        IdRole
    };
    Q_ENUM(Roles)

    explicit cwEntranceModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    Q_INVOKABLE QModelIndex index(int row, int column = 0, const QModelIndex& parent = QModelIndex()) const override;
    Q_INVOKABLE QVariant data(const QModelIndex& index, int role) const override;
    Q_INVOKABLE bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QHash<int, QByteArray> roleNames() const override;

    //! Row of the entrance on \a stationName, or -1. Matches the way
    //! cwFixStationModel::indexOf() does: trim, then case-insensitive. An empty
    //! name matches nothing.
    Q_INVOKABLE int indexOf(const QString& stationName) const;
    //! Row of the entrance carrying \a entranceId, or -1.
    int indexOf(const QUuid& entranceId) const;
    Q_INVOKABLE bool isEntrance(const QString& stationName) const;

    //! Row of the entrance on \a stationName, appending one when the station
    //! is new to the list. Idempotent by name; -1 for an empty name.
    Q_INVOKABLE int addEntrance(const QString& stationName);
    Q_INVOKABLE void removeEntrance(const QString& stationName);
    Q_INVOKABLE void removeAt(int row);
    Q_INVOKABLE cwEntrance entranceAt(int row) const;

    void appendEntrance(const cwEntrance& entrance);
    //! Inserts \a entrance at \a row, clamped to the rows the model has.
    void insertEntrance(int row, const cwEntrance& entrance);
    void setEntrances(const QList<cwEntrance>& entrances);
    const QList<cwEntrance>& entrances() const { return m_entrances; }
    int count() const;

signals:
    void countChanged();

    //! The set of stations marked as entrances changed: a row was added,
    //! removed, replaced or renamed. A label edit leaves it alone.
    void entranceStationsChanged();

private:
    QList<cwEntrance> m_entrances;
};

#endif // CWENTRANCEMODEL_H
