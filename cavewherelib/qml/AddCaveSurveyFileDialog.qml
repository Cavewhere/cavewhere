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
// read the scan's verdict and the file's tree of books, files, blocks,
// and surveys, then choose how CaveWhere uses the file, the same two
// choices the trip dialog offers: Attach (recommended - the file stays
// the source) or Import a copy (Survex only - the
// data lands in a native trip of the cave, editable here).
//
// The dialog owns no attach state of its own beyond the session, which
// attributes the manager's shared attachCompleted report back to this
// surface. A failed attach fires no outcome signal: the error shows
// inline and the dialog stays open for another attempt.
QQ.Item {
    id: root
    objectName: "addCaveSurveyFileDialog"

    property Cave cave: null

    // Import adds the trip it fills, and importFinished names that trip,
    // so the dialog keeps it to recognize its own report.
    property bool importing: false
    property Trip importingTrip: null

    property alias title: dialogId.title

    // True from the Attach click until the session's report lands.
    readonly property bool attaching: sessionId.busy
    readonly property bool busy: root.attaching || root.importing

    // Outcome signals for hosts. Exactly one fires per open()d session
    // that ends: attached() when this dialog's attach succeeds,
    // imported(trip, sourcePath) when its import completes (the cave keeps
    // no record of the file on this path, so the filled trip and the picked
    // path ride the signal for naming), dismissed() when the user backs out (idle
    // Cancel, or a canceled attach). The programmatic close() is silent
    // so hosts and tests can reset state without triggering cleanup.
    signal attached()
    signal imported(trip: Trip, sourcePath: string)
    signal dismissed()

    function open() {
        pickerId.clear()
        sessionId.reset()
        root.importing = false
        root.importingTrip = null
        dialogId.open()
    }

    // Import a copy: the parsed shots go into a new trip of the cave.
    function startImport() {
        sessionId.errorMessage = ""
        root.cave.addTrip()
        root.importingTrip = root.cave.trip(root.cave.tripCount - 1)
        root.importing = true
        RootData.surveyImportManager.importSurvexToTrip(pickerId.sourcePath, root.importingTrip)
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

    QQ.Connections {
        target: RootData.surveyImportManager

        function onImportFinished(trip, hasErrors) {
            if (root.importing && trip === root.importingTrip) {
                const importedTrip = root.importingTrip
                root.importing = false
                root.importingTrip = null
                // Parse warnings live in the global error model; the import
                // lands whatever parsed, so the session is over either way.
                dialogId.close()
                root.imported(importedTrip, pickerId.sourcePath)
            }
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

            // Disclosure, not a picker: auto-creation is silent, so this
            // is where the user sees what the attach is about to make.
            ColumnLayout {
                objectName: "blockPreview"
                Layout.fillWidth: true
                Layout.topMargin: Theme.sectionSpacing
                spacing: Theme.tightSpacing
                visible: pickerId.valid && pickerId.rows.length > 0

                QC.Label {
                    text: qsTr("What the file holds")
                    font.bold: true
                }

                // The stations no block claims - every station of a
                // Compass or prefix-less Walls file - get one
                // whole-file trip, listed first because it stands
                // above the block tree.
                QC.Label {
                    objectName: "wholeFileRow"
                    Layout.fillWidth: true
                    visible: pickerId.rootStationCount > 0
                    elide: QC.Label.ElideRight
                    text: qsTr("Whole file — %n station(s)", "",
                               pickerId.rootStationCount)
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
                    model: pickerId.rows

                    delegate: QC.Label {
                        id: blockRowId
                        objectName: "blockRow"

                        required property cwScanRow modelData

                        // A row with no survey data under it, or a file the
                        // source lists and the disk lacks, is shown so the
                        // structure reads whole, grayed so it reads as skipped.
                        readonly property bool grayed: !blockRowId.modelData.hasSurveyContent
                                                       || blockRowId.modelData.missing
                        readonly property string kindText: pickerId.kindLabel(blockRowId.modelData)

                        width: QQ.ListView.view.width
                        leftPadding: blockRowId.modelData.depth * dialogId.blockIndentWidth
                        elide: QC.Label.ElideRight
                        color: blockRowId.grayed ? Theme.textSubtle : Theme.text
                        text: blockRowId.modelData.missing
                              ? qsTr("%1 · %2 — missing")
                                    .arg(blockRowId.modelData.displayName)
                                    .arg(blockRowId.kindText)
                              : qsTr("%1 · %2 — %n station(s)", "",
                                     blockRowId.modelData.stationCount)
                                    .arg(blockRowId.modelData.displayName)
                                    .arg(blockRowId.kindText)
                    }
                }

                QC.Label {
                    objectName: "blockPreviewHint"
                    Layout.fillWidth: true
                    wrapMode: QC.Label.WordWrap
                    color: Theme.textSubtle
                    font.pixelSize: Theme.fontSizeSmall
                    text: qsTr("One trip per survey block with stations, plus "
                             + "one for the stations no block claims. "
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

            QC.Label {
                Layout.topMargin: Theme.sectionSpacing
                text: qsTr("How should CaveWhere use this file?")
                font.bold: true
            }

            GridLayout {
                Layout.fillWidth: true
                columns: dialogId.compact ? 1 : 2
                columnSpacing: Theme.flowSpacing
                rowSpacing: Theme.flowSpacing

                QC.GroupBox {
                    Layout.fillWidth: true
                    // Equalize heights only side by side; stacked boxes
                    // keep their natural height.
                    Layout.fillHeight: !dialogId.compact
                    // Equal columns: let the grid split the width instead
                    // of following each box's implicit (unwrapped) width.
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop

                    contentItem: ColumnLayout {
                        spacing: Theme.tightSpacing

                        BodyText {
                            objectName: "attachExplainerText"
                            Layout.fillWidth: true
                            wrapMode: QC.Label.WordWrap
                            text: qsTr("The file stays the source of truth — keep "
                                     + "editing it in your survey tool and CaveWhere "
                                     + "stays in sync. A copy travels with your "
                                     + "project for git sync, backups, and sharing; "
                                     + "CaveWhere never writes to your original "
                                     + "files. Survey data is read-only in "
                                     + "CaveWhere.")
                        }

                        QQ.Item { Layout.fillHeight: true }

                        RowLayout {
                            Layout.alignment: Qt.AlignRight
                            Layout.topMargin: Theme.sectionSpacing
                            spacing: Theme.flowSpacing

                            QC.Label {
                                objectName: "recommendedLabel"
                                text: qsTr("Recommended")
                                font.bold: true
                                color: Theme.accent
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

                QC.GroupBox {
                    Layout.fillWidth: true
                    Layout.fillHeight: !dialogId.compact
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop

                    contentItem: ColumnLayout {
                        spacing: Theme.tightSpacing

                        BodyText {
                            objectName: "importExplainerText"
                            Layout.fillWidth: true
                            wrapMode: QC.Label.WordWrap
                            text: qsTr("Copies the data into a trip of this cave. "
                                     + "You edit it here; the original file is "
                                     + "never read again.")
                        }

                        QC.Label {
                            objectName: "importFormatNote"
                            Layout.fillWidth: true
                            visible: pickerId.valid && !pickerId.importSupported
                            wrapMode: QC.Label.WordWrap
                            color: Theme.textSubtle
                            text: qsTr("Import supports Survex files only.")
                        }

                        QQ.Item { Layout.fillHeight: true }

                        QC.Button {
                            objectName: "importButton"
                            Layout.alignment: Qt.AlignRight
                            Layout.topMargin: Theme.sectionSpacing
                            text: qsTr("Import a copy")
                            enabled: root.cave !== null && pickerId.valid
                                     && !root.busy && pickerId.importSupported
                            onClicked: root.startImport()
                        }
                    }
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
                    text: root.importing ? qsTr("Importing…") : qsTr("Attaching…")
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
                    // Import has no way back once it starts.
                    enabled: !root.importing
                    onClicked: {
                        if (root.attaching) {
                            sessionId.cancel()
                        } else {
                            dialogId.close()
                            root.dismissed()
                        }
                    }
                }
            }
        }
    }
}
