import QtQuick

import QmlTestRecorder
import cavewherelib

Rectangle {
    id: rootId
    objectName: "rootId"

    property alias mainWindow: mainWindow

    color: Theme.background

    width: 1200
    height: 700

    TestcaseRecorder {
        id: recorderId
        rootItem: mainWindow
        rootItemId: "mainWindow"
    }

    SaveAsDialog {
        id: testSaveAsDialogId
    }

    AskToSaveDialog {
        id: testAskToSaveDialogId
        objectName: "mainWindowTestAskToSaveDialog"
        saveAsDialog: testSaveAsDialogId
        taskName: "opening a cloned repository"
        anchors.centerIn: parent
    }

    MainContent {
        id: mainWindow
        anchors.fill: parent
        askToSaveDialog: testAskToSaveDialogId
    }
}
