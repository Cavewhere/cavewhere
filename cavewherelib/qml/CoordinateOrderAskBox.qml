/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

// Asks which of a coordinate's first two numbers came first, at the one moment
// the answer is still knowable.
//
// A fix station with no coordinate system has no axis order, and nothing
// recorded the one its text was written in — an svx `*fix` with no `*cs` before
// it is spelled easting first, but the file never says so. Naming a *projected*
// system reads all three numbers straight back; naming a *geographic* one reads
// them latitude first, and if that isn't what they were, the first two have
// silently traded places. There is nothing to detect afterwards: both readings
// are the same two numbers, and either can be perfectly plausible.
//
// So the choice is put to the user while they still have the context to answer
// it, and the swap is offered as their own text with its first two numbers
// exchanged (cwCoordinateText::swapHorizontal) — not as a re-render, so an
// elevation, its unit, and a coordinate that never had one all survive.
//
// The host commits the coordinate system, calls askAbout(), and applies
// swapRequested() to the row. Whether there is anything to ask is decided here
// rather than by the host — the page's two coordinate-system cells already
// funnel through one commit, so a rule left to the hosts would be written twice
// and could drift.
QC.Dialog {
    id: rootId
    objectName: "coordinateOrderAskBox"

    // What the open question is about: the row, its coordinate as stored, and
    // the system just named for it. All three are set by askAbout(), together,
    // so an answer can't be applied to a row other than the one asked about.
    property int row: -1
    property string coordinate: ""
    property string coordinateSystem: ""

    // The same text with its first two numbers exchanged. Empty when there is
    // nothing to exchange, which is what keeps the dialog shut.
    readonly property string swapped: CoordinateText.swapHorizontal(rootId.coordinate)

    readonly property string coordinateSystemName: CSFormat.hasName(rootId.coordinateSystem)
                                                   ? CSFormat.displayName(rootId.coordinateSystem)
                                                   : rootId.coordinateSystem

    anchors.centerIn: QC.Overlay.overlay
    modal: true
    // Fixed rather than derived from the message: a wrapping label sized to its
    // own container is a binding loop through Dialog's implicitWidth.
    implicitWidth: Theme.inlineBannerWidth
    title: qsTr("Which way round is this coordinate?")
    // Escape leaves the coordinate as written, which is the reading already
    // committed — the dialog can be dismissed without deciding anything.
    closePolicy: QC.Popup.CloseOnEscape

    signal swapRequested(int row, string swappedCoordinate)

    // Ask, but only when there is something to ask: a row whose text had no
    // recorded order, now being read latitude first. Every other case has an
    // order the row itself can account for, and asking would be noise. Deciding
    // that here rather than at each call site is deliberate — a rule copied into
    // each host is a rule they can drift apart on.
    //
    // \a orderWasUnknown is CoordinateOrderUnknownRole read *before* the system
    // was committed — committing is what makes the order known, so by the time
    // this is called the row no longer remembers it was ever in doubt.
    function askAbout(row: int, coordinateText: string, newCS: string,
                      orderWasUnknown: bool): void {
        if (!orderWasUnknown
                || CoordinateText.axisOrderFor(newCS) !== CoordinateText.LatitudeLongitude) {
            return
        }

        // Empty when the text doesn't read as a coordinate under either order.
        // Such a row is Unreadable now and says so on its own; there is no
        // arrangement of it to offer.
        //
        // Asked of the argument rather than of rootId.swapped, so that every
        // reason to stay shut is settled before anything is written: a call that
        // decides not to ask must leave a question already on screen pointing at
        // the row it was asked about.
        if (CoordinateText.swapHorizontal(coordinateText) === "") {
            return
        }

        rootId.row = row
        rootId.coordinate = coordinateText
        rootId.coordinateSystem = newCS
        rootId.open()
    }

    contentItem: ColumnLayout {
        spacing: Theme.sectionSpacing

        BodyText {
            objectName: "coordinateOrderAskMessage"
            Layout.fillWidth: true
            wrapMode: QC.Label.WordWrap
            text: qsTr("This fix had no coordinate system, so nothing recorded which of "
                       + "its numbers came first. %1 is written latitude first, so the "
                       + "coordinate now reads:").arg(rootId.coordinateSystemName)
        }

        // The two readings wrap rather than run off the dialog: comparing them
        // is the whole reason this is on screen, so a coordinate long enough to
        // overflow would defeat the question instead of merely looking untidy.
        // WrapAnywhere because a coordinate has few places a word wrap could
        // break, and a mid-number break is no worse than a clipped one.
        GridLayout {
            columns: 2
            Layout.fillWidth: true
            columnSpacing: Theme.columnGap
            rowSpacing: Theme.tightSpacing

            QC.Label {
                text: qsTr("As written")
                color: Theme.textSubtle
                Layout.alignment: Qt.AlignTop
            }

            QC.Label {
                objectName: "coordinateOrderAskAsWritten"
                Layout.fillWidth: true
                font.family: Theme.fontFamilyMono
                wrapMode: QC.Label.WrapAnywhere
                text: rootId.coordinate
            }

            QC.Label {
                text: qsTr("Swapped")
                color: Theme.textSubtle
                Layout.alignment: Qt.AlignTop
            }

            QC.Label {
                objectName: "coordinateOrderAskSwapped"
                Layout.fillWidth: true
                font.family: Theme.fontFamilyMono
                wrapMode: QC.Label.WrapAnywhere
                text: rootId.swapped
            }
        }
    }

    footer: QQ.Item {
        implicitHeight: footerRowId.implicitHeight + Theme.statsPadding * 2

        RowLayout {
            id: footerRowId
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: Theme.statsPadding
            anchors.rightMargin: Theme.statsPadding
            spacing: Theme.flowSpacing

            QQ.Item { Layout.fillWidth: true }

            QC.Button {
                objectName: "coordinateOrderKeepButton"
                text: qsTr("Keep as written")
                onClicked: rootId.close()
            }

            QC.Button {
                objectName: "coordinateOrderSwapButton"
                text: qsTr("Swap")
                onClicked: {
                    rootId.swapRequested(rootId.row, rootId.swapped)
                    rootId.close()
                }
            }
        }
    }
}
