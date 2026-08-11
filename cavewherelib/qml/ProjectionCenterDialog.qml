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
import "Utils.js" as Utils

// Chooses what the project's map projection is centered on: the middle of the
// project's data, or one of its fix stations.
//
// Choosing and doing it are two steps. Centering re-projects every coordinate in
// the project and reloads its point clouds, which is more than a stray click on
// a list row should be able to start, so a row only selects and the Center
// button commits.
//
// The rows are built as the dialog opens rather than kept current, because
// deciding whether a station is a sane place to center reprojects every
// georeferenced input in the project — work worth doing while the list is on
// screen and nowhere else.
QC.Dialog {
    id: rootId
    objectName: "projectionCenterDialog"

    // Wide enough for a cave and a station on one line above a latitude and
    // longitude, and tall enough that a cave's worth of stations scrolls rather
    // than pushing the buttons off the window.
    readonly property int dialogWidth: 420
    readonly property int candidateListHeight: 260

    // Six decimals of a degree is about 10 cm on the ground, finer than any fix
    // station is known to, and the precision the Location row reads in.
    readonly property int coordinatePrecision: 6

    // The reach is a distance in meters and reads as kilometers.
    readonly property int metersPerKilometer: 1000
    readonly property int reachPrecision: 1

    // What the Center button acts on: a fix station's id, dataCenterId for the
    // middle of the data, or the empty string while nothing is chosen. A
    // station's id is a UUID, so it can never collide with the sentinel.
    readonly property string dataCenterId: "dataCenter"
    property string selectedId: ""

    // Set when the projection turns the choice down, which means the project
    // changed between the rows being drawn and the click.
    property bool choiceWentStale: false

    readonly property LocalProjectionManager localProjection: RootData.region.localProjection
    readonly property RecenterCandidateModel candidates: rootId.localProjection.recenterCandidates

    // The two reasons a row can't be picked, worded once so the station rows and
    // the middle-of-the-data row can't drift into saying them differently. The
    // reach comes from the projection rather than being restated here, so the
    // number the user reads is the one eligibility is judged on.
    readonly property string currentCenterText: qsTr("Projection's center")
    readonly property string outOfReachText:
        qsTr("More than %1 km from the middle of your data")
            .arg(Utils.fixed(rootId.localProjection.anchorThresholdMeters
                             / rootId.metersPerKilometer, rootId.reachPrecision))

    anchors.centerIn: QC.Overlay.overlay
    modal: true
    implicitWidth: rootId.dialogWidth
    title: qsTr("Recenter the projection")

    function formatCoordinate(latitude: real, longitude: real) : string {
        return Utils.formatLatLon(latitude, longitude, rootId.coordinatePrecision)
    }

    // The two things a row's second line can say: where it is, and why it can't
    // be picked. Either may be empty.
    function detailLine(place: string, reason: string) : string {
        return [place, reason].filter(part => part !== "").join(" · ")
    }

    // Only a choice the projection took closes the picker. It refuses a station
    // the project has since moved or lost, and a refusal that closed would read
    // exactly like a recentering that worked, so it stays open and redraws the
    // rows against the project as it now stands.
    function commit() {
        const moved = rootId.selectedId === rootId.dataCenterId
                    ? rootId.localProjection.recenterOnDataCenter()
                    : rootId.localProjection.recenterOnStation(rootId.selectedId)
        if (moved) {
            rootId.accept()
            return
        }

        rootId.choiceWentStale = true
        rootId.selectedId = ""
        rootId.candidates.refresh()
    }

    // One thing the projection could be centered on. The two kinds of row read
    // the same way — what it is, then where it is — because they are the same
    // choice made two ways.
    component CenterOption : QC.ItemDelegate {
        id: optionId

        // The first line, as styled text so a cave name can carry the weight.
        required property string title
        // What selecting this row puts in selectedId.
        required property string value
        // The second line: where it is, and why it can't be picked.
        property string detail: ""

        highlighted: rootId.selectedId === optionId.value

        onClicked: rootId.selectedId = optionId.value

        contentItem: ColumnLayout {
            spacing: Theme.tightSpacing

            QC.Label {
                Layout.fillWidth: true
                textFormat: QC.Label.StyledText
                text: optionId.title
                elide: QC.Label.ElideRight
            }

            // Where the row sits, and the reason a row is grayed out, are
            // written under it rather than put in a tooltip: a tooltip is
            // mouse-only, and this list is used on a touch screen.
            QC.Label {
                objectName: "centerOptionDetail"

                Layout.fillWidth: true
                visible: optionId.detail !== ""

                text: optionId.detail
                color: Theme.textSubtle
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: QC.Label.WordWrap
            }
        }
    }

    // Before it is shown rather than after: a row that arrives once the dialog
    // is on screen is laid out against a list that has already claimed its
    // space, and ends up underneath it.
    onAboutToShow: {
        rootId.selectedId = ""
        rootId.choiceWentStale = false
        rootId.candidates.refresh()
    }

    footer: QC.DialogButtonBox {
        // ApplyRole rather than AcceptRole: a button box accepts by closing the
        // dialog, and commit() has to be able to keep it open.
        onApplied: rootId.commit()

        QC.Button {
            objectName: "projectionCenterAcceptButton"

            text: qsTr("Center")
            enabled: rootId.selectedId !== ""

            QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.ApplyRole
        }

        QC.Button {
            objectName: "projectionCenterCancelButton"

            text: qsTr("Cancel")

            QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.RejectRole
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.sectionSpacing

        BodyText {
            Layout.fillWidth: true
            wrapMode: QC.Label.WordWrap
            text: qsTr("Recentering moves the origin of this project's map projection. "
                     + "Your coordinates are re-projected onto a new center and reloaded.")
        }

        QC.Label {
            objectName: "projectionCenterStaleWarning"

            Layout.fillWidth: true
            visible: rootId.choiceWentStale

            text: qsTr("The project changed while this was open, so that choice no "
                     + "longer works. These rows are up to date.")
            color: Theme.warningText
            wrapMode: QC.Label.WordWrap
        }

        QC.Frame {
            Layout.fillWidth: true
            Layout.preferredHeight: rootId.candidateListHeight
            padding: 0

            ColumnLayout {
                // Inset from the frame, so a row's highlight stops short of the
                // border instead of painting over it.
                anchors.fill: parent
                anchors.margins: Theme.delegatePadding
                spacing: 0

                // The middle of the data stays above the stations rather than
                // scrolling with them: it is the choice that fits most projects,
                // and a long list would carry it off screen.
                CenterOption {
                    objectName: "recenterDataCenterRow"

                    Layout.fillWidth: true

                    // The row keeps its place whether or not there is a middle
                    // to offer, and whether or not the frame is already on it.
                    // A row that appears once the dialog is already laid out
                    // lands underneath the list below it.
                    enabled: rootId.candidates.hasDataCenter
                             && !rootId.candidates.dataCenterIsCurrent
                    value: rootId.dataCenterId
                    title: qsTr("<b>Middle of your data</b>")
                    detail: rootId.detailLine(
                                rootId.candidates.hasDataCenter
                                ? rootId.formatCoordinate(rootId.candidates.dataCenterLatitude,
                                                          rootId.candidates.dataCenterLongitude)
                                : "",
                                rootId.candidates.dataCenterIsCurrent
                                ? rootId.currentCenterText
                                : "")
                }

                QC.MenuSeparator {
                    Layout.fillWidth: true
                }

                QQ.ListView {
                    id: candidateListId
                    objectName: "recenterCandidateList"

                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    clip: true
                    model: rootId.candidates

                    delegate: CenterOption {
                        id: candidateId

                        required property string stationId
                        required property string stationName
                        required property string caveName
                        required property real latitude
                        required property real longitude
                        required property bool hasCoordinate
                        required property bool eligible
                        required property bool isCurrent

                        objectName: "recenterCandidate " + candidateId.stationName

                        width: candidateListId.width
                        // The station the frame already sits on stays visible and
                        // says so: its absence would read as the project having
                        // lost it.
                        enabled: candidateId.eligible && !candidateId.isCurrent

                        value: candidateId.stationId
                        // The cave carries the row: a station name means nothing
                        // on its own, and half the projects here have an
                        // "entrance" in every cave.
                        title: qsTr("<b>%1</b> at %2").arg(candidateId.caveName)
                                                      .arg(candidateId.stationName)

                        detail: rootId.detailLine(
                                    candidateId.hasCoordinate
                                    ? rootId.formatCoordinate(candidateId.latitude,
                                                              candidateId.longitude)
                                    : "",
                                    candidateId.isCurrent
                                    ? rootId.currentCenterText
                                    : (candidateId.eligible ? "" : rootId.outOfReachText))
                    }
                }
            }
        }
    }
}
