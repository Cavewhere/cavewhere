import QtQuick

import QmlTestRecorder
import cavewherelib

Rectangle {
    id: rootId
    objectName: "rootId"

    property alias mainWindow: mainWindow
    property alias recorder: recorderId
    // The Save As dialog is a Popup, so it is not in mainWindow's child tree and
    // an objectName search can't reach it. Expose it for tests that drive it.
    property alias saveAsDialog: testSaveAsDialogId

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
