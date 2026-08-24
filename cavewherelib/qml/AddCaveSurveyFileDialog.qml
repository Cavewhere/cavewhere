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

// Add-cave-from-survey-file dialog (Phase 3 §3.1). Pick an entry file,
// read the scan's verdict and its block tree - the trips the attach
// will create - then Attach. There is no Import choice here: whole-file
// native import is the existing Import Survex feature, and the cave
// dialog has exactly one verb.
//
// The dialog owns no attach state of its own beyond the session, which
// attributes the manager's shared attachCompleted report back to this
// surface. A failed attach fires neither outcome signal: the error
// shows inline and the dialog stays open for another attempt.
QQ.Item {
    id: root
    objectName: "addCaveSurveyFileDialog"

    property Cave cave: null

    property alias title: dialogId.title

    // True from the Attach click until the session's report lands.
    readonly property bool busy: sessionId.busy

    // Outcome signals for hosts. Exactly one fires per open()d session
    // that ends: attached() when this dialog's attach succeeds,
    // dismissed() when the user backs out (idle Cancel, or a canceled
    // attach). The programmatic close() is silent so hosts and tests
    // can reset state without triggering cleanup.
    signal attached()
    signal dismissed()

    function open() {
        pickerId.clear()
        sessionId.reset()
        dialogId.open()
    }

    function close() {
        dialogId.close()
    }

    ExternalCenterlineAttachSession {
        id: sessionId

        onSucceeded: {
            dialogId.close()
            root.attached()
        }

        onCanceled: {
            dialogId.close()
            root.dismissed()
        }
    }

    QC.Dialog {
        id: dialogId
        objectName: "addCaveAttachDialog"

        readonly property QQ.Item overlayItem: QC.Overlay.overlay
        readonly property bool compact: overlayItem !== null
                                        && overlayItem.width < Theme.breakpointPanelCollapse
        readonly property int wideWidth: 640
        readonly property int compactWidth: 400

        readonly property int blockIndentWidth: Theme.actionBarSpacing

        anchors.centerIn: QC.Overlay.overlay
        modal: true
        implicitWidth: {
            let target = compact ? compactWidth : wideWidth
            return overlayItem !== null
                    ? Math.min(target, overlayItem.width - 2 * Theme.actionBarSpacing)
                    : target
        }
        title: qsTr("Add cave from survey file")
        closePolicy: QC.Popup.NoAutoClose

        contentItem: ColumnLayout {
            spacing: Theme.tightSpacing

            ExternalCenterlineFilePicker {
                id: pickerId
                Layout.fillWidth: true
                promptText: qsTr("Pick the entry file:")
                fileDialogTitle: dialogId.title
                locked: root.busy
                operationError: sessionId.errorMessage
                onPathEdited: sessionId.errorMessage = ""
            }

            BodyText {
                objectName: "attachExplainerText"
                Layout.fillWidth: true
                Layout.topMargin: Theme.sectionSpacing
                wrapMode: QC.Label.WordWrap
                text: qsTr("The file stays the source of truth — keep "
                         + "editing it in your survey tool and CaveWhere "
                         + "stays in sync. A copy travels with your "
                         + "project for git sync, backups, and sharing; "
                         + "CaveWhere never writes to your original "
                         + "files. Survey data is read-only in "
                         + "CaveWhere.")
            }

            // Disclosure, not a picker: auto-creation is silent, so this
            // is where the user sees what the attach is about to make.
            ColumnLayout {
                objectName: "blockPreview"
                Layout.fillWidth: true
                Layout.topMargin: Theme.sectionSpacing
                spacing: Theme.tightSpacing
                visible: pickerId.valid && pickerId.blocks.length > 0

                QC.Label {
                    text: qsTr("Trips CaveWhere will create")
                    font.bold: true
                }

                // The tree scrolls once it outgrows the shared list cap,
                // so a whole survey project's blocks never push the
                // buttons off the screen.
                QQ.ListView {
                    objectName: "blockPreviewList"
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(contentHeight,
                                                     Theme.floatingListMaxHeight)
                    clip: true
                    model: pickerId.blocks

                    delegate: QC.Label {
                        id: blockRowId
                        objectName: "blockRow"

                        required property cwScanBlock modelData

                        // A block with no stations of its own makes no
                        // trip (§5 Q3) - shown so the structure reads
                        // whole, grayed so it reads as skipped.
                        readonly property bool makesTrip: blockRowId.modelData.stationCount > 0

                        width: QQ.ListView.view.width
                        leftPadding: blockRowId.modelData.depth * dialogId.blockIndentWidth
                        elide: QC.Label.ElideRight
                        color: blockRowId.makesTrip ? Theme.text : Theme.textSubtle
                        text: qsTr("%1 — %n station(s)", "", blockRowId.modelData.stationCount)
                                .arg(blockRowId.modelData.name)
                    }
                }

                QC.Label {
                    objectName: "blockPreviewHint"
                    Layout.fillWidth: true
                    wrapMode: QC.Label.WordWrap
                    color: Theme.textSubtle
                    font.pixelSize: Theme.fontSizeSmall
                    text: qsTr("One trip per *begin block with stations. "
                             + "Rename or delete any of them afterward.")
                }
            }

            // Guidance, not refusal (§5 Q4): a two-entrance system
            // legitimately looks like this, so Attach stays enabled.
            ColumnLayout {
                objectName: "multiCaveHeuristicPanel"
                Layout.fillWidth: true
                Layout.topMargin: Theme.sectionSpacing
                spacing: Theme.tightSpacing
                visible: pickerId.valid && pickerId.topLevelBlockCount >= 2
                         && !pickerId.entryHasOwnShots

                QC.Label {
                    Layout.fillWidth: true
                    wrapMode: QC.Label.WordWrap
                    font.bold: true
                    color: Theme.warning
                    text: qsTr("This file looks like a whole survey project, not one cave")
                }

                BodyText {
                    objectName: "multiCaveHeuristicBody"
                    Layout.fillWidth: true
                    wrapMode: QC.Label.WordWrap
                    text: qsTr("It holds %n top-level survey block(s) and no "
                             + "survey data of its own. Attaching it makes a "
                             + "single cave that contains all of them. To make "
                             + "one cave per survey, attach that survey's own "
                             + "file instead:", "", pickerId.topLevelBlockCount)
                }

                QQ.Repeater {
                    objectName: "heuristicSuggestions"
                    model: pickerId.entryDirectIncludes

                    delegate: LinkText {
                        id: suggestionId
                        objectName: "heuristicSuggestion"

                        required property string modelData

                        Layout.leftMargin: Theme.sectionSpacing
                        text: RootData.fileName(suggestionId.modelData)
                        onClicked: pickerId.setPath(suggestionId.modelData)
                    }
                }

                QC.Label {
                    Layout.fillWidth: true
                    wrapMode: QC.Label.WordWrap
                    color: Theme.textSubtle
                    font.pixelSize: Theme.fontSizeSmall
                    text: qsTr("Splitting one file into several caves arrives "
                             + "with region-level attach.")
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.sectionSpacing
                spacing: Theme.tightSpacing
                visible: root.busy

                QC.BusyIndicator {
                    implicitWidth: Theme.fontSizeLarge
                    implicitHeight: Theme.fontSizeLarge
                    running: root.busy
                }

                QC.Label {
                    objectName: "busyLabel"
                    text: qsTr("Attaching…")
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.sectionSpacing
                spacing: Theme.flowSpacing

                QQ.Item {
                    Layout.fillWidth: true
                }

                QC.Button {
                    objectName: "cancelButton"
                    text: qsTr("Cancel")
                    onClicked: {
                        if (root.busy) {
                            sessionId.cancel()
                        } else {
                            dialogId.close()
                            root.dismissed()
                        }
                    }
                }

                QC.Button {
                    objectName: "attachButton"
                    text: qsTr("Attach")
                    enabled: root.cave !== null && pickerId.valid && !root.busy
                    onClicked: {
                        sessionId.start(root.cave)
                        RootData.attachCaveCenterline(root.cave, pickerId.sourcePath)
                    }
                }
            }
        }
    }
}
