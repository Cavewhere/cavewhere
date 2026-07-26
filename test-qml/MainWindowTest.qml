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

    //This window's shared text editor, for tests that drive or assert on it
    readonly property ShadowEditorHost shadowEditor:
        (rootId.WindowOverlay.overlay as AppOverlay)?.shadowEditor ?? null

    color: Theme.background

    //Every test in a file shares this window, so an editor one test left open
    //is still open in the next. Closing through the field is what happens when
    //a user leaves the cell, so this leaves both sides consistent.
    function closeAnyOpenEditor() {
        if(rootId.shadowEditor === null) {
            return
        }

        if(rootId.shadowEditor.coreClickInput !== null) {
            rootId.shadowEditor.coreClickInput.closeEditor()
        }
        rootId.shadowEditor.enabled = false
    }

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
