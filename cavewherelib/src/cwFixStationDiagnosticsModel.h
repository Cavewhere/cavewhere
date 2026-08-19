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
        //! Read-only: a message when the row has no coordinate that can be read
        //! at all, empty otherwise. Never set at the same time as
        //! DomainErrorRole, which judges a coordinate the row does have: the
        //! domain check defers whenever state() isn't Valid, which is exactly
        //! when this speaks. So the two can share one warning slot.
        //!
        //! It defers on the <i>state</i>, not on the row's components being 0 —
        //! (0, 0) is inside the area of use of plenty of coordinate systems, so
        //! reading the components would defer inconsistently or not at all.
        //!
        //! Two reasons, and they get different messages — text that won't parse
        //! (<i>this can't be read</i>) and text with no coordinate system to read
        //! it under (<i>choose one</i>). Blank for the row nobody has filled in
        //! yet, which is not a complaint.
        CoordinateErrorRole,
        //! Read-only bool: nothing recorded which axis this row's coordinate
        //! leads with. Exactly cwFixStation::NoSystem — a row that names a
        //! system derives the order from it, and a row with no text has nothing
        //! to order.
        //!
        //! Naming a <i>geographic</i> system on such a row reads it
        //! latitude-first whatever it was written as, and there is no way to
        //! detect the transposition afterwards, so the entry surfaces ask before
        //! they let it happen (cwCoordinateText::swapHorizontal()). Read it
        //! before the coordinate system is committed: the write is what
        //! makes the answer false.
        CoordinateOrderUnknownRole,
        //! Read-only: a message when this fix's station reference is broken,
        //! empty otherwise. Two ways it breaks, both of which survex silently
        //! drops: the name matches no station in the owning cave's network, or
        //! there is no name at all. A cave whose network hasn't been computed yet
        //! never flags a *named* fix (nothing to check against); an empty name is
        //! flagged regardless.
        StationErrorRole,
        //! Read-only QStringList: the datum codes the row's coordinate could
        //! sensibly be on, WGS84 first, for the picker's datum combo to offer.
        //! Refreshed exactly when DomainErrorRole is — both are functions of the
        //! coordinate and the CS it is read under.
        //!
        //! A row with a readable coordinate offers WGS84 plus every plate-fixed
        //! datum whose region covers it (a border cave gets two), and, when the
        //! regions leave it out, whatever the row already stores: the combo has
        //! to be able to display what the row is on. Nothing here rewrites the
        //! row — an out-of-region pairing is what DomainErrorRole reports, and
        //! changing the datum is the user's act alone.
        //!
        //! A row with no readable coordinate offers WGS84 and its own datum,
        //! since there is no location to filter by. The picker disables the
        //! combo in that case, so the list is only what it displays.
        AvailableDatumsRole
    };
    Q_ENUM(Roles)

    //! Proxies the cave's own cwFixStationModel and resolves StationErrorRole
    //! against the cave's survey network. The cave is the parent and is fixed
    //! for the proxy's lifetime — cwCave constructs exactly one of these and
    //! never re-points it, so there is no wiring to tear down.
    explicit cwFixStationDiagnosticsModel(cwCave* cave);
    ~cwFixStationDiagnosticsModel() override;

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
