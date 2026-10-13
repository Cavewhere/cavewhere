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

// The source card on a source root's page: the copied file, its format, where
// the copy came from, the solve status with the root's station count, and the
// derived nodes and trips the user hid, with Restore… to bring them back.
// Reload and Replace… sit in the header's right-click menus; they act on the
// whole subtree, so they live on the root and nowhere below it.
QQ.Item {
    id: root
    objectName: "sourceRootCard"

    property SurveyNode node: null
    property NodePageModel pageModel: null
    property ExternalCenterlineManager externalCenterlineManager: RootData.externalCenterlineManager
    property ExternalSourceSettings externalSourceSettings: RootData.externalSourceSettings
    property LinePlotManager linePlotManager: RootData.linePlotManager

    // True while the card pulses to answer a request for it, such as a
    // warning line pointing at the attached file.
    readonly property bool attentionActive: attentionPulseId.running

    readonly property int hiddenCount: root.pageModel !== null ? root.pageModel.hiddenNames.length : 0

    // Whether Reload has a file to copy. That follows the disk and has no
    // change signal, so it is re-read whenever the owner's state moves.
    property bool sourceReloadable: false

    // The attached model's answers for this root, re-read on its signals.
    property string solveError: ""
    property int solveWarningCount: 0

    implicitWidth: contentColumnId.implicitWidth
    implicitHeight: contentColumnId.implicitHeight

    function openReplaceDialog() {
        replaceDialogLoaderId.active = true
        replaceDialogLoaderId.item.open()
    }

    function drawAttention() {
        attentionPulseId.restart()
    }

    function refreshReloadable() {
        root.sourceReloadable = root.node !== null
                && root.externalCenterlineManager.canReloadFromSource(root.node)
    }

    function refreshSolveState() {
        if (root.node === null) {
            root.solveError = ""
            root.solveWarningCount = 0
            return
        }
        const model = root.externalCenterlineManager.attachedCenterlinesModel
        root.solveError = model.errorFor(root.node.id)
        root.solveWarningCount = model.warningCountFor(root.node.id)
    }

    onNodeChanged: {
        root.refreshReloadable()
        root.refreshSolveState()
    }

    QQ.Component.onCompleted: {
        root.refreshReloadable()
        root.refreshSolveState()
    }

    // What the manager knows about this root's attachment, all of it read
    // imperatively.
    ExternalCenterlineOwnerState {
        id: ownerStateId

        owner: root.node
        manager: root.externalCenterlineManager

        onOwnerBusyChanged: root.refreshReloadable()
        onSourceChangedSinceCopyChanged: root.refreshReloadable()
    }

    QQ.Connections {
        target: root.externalSourceSettings

        function onBreadcrumbsChanged() {
            root.refreshReloadable()
        }
    }

    QQ.Connections {
        target: root.externalCenterlineManager.attachedCenterlinesModel

        function onModelReset() {
            root.refreshSolveState()
        }

        function onDataChanged() {
            root.refreshSolveState()
        }

        function onRowsInserted() {
            root.refreshSolveState()
        }

        function onRowsRemoved() {
            root.refreshSolveState()
        }
    }

    QQ.Rectangle {
        id: attentionFrameId

        anchors.fill: parent
        anchors.margins: -Theme.tightSpacing
        radius: Theme.bannerRadius
        color: Theme.highlight
        border.color: Theme.focusRing
        border.width: Theme.attentionBorderWidth
        opacity: 0
    }

    ColumnLayout {
        id: contentColumnId

        anchors.fill: parent
        spacing: Theme.tightSpacing

        ExternalCenterlineAttachedHeader {
            id: attachedHeaderId

            Layout.fillWidth: true

            owner: root.node
            externalSourceSettings: root.externalSourceSettings
            actionsEnabled: !ownerStateId.ownerBusy
            entryFilePath: ownerStateId.entryFilePath
            sourceChangedSinceCopy: ownerStateId.sourceChangedSinceCopy
            sourceReloadable: root.sourceReloadable

            onReplaceRequested: root.openReplaceDialog()
            onReloadFromSourceRequested: RootData.reloadSource(root.node)
            onSourceEligibilityRefreshRequested: root.refreshReloadable()
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.hiddenCount > 0
            spacing: Theme.tightSpacing

            QC.Label {
                objectName: "sourceRootHiddenLabel"
                Layout.fillWidth: true
                wrapMode: QC.Label.WordWrap
                color: Theme.textSecondary
                text: root.pageModel !== null
                      ? qsTr("%1 hidden: %2 ·").arg(root.hiddenCount)
                                               .arg(root.pageModel.hiddenNames.join(", "))
                      : ""
            }

            LinkText {
                objectName: "sourceRootRestoreLink"
                text: qsTr("Restore…")
                onClicked: restoreDialogId.open()
            }
        }

        ExternalCenterlineSolveStatus {
            Layout.fillWidth: true

            hasError: root.solveError.length > 0 || root.linePlotManager.hasSolveError
            warningCount: root.solveWarningCount
            stationCount: root.pageModel !== null ? root.pageModel.stationCount : 0

            onViewCavernOutputRequested: {
                RootData.pageSelectionModel.gotoPageByName(null, "Cavern")
            }
        }
    }

    QQ.SequentialAnimation {
        id: attentionPulseId

        loops: Theme.attentionPulseCount

        QQ.NumberAnimation {
            target: attentionFrameId
            property: "opacity"
            to: 1
            duration: Theme.attentionPulseDuration
            easing.type: QQ.Easing.OutQuad
        }

        QQ.NumberAnimation {
            target: attentionFrameId
            property: "opacity"
            to: 0
            duration: Theme.attentionPulseDuration
            easing.type: QQ.Easing.InOutQuad
        }
    }

    QC.Dialog {
        id: restoreDialogId
        objectName: "sourceRootRestoreDialog"

        anchors.centerIn: QC.Overlay.overlay
        // A fixed width lets the message wrap rather than size the dialog.
        width: Theme.inlineBannerWidth
        modal: true
        title: qsTr("Restore hidden items")

        contentItem: QC.Label {
            objectName: "sourceRootRestoreMessage"
            wrapMode: QC.Label.WordWrap
            text: qsTr("Restore %n hidden item(s) from %1? They come back as the file describes them.",
                       "", root.hiddenCount)
                  .arg(attachedHeaderId.fileName)
        }

        footer: QC.DialogButtonBox {
            QC.Button {
                objectName: "sourceRootRestoreCancelButton"
                text: qsTr("Cancel")
                QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.RejectRole
            }

            QC.Button {
                objectName: "sourceRootRestoreConfirmButton"
                text: qsTr("Restore")
                QC.DialogButtonBox.buttonRole: QC.DialogButtonBox.AcceptRole
            }
        }

        onAccepted: RootData.restoreHidden(root.node)
    }

    // Built on the first Replace…: the dialog carries a file dialog and a
    // scan preview that most pages never open.
    QQ.Loader {
        id: replaceDialogLoaderId

        active: false
        sourceComponent: QQ.Component {
            ReplaceCenterlineDialog {
                owner: root.node

                // Freed once it is off the screen, for the same reason it is
                // built late. Deferred out of the close handler so the popup
                // is not destroyed from inside its own emission.
                onClosed: Qt.callLater(() => replaceDialogLoaderId.active = false)
            }
        }
    }
}
