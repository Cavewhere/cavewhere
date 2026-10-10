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

// Replace dialog for an already-attached owner — a trip or a cave
// (plans/EXTERNAL_FILE_LIVE_LINK_RETIREMENT.html commit 1). Pick a new
// entry file, preview the scan, and swap the whole closure in one
// operation: the manager reconciles the new file's dependencies into
// the owner's existing attachment dir and garbage-collects whatever the
// new file stops referencing.
//
// The dialog itself is the confirmation the plan calls for (§7): a
// plain statement of what Replace does, rather than a warning about
// local changes we have no baseline to detect.
QQ.Item {
    id: root
    objectName: "replaceCenterlineDialog"

    property QQ.QtObject owner: null

    // The dialog is off the screen again — succeeded, canceled, or dismissed.
    // Whoever built it can drop it here.
    signal closed()

    function open() {
        // The breadcrumb only chooses where Browse starts. The field
        // stays empty: the replacement is a different file than the one
        // the owner was attached from.
        pickerId.initialFolder = owner !== null
                ? RootData.externalSourceSettings.breadcrumbFolder(owner.id)
                : ""
        pickerId.clear()
        sessionId.reset()
        dialogId.open()
    }

    ExternalCenterlineAttachSession {
        id: sessionId
        onSucceeded: dialogId.close()
        onCanceled: dialogId.close()
    }

    QC.Dialog {
        id: dialogId
        objectName: "replaceDialog"

        readonly property QQ.Item overlayItem: QC.Overlay.overlay
        readonly property int preferredWidth: 520

        anchors.centerIn: QC.Overlay.overlay
        modal: true
        implicitWidth: overlayItem !== null
                       ? Math.min(preferredWidth,
                                  overlayItem.width - 2 * Theme.actionBarSpacing)
                       : preferredWidth
        title: qsTr("Replace centerline file")
        closePolicy: QC.Popup.NoAutoClose

        contentItem: ColumnLayout {
            spacing: Theme.tightSpacing

            QC.Label {
                objectName: "currentEntryLabel"
                Layout.fillWidth: true
                elide: QC.Label.ElideMiddle
                text: qsTr("Currently attached: %1").arg(
                          root.owner !== null ? root.owner.externalCenterline.entryFile : "")
            }

            ExternalCenterlineFilePicker {
                id: pickerId
                Layout.fillWidth: true
                promptText: qsTr("Pick the replacement file:")
                fileDialogTitle: dialogId.title
                showsSupportedFormats: false
                locked: sessionId.busy
                operationError: sessionId.errorMessage
                onPathEdited: sessionId.errorMessage = ""
            }

            BodyText {
                objectName: "replaceExplainerText"
                Layout.fillWidth: true
                Layout.topMargin: Theme.sectionSpacing
                wrapMode: QC.Label.WordWrap
                text: qsTr("Replacing swaps the files copied into this project "
                         + "for the picked file and everything it includes. The "
                         + "current copies, including any edits made to them, are "
                         + "replaced; files the new one leaves out are removed.")
            }

            // §6.11 case 5: a scrap on a derived trip anchors on station
            // names, and a file that names its stations differently strands
            // them, so the confirm counts what the swap costs.
            QC.Label {
                id: anchorLossId
                objectName: "replaceAnchorLossText"

                readonly property SurveyNode sourceRoot: root.owner as SurveyNode
                readonly property int noteCount: anchorLossId.sourceRoot !== null && pickerId.valid
                                                 ? pickerId.notesLosingAnchors(anchorLossId.sourceRoot)
                                                 : 0
                readonly property int scrapCount: anchorLossId.sourceRoot !== null && pickerId.valid
                                                  ? pickerId.scrapsLosingAnchors(anchorLossId.sourceRoot)
                                                  : 0

                Layout.fillWidth: true
                visible: anchorLossId.scrapCount > 0
                wrapMode: QC.Label.WordWrap
                color: Theme.warning
                text: qsTr("Replacing strands %1 on %2: they sit on stations the new file does not name.")
                          .arg(anchorLossId.scrapCount === 1 ? qsTr("1 scrap")
                                                             : qsTr("%1 scraps").arg(anchorLossId.scrapCount))
                          .arg(anchorLossId.noteCount === 1 ? qsTr("1 note")
                                                            : qsTr("%1 notes").arg(anchorLossId.noteCount))
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.tightSpacing
                visible: sessionId.busy

                QC.BusyIndicator {
                    implicitWidth: Theme.fontSizeLarge
                    implicitHeight: Theme.fontSizeLarge
                    running: sessionId.busy
                }

                QC.Label {
                    objectName: "replaceBusyLabel"
                    text: qsTr("Replacing…")
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
                    objectName: "replaceCancelButton"
                    text: qsTr("Cancel")
                    onClicked: {
                        if (sessionId.busy) {
                            sessionId.cancel()
                        } else {
                            dialogId.close()
                        }
                    }
                }

                QC.Button {
                    objectName: "replaceConfirmButton"
                    text: qsTr("Replace")
                    enabled: root.owner !== null && pickerId.valid && !sessionId.busy
                    onClicked: {
                        sessionId.start(root.owner)
                        if (root.owner instanceof SurveyNode) {
                            RootData.replaceSource(root.owner, pickerId.sourcePath)
                        } else {
                            RootData.replaceTripCenterline(root.owner, pickerId.sourcePath)
                        }
                    }
                }
            }
        }

        onClosed: root.closed()
    }
}
