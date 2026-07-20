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

// Modal attach dialog for an externally-backed trip (master plan
// §8.1, Phase 2 commit 12). Pick an entry file, preview the scan
// (auto-detected format, file count, warnings, errors), then run
// the attach through the manager's completion-signal bridge. There
// is no watch-vs-import choice - the source path is always
// remembered (direction change 2026-07-16). Cancel during the
// attach routes through cancelAttach, which the manager honors
// only until the attach's internal scan lands; a later click is a
// structural no-op, so the button can stay enabled for the whole
// operation without the q14 token-release hazard.
QQ.Item {
    id: root
    objectName: "attachExternalCenterlineDialog"

    property Trip trip: null
    property ExternalCenterlineManager externalCenterlineManager: RootData.externalCenterlineManager

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

    function open() {
        pathFieldId.text = ""
        root.attachError = ""
        root.attaching = false
        root.attachingTrip = null
        root.attachingTripId = ""
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
        target: root.externalCenterlineManager

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
            if (report.success || report.canceled) {
                dialogId.close()
            } else {
                root.attachError = report.errorMessage
            }
        }
    }

    QC.Dialog {
        id: dialogId
        objectName: "attachDialog"

        anchors.centerIn: QC.Overlay.overlay
        modal: true
        implicitWidth: 480
        title: qsTr("Attach external centerline")
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
                    enabled: !root.attaching
                    placeholderText: qsTr("/path/to/centerline.svx")
                    // A stale failure from the previous attempt must not
                    // outrank the fresh path's scan result.
                    onTextChanged: root.attachError = ""
                }

                QC.Button {
                    objectName: "browseButton"
                    enabled: !root.attaching
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

            BodyText {
                objectName: "copyExplainerText"
                Layout.fillWidth: true
                text: qsTr("CaveWhere copies the file (and everything it includes) "
                         + "into your project, so the centerline travels with git "
                         + "sync, backups, and sharing. CaveWhere never writes to "
                         + "your original files — keep editing them in your survey "
                         + "tool.")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.tightSpacing
                visible: root.attaching

                QC.BusyIndicator {
                    implicitWidth: Theme.fontSizeLarge
                    implicitHeight: Theme.fontSizeLarge
                    running: root.attaching
                }

                QC.Label {
                    objectName: "attachingLabel"
                    text: qsTr("Attaching…")
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
                    onClicked: {
                        if (root.attaching) {
                            if (root.attachingTrip !== null) {
                                root.externalCenterlineManager.cancelAttach(
                                    root.attachingTrip.id)
                            }
                        } else {
                            dialogId.close()
                        }
                    }
                }

                QC.Button {
                    id: attachButtonId
                    objectName: "attachButton"
                    text: qsTr("Attach")
                    enabled: root.trip !== null && previewId.valid && !root.attaching
                    onClicked: {
                        root.attachError = ""
                        root.attachingTrip = root.trip
                        root.attachingTripId = String(root.trip.id)
                        root.attaching = true
                        RootData.attachTripCenterline(root.attachingTrip,
                                                      previewId.sourcePath)
                    }
                }
            }
        }
    }

    QD.FileDialog {
        id: fileDialogId
        objectName: "entryFileDialog"
        title: qsTr("Attach external centerline")
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
