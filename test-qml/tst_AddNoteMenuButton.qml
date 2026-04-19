import QtQuick
import QtTest
import cavewherelib

Item {
    id: rootId
    objectName: "rootId"

    width: 200
    height: 200

    AddNoteMenuButton {
        id: buttonId
        objectName: "addNoteMenuButton"
        anchors.centerIn: parent
    }

    SignalSpy {
        id: filesSpy
        target: buttonId
        signalName: "filesRequested"
    }

    SignalSpy {
        id: sketchSpy
        target: buttonId
        signalName: "sketchRequested"
    }

    TestCase {
        name: "AddNoteMenuButton"
        when: windowShown

        function cleanup() {
            filesSpy.clear()
            sketchSpy.clear()
        }

        function test_menuItemsExist() {
            const filesItem = findChild(buttonId, "filesMenuItem")
            const sketchItem = findChild(buttonId, "sketchMenuItem")
            verify(filesItem !== null, "filesMenuItem should exist")
            verify(sketchItem !== null, "sketchMenuItem should exist")
            compare(filesItem.text, "Notes or 3D Model")
            compare(sketchItem.text, "Sketch")
        }

        function test_sketchMenuItemEmitsSignal() {
            const sketchItem = findChild(buttonId, "sketchMenuItem")
            sketchItem.triggered()
            compare(sketchSpy.count, 1, "sketchRequested should fire once")
            compare(filesSpy.count, 0, "filesRequested should not fire")
        }

        function test_filesMenuItemOpensDialog() {
            const filesItem = findChild(buttonId, "filesMenuItem")
            filesItem.triggered()
            compare(sketchSpy.count, 0, "sketchRequested should not fire")
        }
    }
}
