/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

// One surface's in-flight attach or replace, and the attribution that
// picks its report out of the manager's shared attachCompleted signal.
//
// The owner id is snapshotted at the click rather than read off a live
// trip binding, so a host that rebinds (or nulls) trip mid-operation
// can neither wedge busy forever nor let a sibling surface's report end
// this session. The Trip itself is kept only for cancel(), which needs
// the QUuid; losing it (trip destroyed mid-operation) costs the ability
// to cancel and nothing else - the manager's owner-was-deleted failure
// report still matches the id string and releases the surface.
QQ.Item {
    id: root

    // Snapshot of the owner this session started with. Held as an
    // implementation detail of attribution; hosts read busy instead.
    property Trip owner: null
    property string ownerId: ""

    // The failure text of the last report, for the surface to display.
    // Cleared at every start().
    property string errorMessage: ""

    readonly property bool busy: ownerId.length > 0

    // Exactly one fires per started session that ends. A failure fires
    // neither - errorMessage carries it and the surface stays open for
    // another attempt.
    signal succeeded()
    signal canceled()

    function start(trip) {
        root.errorMessage = ""
        root.owner = trip
        root.ownerId = String(trip.id)
    }

    // Honored only until the operation's internal scan lands; a later
    // call is a structural no-op (the manager's q14 rule), so the button
    // driving this can stay enabled for the whole operation.
    function cancel() {
        if (root.owner !== null) {
            RootData.externalCenterlineManager.cancelAttach(root.owner.id)
        }
    }

    function reset() {
        root.errorMessage = ""
        root.owner = null
        root.ownerId = ""
    }

    QQ.Connections {
        target: RootData.externalCenterlineManager

        function onAttachCompleted(report) {
            // QUuid wrappers never compare equal directly in JS - compare
            // the string forms. An idle session's empty id matches no
            // report, so this doubles as the not-ours guard.
            if (String(report.ownerId) !== root.ownerId) {
                return
            }
            root.owner = null
            root.ownerId = ""
            if (report.success) {
                root.succeeded()
            } else if (report.canceled) {
                root.canceled()
            } else {
                root.errorMessage = report.errorMessage
            }
        }
    }
}
