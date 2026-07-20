/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Dialogs as QD
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

    // True from the Attach click until the bridge reports for this
    // trip. The manager's busy token covers the same window, but the
    // local flag keeps a busy *sibling* operation from being mistaken
    // for this dialog's attach.
    property bool attaching: false
    property string attachError: ""

    // Owner snapshot taken at the Attach click. Report attribution
    // matches on the id string - not the live trip binding - so a host
    // rebinding (or nulling) trip mid-attach can neither wedge
    // `attaching` forever nor let a sibling surface's report for the
    // new trip close this dialog. The Trip itself is kept only for
    // cancelAttach, which needs the QUuid; losing it (trip destroyed
    // mid-attach) only costs the ability to cancel - the manager's
    // owner-was-deleted failure report still matches the id string
    // and unwedges the dialog.
    property Trip attachingTrip: null
    property string attachingTripId: ""

    // Same snapshot idea for Import: importFinished carries the trip
    // pointer, so identity-compare against the snapshot (a trip
    // destroyed mid-import nulls both sides, which still matches and
    // unwedges the dialog). The source path needs no snapshot - the
    // path field is locked while busy, so previewId.sourcePath is
    // stable until the import reports.
    property bool importing: false
    property Trip importingTrip: null

    property alias title: dialogId.title

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
        pathFieldId.text = ""
        root.attachError = ""
        root.attaching = false
        root.attachingTrip = null
        root.attachingTripId = ""
        root.importing = false
        root.importingTrip = null
        dialogId.open()
    }

    function close() {
        dialogId.close()
    }

    ExternalCenterlineScanPreview {
        id: previewId
        sourcePath: pathFieldId.text
    }

    QQ.Connections {
        target: RootData.externalCenterlineManager

        function onAttachCompleted(report) {
            if (!root.attaching || root.attachingTripId.length === 0) {
                return
            }
            // QUuid wrappers never compare equal directly in JS -
            // compare the string forms (same idiom as the panel tests).
            if (String(report.ownerId) !== root.attachingTripId) {
                return
            }
            root.attaching = false
            root.attachingTrip = null
            root.attachingTripId = ""
            if (report.success) {
                dialogId.close()
                root.attached()
            } else if (report.canceled) {
                dialogId.close()
                root.dismissed()
            } else {
                root.attachError = report.errorMessage
            }
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
            root.imported(previewId.sourcePath)
        }
    }

    QC.Dialog {
        id: dialogId
        objectName: "attachDialog"

        anchors.centerIn: QC.Overlay.overlay
        modal: true
        implicitWidth: 480
        title: qsTr("Add trip from survey file")
        closePolicy: QC.Popup.NoAutoClose

        contentItem: ColumnLayout {
            spacing: Theme.tightSpacing

            QC.Label {
                text: qsTr("Pick the entry file:")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.tightSpacing

                QC.TextField {
                    id: pathFieldId
                    objectName: "sourcePathField"
                    Layout.fillWidth: true
                    enabled: !root.busy
                    placeholderText: qsTr("/path/to/centerline.svx")
                    // A stale failure from the previous attempt must not
                    // outrank the fresh path's scan result.
                    onTextChanged: root.attachError = ""
                }

                QC.Button {
                    objectName: "browseButton"
                    enabled: !root.busy
                    text: qsTr("Browse…")
                    onClicked: fileDialogId.open()
                }
            }

            QC.Label {
                objectName: "formatLabel"
                visible: previewId.formatName.length > 0
                color: Theme.textSecondary
                text: qsTr("Format: %1 (auto-detected)").arg(previewId.formatName)
            }

            QC.Label {
                objectName: "scanningLabel"
                visible: previewId.scanning
                color: Theme.textSecondary
                text: qsTr("Scanning…")
            }

            QC.Label {
                objectName: "scanSummaryLabel"
                visible: previewId.valid
                text: qsTr("✓ Found %n file(s)", "", previewId.fileCount)
            }

            QC.Label {
                objectName: "scanWarningLabel"
                Layout.fillWidth: true
                visible: previewId.warnings.length > 0
                wrapMode: QC.Label.WordWrap
                color: Theme.warning
                text: previewId.warnings.join("\n")
            }

            QC.Label {
                objectName: "scanErrorLabel"
                Layout.fillWidth: true
                visible: text.length > 0
                wrapMode: QC.Label.WordWrap
                color: Theme.danger
                text: root.attachError.length > 0 ? root.attachError
                                                  : previewId.errorMessage
            }

            QC.Label {
                text: qsTr("How should CaveWhere use this file?")
                font.bold: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.tightSpacing

                QC.Button {
                    id: attachButtonId
                    objectName: "attachButton"
                    text: qsTr("Attach")
                    enabled: root.trip !== null && previewId.valid && !root.busy
                    onClicked: {
                        root.attachError = ""
                        root.attachingTrip = root.trip
                        root.attachingTripId = String(root.trip.id)
                        root.attaching = true
                        RootData.attachTripCenterline(root.attachingTrip,
                                                      previewId.sourcePath)
                    }
                }

                QC.Label {
                    objectName: "recommendedLabel"
                    text: qsTr("Recommended")
                    font.bold: true
                    color: Theme.accent
                }
            }

            BodyText {
                objectName: "attachExplainerText"
                Layout.fillWidth: true
                text: qsTr("The file stays the source of truth — keep editing it "
                         + "in your survey tool and CaveWhere stays in sync. A "
                         + "copy travels with your project for git sync, backups, "
                         + "and sharing; CaveWhere never writes to your original "
                         + "files. Survey data is read-only in CaveWhere.")
            }

            QC.Button {
                id: importButtonId
                objectName: "importButton"
                text: qsTr("Import a copy")
                enabled: root.trip !== null && previewId.valid && !root.busy
                         && previewId.importSupported
                onClicked: {
                    root.attachError = ""
                    root.importingTrip = root.trip
                    root.importing = true
                    RootData.surveyImportManager.importSurvexToTrip(
                        previewId.sourcePath, root.importingTrip)
                }
            }

            BodyText {
                objectName: "importExplainerText"
                Layout.fillWidth: true
                text: qsTr("Copies the data into CaveWhere. You edit it here; "
                         + "the original file is never read again.")
            }

            QC.Label {
                objectName: "importFormatNote"
                visible: previewId.valid && !previewId.importSupported
                color: Theme.textSubtle
                text: qsTr("Import supports Survex files only.")
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
                            if (root.attachingTrip !== null) {
                                RootData.externalCenterlineManager.cancelAttach(
                                    root.attachingTrip.id)
                            }
                        } else {
                            dialogId.close()
                            root.dismissed()
                        }
                    }
                }
            }
        }
    }

    QD.FileDialog {
        id: fileDialogId
        objectName: "entryFileDialog"
        title: dialogId.title
        nameFilters: [
            qsTr("Survey files (*.svx *.dat *.mak *.srv *.wpj)"),
            qsTr("All files (*)")
        ]
        currentFolder: RootData.lastDirectory
        fileMode: QD.FileDialog.OpenFile
        onAccepted: {
            RootData.lastDirectory = selectedFile
            pathFieldId.text = RootData.urlToLocal(selectedFile)
        }
    }
}
