/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWFIXSTATIONDIAGNOSTICSMODEL_H
#define CWFIXSTATIONDIAGNOSTICSMODEL_H

//Qt includes
#include <QIdentityProxyModel>
#include <QList>
#include <QQmlEngine>

//Our includes
#include "cwGlobals.h"

class cwCave;
class cwFixStation;
class cwFixStationModel;

/**
 * Adds the read-only, computed error roles on top of a cave's cwFixStationModel.
 *
 * The verdicts are cwFixStationDiagnostics'; this class only decides which role
 * carries which one, and when to re-emit it.
 *
 * These verdicts are derived from the line-plot solve and the region's
 * coordinate system, not from anything the user typed or the project persists,
 * so they deliberately live outside the row model: cwFixStationModel::dataChanged
 * then means only "persisted data changed", and the consumers that re-solve,
 * re-validate or save on it (cwLinePlotManager, cwFixStationValidator,
 * cwTripCalibration, cwSaveLoad) connect to the source model and cannot hear a
 * diagnostics refresh at all. Before this split they each had to remember a role
 * filter, and a consumer that forgot silently dirtied the project or re-solved
 * in a loop.
 *
 * FixStationPage binds its delegates to this proxy. Row indices and role names
 * pass through unchanged, so edits still go to the source model's setData().
 */
class CAVEWHERE_LIB_EXPORT cwFixStationDiagnosticsModel : public QIdentityProxyModel
{
    Q_OBJECT
    QML_NAMED_ELEMENT(FixStationDiagnosticsModel)
    QML_UNCREATABLE("Owned by Cave; access via cave.fixStationDiagnostics")

public:
    //! Numbered clear of cwFixStationModel's Qt::UserRole + 1… block: roleNames()
    //! merges both sets into one hash, so an overlapping value would shadow a
    //! persisted role rather than fail loudly.
    enum Roles {
        //! Read-only: a short message when the row's coordinate falls outside
        //! the valid domain of its own input CS (a transposed digit, the wrong
        //! zone), empty otherwise. A pure per-row check, so the FixStationPage
        //! can flag the bad cell inline — independent of the region-wide
        //! outlier detection in cwFixStationValidator.
        DomainErrorRole = Qt::UserRole + 100,
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

    //! Proxies the cave's own cwFixStationModel and resolves StationErrorRole
    //! against the cave's survey network. The cave is the parent and is fixed
    //! for the proxy's lifetime — cwCave constructs exactly one of these and
    //! never re-points it, so there is no wiring to tear down.
    explicit cwFixStationDiagnosticsModel(cwCave* cave);
    ~cwFixStationDiagnosticsModel() override;

    //! Re-emit the domain roles for every row. The per-row check falls back to
    //! the region's global CS for a fix that omits its own, so a change to that
    //! CS moves rows in and out of the flagged state with no edit to the model.
    //! Wired by cwCavingRegion alongside cwCave::recomputeGridConvergence.
    void refreshDomainErrors();

    //! Overridden only for the column default QAbstractItemModel::index lacks,
    //! so QML can write index(row) as it does on cwFixStationModel. data() and
    //! setData() are inherited — the base already declares both Q_INVOKABLE, so
    //! a QML call reaches this class's data() by virtual dispatch.
    Q_INVOKABLE QModelIndex index(int row, int column = 0, const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    //! Re-emit the derived roles a source edit invalidates. The domain check
    //! reads the easting, northing and input CS; the station reference is judged
    //! from the station name. Only ever carries roles from this class, so the
    //! source model's own consumers are untouched.
    void augmentSourceChange(const QModelIndex& topLeft,
                             const QModelIndex& bottomRight,
                             const QList<int>& roles);

    //! Emit dataChanged over every row for the given derived roles.
    void refreshRoles(const QList<int>& roles);

    //! StationErrorRole's message for one fix, resolved against m_cave's network.
    //! Empty when the reference is Ok (or there is no cave); distinct messages
    //! for an Unknown name and a missing one.
    QString stationErrorMessage(const cwFixStation& fix) const;

    //! The region's global CS, for a fix that omits its own input CS. Empty when
    //! the proxy has no cave, or its cave no region, or the region no global CS.
    QString fallbackCS() const;

    //! The source row's fix, or null when there is no source model or the index
    //! is out of range.
    const cwFixStation* fixAt(const QModelIndex& proxyIndex) const;

    //! The typed source model, for direct row access. Never null, never
    //! re-pointed. Not pointee-const: the base class hands this to
    //! setSourceModel() and forwards edits into it, so the proxy is a write
    //! path to this model even though our own reads of it are const.
    cwFixStationModel* const m_fixStations;

    //! The owning cave, for StationErrorRole's network lookup and the region CS
    //! fallback. Never null, never re-pointed, and only ever read through.
    const cwCave* const m_cave;
};

#endif // CWFIXSTATIONDIAGNOSTICSMODEL_H
