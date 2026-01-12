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

    MainContent {
        id: mainWindow
        anchors.fill: parent
    }
}
