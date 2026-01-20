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

    visible: false
    width: 1024
    height: 576
    readonly property bool isMacOS: Qt.platform.os === "osx"
    readonly property bool showMenuBar: RootData.desktopBuild && RootData.account.isValid && isMacOS
    // flags: Qt.FramelessWindowHint

    title: {
        var baseName = "CaveWhere - " + RootData.version
        var filename = RootData.project.isTemporaryProject ? "New File" : RootData.project.filename;
        return baseName + "   " + filename
    }

    menuBar: macosMenuBarLoaderId.item

    component MainWindowFileMenu : FileMenu {
        applicationWindow: applicationWindowId
        saveAsFileDialog: saveAsFileDialogId
        loadFileDialog: loadDialogId.loadFileDialog
        askToSaveDialog: askToSaveDialogId
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

    //This is only shown on shutdown when task still need to be completed
    QQ.Loader {
        id: shutdownLoader
        active: false
        anchors.fill: parent

        sourceComponent: QQ.Item {
            anchors.fill: parent

            QQ.Rectangle {
                anchors.fill: parent
                color: Theme.background
            }

            ColumnLayout {
                id: columnId
                anchors.centerIn: parent

                Text {
                    text: "Finishing these jobs"
                    font.pixelSize: 30
                    Layout.alignment: Qt.AlignHCenter
                }

                QC.Button {
                    text: "Quit Now"
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: {
                        Qt.quit();
                    }
                }
            }

            TaskListView {
                width: Math.min(parent.width - 10, Math.max(parent.width * 0.2, 200))
                anchors {
                    top: columnId.bottom
                    bottom: parent.bottom
                    horizontalCenter: parent.horizontalCenter
                    margins: 10
                }
                verticalLayoutDirection: QQ.ListView.TopToBottom
            }
        }
    }

    SaveAsDialog {
        id: saveAsFileDialogId
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

    onClosing: (close) => {
        askToSaveDialogId.taskName = "quiting"
        askToSaveDialogId.afterSaveFunc = function() {
            mainContentId.enabled = false;
            applicationWindowId.menuBar = null
            shutdownLoader.active = true
            RootData.shutdown();
            Qt.quit();
        }

        askToSaveDialogId.askToSave();
        close.accepted = false;
    }

    QQ.Component.onCompleted: {
        screenSizeSaverId.resize();
    }
}
