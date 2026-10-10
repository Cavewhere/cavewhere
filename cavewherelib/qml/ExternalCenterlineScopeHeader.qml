/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

// Header for a derived trip: one unit of its source root's survey file,
// or the root's whole-node window (plans/SURVEY_TREE_PLAN.html §7.2). The
// trip owns no file — its source root does — so this header names the
// node it belongs to and the one destructive verb the trip still answers
// to: Remove trip.
QQ.Item {
    id: root
    objectName: "scopeHeader"

    property Trip trip: null

    readonly property Cave cave: trip !== null ? trip.parentCave : null

    // cwLinkGenerator owns the page-address scheme, so this leaf doesn't
    // hardcode the page tree.
    function gotoCave(targetCave) {
        if(targetCave !== null) {
            RootData.pageSelectionModel.currentPageAddress =
                    linkGeneratorId.caveLink(targetCave)
        }
    }

    implicitWidth: contentColumnId.implicitWidth
    // The prompt hangs below the column, and the header has to own that
    // room: an item drawn outside its parent's bounds inside a scrolling
    // viewport is drawn but never clicked.
    implicitHeight: contentColumnId.implicitHeight
                    + (removeTripChallengeId.visible
                       ? Theme.tightSpacing + removeTripChallengeId.height : 0)

    ColumnLayout {
        id: contentColumnId

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: Theme.tightSpacing

        RowLayout {
            objectName: "scopeHeaderParentRow"

            Layout.fillWidth: true
            spacing: Theme.tightSpacing

            QC.Label {
                objectName: "partOfLabel"
                font.bold: true
                text: qsTr("Part of:")
            }

            // The link hugs its text — LinkText makes its whole label
            // clickable, so a stretched one would turn the blank space
            // beside a short cave name into a navigation trap. The
            // spacer takes the slack instead, and elide covers a name
            // too long for the row.
            LinkText {
                objectName: "parentCaveLink"

                elide: QC.Label.ElideMiddle
                text: root.cave !== null ? root.cave.name : ""

                onClicked: root.gotoCave(root.cave)
            }

            QQ.Item { Layout.fillWidth: true }
        }

        QC.Button {
            id: removeTripButtonId
            objectName: "removeTripButton"

            Layout.alignment: Qt.AlignRight

            text: qsTr("Remove trip…")

            onClicked: removeTripChallengeId.show()
        }
    }

    LinkGenerator {
        id: linkGeneratorId
        pageSelectionModel: RootData.pageSelectionModel
    }

    // Sits outside the layout: a prompt that only appears on demand must
    // never take a row of its own in the column.
    RemoveAskBox {
        id: removeTripChallengeId
        objectName: "removeTripChallenge"

        removeName: root.trip !== null ? root.trip.name : ""

        // Hangs under the button it belongs to, pulled back inside the
        // header so a long trip name stays on the screen. A binding,
        // because the box only reaches its full width once its message
        // is laid out.
        x: Math.max(0, Math.min(removeTripButtonId.x + removeTripButtonId.width - width,
                                root.width - width))
        y: removeTripButtonId.y + removeTripButtonId.height + Theme.tightSpacing

        onRemove: {
            // removeTrip destroys the trip on the spot when there is no
            // undo stack, so everything this handler needs is read first,
            // and the page showing the trip is left before it goes.
            const cave = root.cave
            if(cave === null) {
                return
            }

            const index = cave.indexOf(root.trip)
            const caveAddress = linkGeneratorId.caveLink(cave)

            // Leaving the trip page destroys this header and the prompt
            // that is still running, so the destructive half waits for
            // the next pass through the event loop and touches only the
            // cave and the page model, which both outlive the header.
            Qt.callLater(() => {
                RootData.pageSelectionModel.currentPageAddress = caveAddress
                cave.removeTrip(index)
            })
        }
    }
}
