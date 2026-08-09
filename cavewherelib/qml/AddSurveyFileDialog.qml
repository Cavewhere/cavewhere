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

// Unified add-from-survey-file dialog (Add Trip UX plan §3). Pick an
// entry file, preview the scan (auto-detected format, file count,
// warnings, errors), then choose how CaveWhere uses it: Attach
// (recommended - the file stays the source of truth) or Import a copy
// (Survex only). Attach runs through the manager's completion-signal
// bridge; Import through cwSurveyImportManager's importFinished
// bridge. Cancel during an attach routes through cancelAttach, which
// the manager honors only until the attach's internal scan lands; a
// later click is a structural no-op, so the button can stay enabled
// for the whole operation without the q14 token-release hazard.
// Import has no cancel path, so Cancel disables while importing.
QQ.Item {
    id: root
    objectName: "attachExternalCenterlineDialog"

    property Trip trip: null

    // Import keeps its own owner snapshot, the way the attach session
    // keeps one: importFinished carries the trip pointer, so
    // identity-compare against the snapshot (a trip
    // destroyed mid-import nulls both sides, which still matches and
    // unwedges the dialog). The source path needs no snapshot - the
    // picker is locked while busy, so its sourcePath is stable until
    // the import reports.
    property bool importing: false
    property Trip importingTrip: null

    property alias title: dialogId.title

    // True from the Attach click until the session's report lands.
    readonly property bool attaching: sessionId.busy
    readonly property bool busy: attaching || importing

    // Outcome signals for hosts. Exactly one fires per open()d session
    // that ends: attached() when this dialog's attach succeeds,
    // imported(sourcePath) when its import completes (the trip carries
    // no entry-file record on this path, so the picked path rides the
    // signal for naming), dismissed() when the user backs out (idle
    // Cancel, or a canceled attach). A failed attach fires neither -
    // the dialog stays open for another attempt. The programmatic
    // close() is silent so hosts and tests can reset state without
    // triggering cleanup.
    signal attached()
    signal imported(string sourcePath)
    signal dismissed()

    function open() {
        pickerId.clear()
        sessionId.reset()
        root.importing = false
        root.importingTrip = null
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

    QQ.Connections {
        target: RootData.surveyImportManager

        function onImportFinished(trip, hasErrors) {
            if (!root.importing || trip !== root.importingTrip) {
                return
            }
            root.importing = false
            root.importingTrip = null
            // Parse warnings live in the global error model (import
            // still lands whatever parsed); the dialog's session is
            // over either way.
            dialogId.close()
            root.imported(pickerId.sourcePath)
        }
    }

    QC.Dialog {
        id: dialogId
        objectName: "attachDialog"

        // The two choice group boxes sit side by side when the window
        // has room and stack when it doesn't.
        readonly property QQ.Item overlayItem: QC.Overlay.overlay
        readonly property bool compact: overlayItem !== null
                                        && overlayItem.width < Theme.breakpointPanelCollapse
        readonly property int wideWidth: 640
        readonly property int compactWidth: 400

        anchors.centerIn: QC.Overlay.overlay
        modal: true
        implicitWidth: {
            let target = compact ? compactWidth : wideWidth
            return overlayItem !== null
                    ? Math.min(target, overlayItem.width - 2 * Theme.actionBarSpacing)
                    : target
        }
        title: qsTr("Add trip from survey file")
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

                        // The filler above holds the choice button at the
                        // bottom right, where dialog choice buttons live;
                        // stacked, it collapses and the margin makes the gap.
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
                                enabled: root.trip !== null && pickerId.valid
                                         && !root.busy
                                onClicked: {
                                    sessionId.start(root.trip)
                                    RootData.attachTripCenterline(root.trip,
                                                                  pickerId.sourcePath)
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
                            text: qsTr("Copies the data into CaveWhere. You edit "
                                     + "it here; the original file is never read "
                                     + "again.")
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
                            enabled: root.trip !== null && pickerId.valid
                                     && !root.busy && pickerId.importSupported
                            onClicked: {
                                sessionId.errorMessage = ""
                                root.importingTrip = root.trip
                                root.importing = true
                                RootData.surveyImportManager.importSurvexToTrip(
                                    pickerId.sourcePath, root.importingTrip)
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
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
                spacing: Theme.flowSpacing

                QQ.Item {
                    Layout.fillWidth: true
                }

                QC.Button {
                    id: cancelButtonId
                    objectName: "cancelButton"
                    text: qsTr("Cancel")
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
