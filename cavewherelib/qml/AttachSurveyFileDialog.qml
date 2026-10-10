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

// The Attach survey file dialog (SURVEY_TREE_PLAN C7.4a). Pick an entry file,
// read the scan's summary, see where the subtree goes and the subtree itself,
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

    // "Region root (top level)", or the target's path from the top with its
    // kind: "Kentucky › Side Cave (Cave)".
    function placeUnderText() : string {
        if (root.target === null || root.target === RootData.region.rootNode) {
            return qsTr("Region root (top level)")
        }
        let names = []
        for (let node = root.target;
             node !== null && node !== RootData.region.rootNode;
             node = node.parentNode) {
            names.unshift(node.name)
        }
        const kindLabel = RegionSurveyTree.data(RegionSurveyTree.indexOf(root.target),
                                                SurveyTreeModel.KindLabelRole)
        return qsTr("%1 (%2)").arg(names.join(" › ")).arg(kindLabel)
    }

    // "1 file", "4 files".
    function countText(count: int, singular: string, plural: string) : string {
        return qsTr("%1 %2").arg(count).arg(count === 1 ? singular : plural)
    }

    // One line for what the scan found: the kind, the files, the surveys and
    // stations they hold, and the listed files the disk lacks.
    function summaryText() : string {
        if (pickerId.rows.length === 0) {
            return ""
        }
        const counts = [root.countText(pickerId.fileCount, qsTr("file"), qsTr("files")),
                        root.countText(pickerId.surveyCount, qsTr("survey"), qsTr("surveys")),
                        root.countText(pickerId.stationCount, qsTr("station"), qsTr("stations"))]
        const found = qsTr("✓ %1 — %2").arg(pickerId.kindLabel(pickerId.rows[0])).arg(counts.join(", "))
        return pickerId.missingFileCount > 0
                ? qsTr("%1 · %2 missing").arg(found)
                      .arg(root.countText(pickerId.missingFileCount, qsTr("listed file"), qsTr("listed files")))
                : found
    }

    // How the scan's tree becomes nodes and trips, said for the format of the
    // subtree's root, the scan's first row.
    function previewHintText() : string {
        switch (pickerId.rows.length > 0 ? pickerId.rows[0].kind : SurveyNodeKind.Folder) {
        case SurveyNodeKind.CompassProject:
        case SurveyNodeKind.CompassFile:
            return qsTr("One node per .dat file, one trip per survey.")
        case SurveyNodeKind.WallsBook:
            return qsTr("One node per book, at the depth the project file gives it; "
                        + "one trip per survey file with shots. Books without "
                        + "surveys stay in the tree, grayed.")
        default:
            return qsTr("One node per block; a block with its own shots also gets a trip.")
        }
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

        readonly property int blockIndentWidth: Theme.actionBarSpacing

        anchors.centerIn: QC.Overlay.overlay
        modal: true
        implicitWidth: {
            let target = compact ? compactWidth : wideWidth
            return overlayItem !== null
                    ? Math.min(target, overlayItem.width - 2 * Theme.actionBarSpacing)
                    : target
        }
        title: qsTr("Attach survey file")
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
                objectName: "scanSummaryText"
                Layout.fillWidth: true
                visible: pickerId.valid && pickerId.rows.length > 0
                wrapMode: QC.Label.WordWrap
                color: Theme.textSecondary
                text: root.summaryText()
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.sectionSpacing
                spacing: Theme.tightSpacing

                QC.Label {
                    text: qsTr("Place under")
                    font.bold: true
                }

                QC.Label {
                    objectName: "placeUnderText"
                    Layout.fillWidth: true
                    elide: QC.Label.ElideMiddle
                    color: Theme.textSecondary
                    text: root.placeUnderText()
                }
            }

            // Disclosure, not a picker: the subtree is built as scanned, so
            // this is where the user sees what the attach is about to make.
            ColumnLayout {
                objectName: "blockPreview"
                Layout.fillWidth: true
                Layout.topMargin: Theme.sectionSpacing
                spacing: Theme.tightSpacing
                visible: pickerId.valid && pickerId.rows.length > 0

                QC.Label {
                    objectName: "blockPreviewTitle"
                    text: qsTr("Subtree CaveWhere will create")
                    font.bold: true
                }

                QC.Label {
                    objectName: "blockPreviewHint"
                    Layout.fillWidth: true
                    wrapMode: QC.Label.WordWrap
                    color: Theme.textSubtle
                    font.pixelSize: Theme.fontSizeSmall
                    text: root.previewHintText()
                }

                // The tree scrolls once it outgrows the shared list cap, so a
                // whole survey project never pushes the buttons off the screen.
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
                                     + "editing it in your survey tool and Reload "
                                     + "brings the changes in. A copy travels with "
                                     + "your project for git sync, backups, and "
                                     + "sharing; CaveWhere never writes to your "
                                     + "original files. Survey data is read-only "
                                     + "in CaveWhere.")
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
                            text: qsTr("Copies the data into a trip of a new cave. "
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

                        QC.Label {
                            objectName: "importPlacementNote"
                            Layout.fillWidth: true
                            visible: root.target !== null && !root.targetTakesCaves
                            wrapMode: QC.Label.WordWrap
                            color: Theme.textSubtle
                            text: qsTr("Import adds a cave, so it works only where caves go.")
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
