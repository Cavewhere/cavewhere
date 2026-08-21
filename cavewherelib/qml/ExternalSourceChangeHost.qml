/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Layouts
import cavewherelib

/**
  Where the app-scope source-change banner and its review list are wired to
  the manager (plans/EXTERNAL_SOURCE_CHANGE_NOTIFY.html §4). One per window,
  held by AppOverlay, so the strip floats over the top of the window's
  content instead of pushing a page around.

  The banner and the list stay dumb; everything that knows about the
  manager is here: the live count, the [Update all] and per-row Update
  verbs, and the dismissal bookkeeping.

  Dismissal is remembered per fingerprint and for this session only: what is
  recorded is the revision each changed source had when the user pressed ✕,
  so the banner comes back when that source is edited again, and a fresh
  session re-asks the question honestly from what is on disk then.
 */
QQ.Item {
    id: root
    objectName: "externalSourceChangeHost"

    property ExternalCenterlineManager externalCenterlineManager: RootData.externalCenterlineManager

    readonly property ExternalSourceStatusModel statusModel:
        root.externalCenterlineManager.sourceStatusModel

    // How many sources the banner says have changed — live from the model.
    readonly property int changedCount: root.statusModel.changedCount

    // Whether at least one changed source is one the user has not already
    // waved off at its current revision. What decides the banner is shown.
    property bool hasUndismissedChange: false

    property bool listShown: false

    height: contentColumnId.implicitHeight
    visible: root.hasUndismissedChange

    // True when this owner's source was already waved off with exactly the
    // contents it has now. A later edit moves the revision and the answer
    // goes back to false, which is what brings the banner back.
    function isDismissed(ownerKey: string, sourceRevision: string) : bool {
        for (let i = 0; i < dismissedRevisionsId.count; ++i) {
            const dismissed = dismissedRevisionsId.get(i)
            if (dismissed.ownerKey === ownerKey) {
                return dismissed.sourceRevision === sourceRevision
            }
        }
        return false
    }

    function rememberDismissal(ownerKey: string, sourceRevision: string) {
        for (let i = 0; i < dismissedRevisionsId.count; ++i) {
            if (dismissedRevisionsId.get(i).ownerKey === ownerKey) {
                dismissedRevisionsId.setProperty(i, "sourceRevision", sourceRevision)
                return
            }
        }
        dismissedRevisionsId.append({ ownerKey: ownerKey, sourceRevision: sourceRevision })
    }

    // Re-reads the manager's answer: whether anything undismissed is
    // changed, and the rows the review list shows.
    function refresh() {
        const sources = root.externalCenterlineManager.sourcesNeedingAttention()

        let undismissed = false
        reviewRowsId.clear()
        for (const source of sources) {
            reviewRowsId.append({
                ownerKey: source.ownerKey,
                ownerName: source.ownerName,
                ownerKind: source.ownerKind,
                caveName: source.caveName,
                sourcePath: source.sourcePath,
                sourceRevision: source.sourceRevision,
                status: source.status
            })

            if (source.status === ExternalSourceStatusModel.Changed
                    && !root.isDismissed(source.ownerKey, source.sourceRevision)) {
                undismissed = true
            }
        }

        root.hasUndismissedChange = undismissed
        if (!undismissed) {
            root.listShown = false
        }
    }

    // Waves off every changed source at the revision it has now — the rows
    // the last refresh built, which are what the user is looking at.
    function dismiss() {
        for (let i = 0; i < reviewRowsId.count; ++i) {
            const row = reviewRowsId.get(i)
            if (row.status === ExternalSourceStatusModel.Changed) {
                root.rememberDismissal(row.ownerKey, row.sourceRevision)
            }
        }
        root.refresh()
    }

    QQ.Component.onCompleted: root.refresh()

    QQ.Connections {
        target: root.statusModel

        function onStatusesChanged() {
            root.refresh()
        }
    }

    // A cave or trip rename rewrites the attached rows without touching a
    // status, so an open review list follows the names from here.
    QQ.Connections {
        target: root.externalCenterlineManager.attachedCenterlinesModel

        function onDataChanged() {
            root.refresh()
        }
    }

    ColumnLayout {
        id: contentColumnId

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Theme.tightSpacing

        ExternalSourceChangeBanner {
            id: bannerId

            Layout.fillWidth: true
            changedCount: root.changedCount

            onUpdateAllRequested: root.externalCenterlineManager.updateAllChangedSources()
            onShowRequested: root.listShown = !root.listShown
            onDismissRequested: root.dismiss()
        }

        ExternalSourceChangeList {
            id: reviewListId

            Layout.fillWidth: true
            visible: root.listShown
            rows: reviewRowsId

            // The key is the owner id spelled as a string, which the verb
            // parses back into the id it wants.
            onUpdateRequested: (ownerKey) =>
                root.externalCenterlineManager.updateFromSource(ownerKey)
        }
    }

    // What the review list shows, rebuilt from the manager on every status
    // sweep so an open list follows the sources live.
    QQ.ListModel {
        id: reviewRowsId
    }

    // The revision each owner's source had when the user last waved it off.
    QQ.ListModel {
        id: dismissedRevisionsId
    }
}
