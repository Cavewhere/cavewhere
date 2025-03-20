import QtQuick

import QmlTestRecorder
import cavewherelib

Item {
    id: rootId
    objectName: "rootId"

    property alias mainWindow: mainWindow

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
