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

class cwCave;
class cwSurveyNetwork;

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
        IdRole,
        //! Read-only: a short message when the row's coordinate falls outside
        //! the valid domain of its own input CS (a transposed digit, the wrong
        //! zone), empty otherwise. A pure per-row check, so the FixStationPage
        //! can flag the bad cell inline — independent of the region-wide
        //! outlier detection in cwFixStationValidator.
        DomainErrorRole,
        //! Read-only bools: whether the easting / northing specifically falls
        //! outside the input CS's valid domain, so the page can tint just the
        //! offending coordinate cell red. Both false when the row is fine or the
        //! CS is un-checkable. Elevation is never domain-flagged (z is not part
        //! of the check).
        EastingDomainErrorRole,
        NorthingDomainErrorRole,
        //! Read-only: a message when this fix's station reference is broken,
        //! empty otherwise. Two ways it breaks, both of which survex silently
        //! drops: the name matches no station in the owning cave's network, or
        //! there is no name at all. A cave whose network hasn't been computed yet
        //! never flags a *named* fix (nothing to check against); an empty name is
        //! flagged regardless.
        StationErrorRole
    };
    Q_ENUM(Roles)

    //! A fix's station-name reference measured against a cave's survey network,
    //! shared by this model's StationErrorRole and cwFixStationValidator's
    //! cave-level FixStationReference warning so both read the same verdict.
    //! A named fix defers to Ok while the network is empty (nothing to check
    //! against yet); an empty name is always flagged (survex drops such a fix).
    enum class StationReference {
        Ok,      //!< names an existing station (or nothing to check against)
        Empty,   //!< no name — an incomplete fix that survex silently drops
        Unknown  //!< a non-empty name that no survey station matches
    };
    static StationReference classifyStationReference(const QString& stationName,
                                                     const cwSurveyNetwork& network);

    //! True when a dataChanged() roles list carries only the read-only, computed
    //! error roles (domain / station warnings). Those roles are derived from the
    //! solve output, so a consumer that re-runs the line plot (or re-resolves
    //! declination) on such a change would feed back into an endless loop — it
    //! must skip these. An empty roles list means "all roles" per Qt and returns
    //! false (treat as a real edit).
    static bool isErrorOnlyRoleChange(const QList<int>& roles);

    explicit cwFixStationModel(QObject* parent = nullptr);
    ~cwFixStationModel() override;

    //! Wire the model to the cave that owns it, so StationErrorRole can resolve
    //! names against the cave's survey network and refresh live when it changes.
    //! Called once by cwCave right after construction; re-pointing at a
    //! different cave drops the previous cave's wiring first.
    void setCave(cwCave* cave);

    //! Re-emit the domain roles for every row. The per-row check falls back to
    //! the region's global CS for a fix that omits its own, so a change to that
    //! CS moves rows in and out of the flagged state with no edit to the model.
    //! Wired by cwCavingRegion alongside cwCave::recomputeGridConvergence.
    void refreshDomainErrors();

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
    //! StationErrorRole's message for one fix, resolved against m_cave's network.
    //! Empty when the reference is Ok (or there is no cave); distinct messages
    //! for an Unknown name and a missing one.
    QString stationErrorMessage(const cwFixStation& fix) const;

    //! The region's global CS, for a fix that omits its own input CS. Empty when
    //! the model has no cave, or its cave no region, or the region no global CS.
    QString fallbackCS() const;

    //! Emit dataChanged over every row for read-only computed error roles only —
    //! callers must pass nothing but those, since consumers skip such a change
    //! (cwFixStationModel::isErrorOnlyRoleChange).
    void refreshComputedErrors(const QList<int>& roles);

    QList<cwFixStation> m_fixStations;

    //! The owning cave, for StationErrorRole's network lookup. Null until
    //! setCave() runs; the model degrades to "no station error" without it.
    cwCave* m_cave = nullptr;
};

#endif // CWFIXSTATIONMODEL_H
