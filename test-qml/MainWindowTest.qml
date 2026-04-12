import QtQuick

import QmlTestRecorder
import cavewherelib

Rectangle {
    id: rootId
    objectName: "rootId"

    property alias mainWindow: mainWindow
    property alias recorder: recorderId

    color: Theme.background

    width: 1200
    height: 700

    TestcaseRecorder {
        id: recorderId
        rootItem: mainWindow
        rootItemId: "mainWindow"
        // Hidden by default so it does not steal OS-level focus from the
        // test window on real compositors (Wayland ignores raise/activate
        // requests from other windows). Show manually when needed.
        visible: false
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
