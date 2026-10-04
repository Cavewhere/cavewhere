/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Window
import QtQuick.Dialogs
import QtQuick.Layouts
import cavewherelib
import QQuickGit

QC.ApplicationWindow {
    id: applicationWindowId
    objectName: "applicationWindow"

    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeUI

    Binding { target: GitFontScale; property: "fontSizeCaption"; value: Theme.fontSizeCaption }
    Binding { target: GitFontScale; property: "fontSizeSmall";   value: Theme.fontSizeSmall }
    Binding { target: GitFontScale; property: "fontSizeUI";      value: Theme.fontSizeBody }
    Binding { target: GitFontScale; property: "fontSizeBase";    value: Theme.fontSizeUI }
    Binding { target: GitFontScale; property: "fontSizeTitle";   value: Theme.fontSizeTitle }
    Binding { target: GitFontScale; property: "fontFamilyMono"; value: Theme.fontFamilyMono }

    visible: false
    width: 1024
    height: 576
    readonly property bool isMacOS: Qt.platform.os === "osx"
    readonly property bool showMenuBar: RootData.desktopBuild && RootData.account.isValid && isMacOS
    // flags: Qt.FramelessWindowHint

    title: {
        var baseName = "CaveWhere - " + RootData.version
        var isTemp = RootData.project.isTemporaryProject
        var filename = isTemp ? "New File" : RootData.project.filename
        var dirtyMark = !isTemp && RootData.project.modified ? "* " : ""

        return baseName + "   " + filename + dirtyMark
    }

    menuBar: macosMenuBarLoaderId.item

    component MainWindowFileMenu : FileMenu {
        applicationWindow: applicationWindowId
        saveAsFileDialog: saveAsFileDialogId
        loadFileDialog: loadDialogId.loadFileDialog
        askToSaveDialog: askToSaveDialogId
        onShareRequested: shareDialogId.open()
        onOpenSharedLinkRequested: openSharedLinkDialogId.open()
    }

    QQ.Loader {
        id: macosMenuBarLoaderId
        active: RootData.desktopBuild && RootData.account.isValid && isMacOS
        sourceComponent: QC.MenuBar {
            MainWindowFileMenu {}
        }
    }

    QQ.Loader {
        id: windowsLinuxFileMenuLoader
        active: RootData.desktopBuild && !macosMenuBarLoaderId.active
        sourceComponent: MainWindowFileMenu {}
    }

    ScreenSizeSaver {
        id: screenSizeSaverId
        window: applicationWindowId
        windowName: "mainWindow"
    }

    QQ.Loader {
        id: loadAboutWindowId
    }

    QQ.Loader {
        id: mainContentId
        active: RootData.account.isValid
        anchors.fill: parent
        sourceComponent: MainContent {
            anchors.fill: parent
            fileMenu: windowsLinuxFileMenuLoader.item
            askToSaveDialog: askToSaveDialogId
        }
    }

    QQ.Loader {
        active: !RootData.account.isValid
        anchors.fill: parent
        sourceComponent: WelcomePage {
            anchors.fill: parent
            account: RootData.account
        }
    }

    QQ.Item {
        id: overlayItem
        anchors.fill: parent
    }

    //This is only shown on shutdown when tasks still need to be completed
    QQ.Loader {
        id: shutdownLoader
        active: false
        anchors.fill: parent
        sourceComponent: ShutdownScreen {}
    }

    // Single exit point: shutdown() drains all tasks/futures asynchronously,
    // then emits shutdownComplete. Qt.quit() triggers the engine cleanup
    // lambda in main.cpp which deletes the QML engine and calls a.quit().
    QQ.Connections {
        target: RootData
        function onShutdownComplete() {
            Qt.quit();
        }
    }

    SaveAsDialog {
        id: saveAsFileDialogId
    }

    ShareDialog {
        id: shareDialogId
        onSetupRemoteRequested: shareSetupWizardLoaderId.open()
    }

    SetupRemoteWizardLoader {
        id: shareSetupWizardLoaderId
    }

    ErrorDialog {
        id: projectErrorDialog
        model: RootData.project.errorModel
    }

    LoadProjectDialog {
        id: loadDialogId
    }

    AskToSaveDialog {
        id: askToSaveDialogId
        saveAsDialog: saveAsFileDialogId
        taskName: "quiting"
    }

    SaveFeedbackHelpBox {
        anchors.centerIn: parent
    }

    DeepLinkConfirmDialog {
        id: deepLinkConfirmDialogId
    }

    OpenSharedLinkDialog {
        id: openSharedLinkDialogId
    }

    QQ.Connections {
        target: RootData.deepLinkHandler
        function onOpenRepoRequested(url) {
            deepLinkConfirmDialogId.open(url)
        }
    }

    QQ.Connections {
        target: deepLinkConfirmDialogId
        function onOpenRequested(filePath) {
            askToSaveDialogId.taskName = "opening a cloned repository"
            askToSaveDialogId.afterSaveFunc = function() {
                RootData.loadProject(filePath)
                deepLinkConfirmDialogId.close()
            }
            askToSaveDialogId.askToSave()
        }
    }

    LfsMissingFilesBanner {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        anchors.horizontalCenter: parent.horizontalCenter
    }

    VersionIncompatibleBanner {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.bottomMargin: 20
        anchors.rightMargin: 20
    }

    onClosing: (close) => {
        if (shutdownLoader.active) {
            close.accepted = true;
            return;
        }
        askToSaveDialogId.taskName = "quiting"
        askToSaveDialogId.onSaveConfirmed = function() {
            mainContentId.enabled = false;
            applicationWindowId.menuBar = null;
            shutdownLoader.active = true;
        }
        askToSaveDialogId.afterSaveFunc = function() {
            // Activate the shutdown screen before quitting. For non-temporary
            // projects onSaveConfirmed already does this, but for temporary
            // projects the user first saves via SaveAsDialog so onSaveConfirmed
            // is never invoked — this ensures shutdown always runs.
            mainContentId.enabled = false;
            applicationWindowId.menuBar = null;
            shutdownLoader.active = true;
            RootData.shutdown();
        }

        askToSaveDialogId.askToSave();
        close.accepted = false;
    }

    QQ.Component.onCompleted: {
        screenSizeSaverId.resize();

        // Drain any deep-link URL that arrived before QML was loaded (Windows cold start)
        var pending = RootData.deepLinkHandler.takePendingUrl()
        if (pending.length > 0)
            deepLinkConfirmDialogId.open(pending)
    }
}
