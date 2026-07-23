/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSCOPESTATIONLISTMODEL_H
#define CWSCOPESTATIONLISTMODEL_H

//Qt includes
#include <QAbstractListModel>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVector3D>
#include <QtQml/qqmlregistration.h>

//Our includes
#include "cwGlobals.h"
#include "cwSurveyNetwork.h"

class cwTrip;

/**
 * Read-only list model exposing a trip's post-solve stations by their
 * scope-relative (prefix-stripped) name, for autocomplete / reference
 * entry at the note, scrap, and LiDAR sites.
 *
 * Two ways to drive it, most-general first:
 *
 *  - trip: set a cwTrip and the model lists that trip's solved stations
 *    via cwTrip::solvedStations(), which speaks scope-relative names for
 *    *both* native trips (bare chunk-station names) and scoped/external
 *    trips (prefix-stripped tails). This is the general path; a native
 *    trip has an empty scope prefix and could not be selected by the
 *    scopePrefix path below.
 *
 *  - scopePrefix + network: the legacy path — list the network stations
 *    whose qualified name starts with scopePrefix (e.g.
 *    "cave_<uuid>.trip_<uuid>."), prefix-stripped. Used where only a
 *    prefix and a network are on hand (the external-centerline panel).
 *    An empty scopePrefix yields zero rows rather than every station.
 *
 * When a trip is set it takes precedence over scopePrefix. Bind network
 * to cwLinePlotManager::regionNetwork in either mode: its value feeds the
 * legacy path, and its change is the re-solve pulse that re-pulls
 * solvedStations() in trip mode (a full modelReset, no incremental diff).
 * Rows are sorted in natural order by qualified name (trailing station
 * numbers ascend numerically, so "a2" precedes "a10") so consumers stay
 * deterministic.
 */
class CAVEWHERE_LIB_EXPORT cwScopeStationListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ScopeStationListModel)

    Q_PROPERTY(cwTrip* trip READ trip WRITE setTrip NOTIFY tripChanged)
    Q_PROPERTY(QString scopePrefix READ scopePrefix WRITE setScopePrefix NOTIFY scopePrefixChanged)
    Q_PROPERTY(cwSurveyNetwork network READ network WRITE setNetwork NOTIFY networkChanged)

public:
    enum Roles {
        StationNameRole = Qt::UserRole + 1, //prefix-stripped display name
        QualifiedNameRole,                  //full qualified name
        PositionRole                        //QVector3D from the network
    };
    Q_ENUM(Roles)

    explicit cwScopeStationListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    cwTrip* trip() const { return m_trip; }
    void setTrip(cwTrip* trip);

    QString scopePrefix() const { return m_scopePrefix; }
    void setScopePrefix(const QString& scopePrefix);

    cwSurveyNetwork network() const { return m_network; }
    void setNetwork(const cwSurveyNetwork& network);

    //Non-blocking membership check: is displayName one of the in-scope
    //station names? Case-insensitive, canonicalized like the rows.
    //Drives the note-entry advisory ("a2 isn't in this trip") without
    //touching the permissive cwStationValidator.
    Q_INVOKABLE bool containsStation(const QString& displayName) const;

    //In-scope display names whose start matches prefix (case-insensitive),
    //in row order, for the note-entry autocomplete dropdown + inline ghost.
    //An empty prefix returns every in-scope name.
    Q_INVOKABLE QStringList matchingStations(const QString& prefix) const;

signals:
    void tripChanged();
    void scopePrefixChanged();
    void networkChanged();

private:
    struct Row {
        QString displayName;
        QString qualifiedName;
        QVector3D position;
    };

    void rebuildRows();

    QPointer<cwTrip> m_trip;
    QString m_scopePrefix;
    cwSurveyNetwork m_network;
    QList<Row> m_rows;
};

#endif // CWSCOPESTATIONLISTMODEL_H
