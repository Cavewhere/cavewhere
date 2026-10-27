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

// The Add survey file dialog (SURVEY_TREE_PLAN C7.4a). Pick an entry file,
// then Attach: the file's tree becomes a read-only subtree under target. Import
// a copy stays beside it for Survex files, where the target takes caves: the
// parsed shots land in a native trip of a new cave there.
//
// The dialog owns no attach state of its own beyond the session, which
// attributes the manager's shared attachCompleted report back to this
// surface. A failed attach fires no outcome signal: the error shows inline and
// the dialog stays open for another attempt.
QQ.Item {
    id: root
    objectName: "attachSurveyFileDialog"

    // Where Attach builds the file's subtree, the region's root for the top
    // level. Import adds its cave here too.
    property SurveyNode target: null

    // The cave Import created and filled.
    property Cave importedCave: null

    // Import adds the trip it fills, and importFinished names that trip, so
    // the dialog keeps it to recognize its own report.
    property bool importing: false
    property Trip importingTrip: null

    property alias title: dialogId.title

    // True from the Attach click until the session's report lands.
    readonly property bool attaching: sessionId.busy
    readonly property bool busy: root.attaching || root.importing

    // A cave goes only where caves go, so Import is offered there alone.
    readonly property bool targetTakesCaves: root.target !== null && root.target.takesCaves

    // Outcome signals for hosts. Exactly one fires per open()d session that
    // ends: attached(sourceRoot) when this dialog's attach succeeds,
    // imported(cave) when its import completes, named after the file,
    // dismissed() when the user backs out (idle Cancel, or a canceled attach).
    // The programmatic close() is silent so hosts and tests can reset state
    // without triggering cleanup.
    signal attached(sourceRoot: SurveyNode)
    signal imported(cave: Cave)
    signal dismissed()

    function open() {
        pickerId.clear()
        sessionId.reset()
        root.importing = false
        root.importingTrip = null
        root.importedCave = null
        dialogId.open()
    }

    function close() {
        dialogId.close()
    }

    // Import a copy: the parsed shots go into a new trip of a new cave under
    // the target.
    function startImport() {
        sessionId.errorMessage = ""
        root.importedCave = RootData.region.addNode(root.target, SurveyNodeKind.Cave) as Cave
        root.importedCave.addTrip()
        root.importingTrip = root.importedCave.trip(root.importedCave.tripCount - 1)
        root.importing = true
        RootData.surveyImportManager.importSurvexToTrip(pickerId.sourcePath, root.importingTrip)
    }

    // The child of target whose id reads as nodeId, or null.
    function childWithId(nodeId: string) : SurveyNode {
        if (root.target === null) {
            return null
        }
        for (let i = 0; i < root.target.childNodeCount; ++i) {
            const child = root.target.childNode(i)
            if (String(child.id) === nodeId) {
                return child
            }
        }
        return null
    }

    ExternalCenterlineAttachSession {
        id: sessionId

        onSucceeded: (sourceRootId) => {
            dialogId.close()
            root.attached(root.childWithId(sourceRootId))
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
                root.importing = false
                root.importingTrip = null
                // Parse warnings live in the global error model; the import
                // lands whatever parsed, so the session is over either way.
                // The file's name without its directory or extension names
                // both; uniqueTripName and uniqueChildName dedupe, since
                // setName silently refuses a name a sibling holds.
                const fileName = RootData.fileName(pickerId.sourcePath)
                const dotIndex = fileName.lastIndexOf(".")
                const baseName = dotIndex > 0 ? fileName.substring(0, dotIndex) : fileName
                trip.name = root.importedCave.uniqueTripName(baseName)
                root.importedCave.name = root.importedCave.parentNode.uniqueChildName(baseName)
                dialogId.close()
                root.imported(root.importedCave)
            }
        }
    }

    QC.Dialog {
        id: dialogId
        objectName: "attachDialog"

        readonly property QQ.Item overlayItem: QC.Overlay.overlay
        readonly property bool compact: overlayItem !== null
                                        && overlayItem.width < Theme.breakpointPanelCollapse
        readonly property int wideWidth: 640
        readonly property int compactWidth: 400
        readonly property bool offersImport: pickerId.importSupported && root.targetTakesCaves

        anchors.centerIn: QC.Overlay.overlay
        modal: true
        implicitWidth: {
            let target = compact ? compactWidth : wideWidth
            return overlayItem !== null
                    ? Math.min(target, overlayItem.width - 2 * Theme.actionBarSpacing)
                    : target
        }
        title: qsTr("Add survey file")
        closePolicy: QC.Popup.NoAutoClose

        contentItem: ColumnLayout {
            spacing: Theme.tightSpacing

            ExternalCenterlineFilePicker {
                id: pickerId
                Layout.fillWidth: true
                fileDialogTitle: dialogId.title
                locked: root.busy
                operationError: sessionId.errorMessage
                onPathEdited: sessionId.errorMessage = ""
            }

            QC.Label {
                objectName: "choicesHeading"
                Layout.topMargin: Theme.sectionSpacing
                visible: pickerId.valid && dialogId.offersImport
                text: qsTr("How should CaveWhere use this file?")
                font.bold: true
            }

            GridLayout {
                Layout.fillWidth: true
                // The heading carries the section gap; alone, the grid does.
                Layout.topMargin: dialogId.offersImport ? 0 : Theme.sectionSpacing
                visible: pickerId.valid
                columns: !dialogId.compact && dialogId.offersImport ? 2 : 1
                columnSpacing: Theme.flowSpacing
                rowSpacing: Theme.flowSpacing

                QC.GroupBox {
                    objectName: "attachChoiceBox"
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
                            // The scan names the tool and counts the files before the box shows.
                            text: (pickerId.fileCount === 1
                                   ? qsTr("Keep editing in %1; Reload picks up the "
                                          + "changes. The survey file is read-only in "
                                          + "CaveWhere, and a copy ships with your project.")
                                   : qsTr("Keep editing in %1; Reload picks up the "
                                          + "changes. The survey files are read-only in "
                                          + "CaveWhere, and a copy ships with your project."))
                                  .arg(pickerId.formatName)
                        }

                        QQ.Item { Layout.fillHeight: true }

                        RowLayout {
                            Layout.alignment: Qt.AlignRight
                            Layout.topMargin: Theme.sectionSpacing
                            spacing: Theme.flowSpacing

                            QC.Label {
                                objectName: "recommendedLabel"
                                visible: dialogId.offersImport
                                text: qsTr("Recommended")
                                font.bold: true
                                color: Theme.accent
                            }

                            QC.Button {
                                objectName: "attachButton"
                                text: qsTr("Attach")
                                enabled: root.target !== null && pickerId.valid && !root.busy
                                onClicked: {
                                    sessionId.start(root.target)
                                    RootData.attachSource(root.target, pickerId.sourcePath)
                                }
                            }
                        }
                    }
                }

                QC.GroupBox {
                    objectName: "importChoiceBox"
                    Layout.fillWidth: true
                    Layout.fillHeight: !dialogId.compact
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop
                    visible: dialogId.offersImport

                    contentItem: ColumnLayout {
                        spacing: Theme.tightSpacing

                        BodyText {
                            objectName: "importExplainerText"
                            Layout.fillWidth: true
                            wrapMode: QC.Label.WordWrap
                            text: qsTr("Copies the survey into a new cave you edit "
                                     + "here. The file is read once.")
                        }

                        QQ.Item { Layout.fillHeight: true }

                        QC.Button {
                            objectName: "importButton"
                            Layout.alignment: Qt.AlignRight
                            Layout.topMargin: Theme.sectionSpacing
                            text: qsTr("Import a copy")
                            enabled: pickerId.valid && !root.busy && pickerId.importSupported
                                     && root.targetTakesCaves
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
