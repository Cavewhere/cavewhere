/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWFIXSTATIONMODEL_H
#define CWFIXSTATIONMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QList>
#include <QQmlEngine>

//Our includes
#include "cwFixStation.h"
#include "cwGlobals.h"

/**
 * QAbstractListModel of cwFixStation rows owned by a cwCave.
 *
 * QML edits cells in place via setData() using the role names exposed in
 * roleNames(). Add and remove rows with addFixStation() / removeAt(index).
 */
class CAVEWHERE_LIB_EXPORT cwFixStationModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(FixStationModel)

    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        StationNameRole = Qt::UserRole + 1,
        InputCSRole,
        EastingRole,
        NorthingRole,
        ElevationRole,
        HorizontalVarianceRole,
        VerticalVarianceRole,
        IdRole
    };
    Q_ENUM(Roles)

    explicit cwFixStationModel(QObject* parent = nullptr);
    ~cwFixStationModel() override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    Q_INVOKABLE QModelIndex index(int row, int column = 0, const QModelIndex& parent = QModelIndex()) const override;
    Q_INVOKABLE QVariant data(const QModelIndex& index, int role) const override;
    Q_INVOKABLE bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addFixStation();
    Q_INVOKABLE void removeAt(int index);
    Q_INVOKABLE cwFixStation fixStationAt(int index) const;

    void appendFixStation(const cwFixStation& fix);
    void setFixStations(const QList<cwFixStation>& fixes);
    const QList<cwFixStation>& fixStations() const { return m_fixStations; }
    int count() const { return m_fixStations.size(); }

signals:
    void countChanged();

private:
    QList<cwFixStation> m_fixStations;
};

#endif // CWFIXSTATIONMODEL_H
