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
            verify(filesItem !== null, "filesMenuItem should exist")
            compare(filesItem.text, "Notes or 3D Model")
        }

        // Sketch is hidden from the menu for this release (#587); re-enable
        // this test and the sketch assertions above with the menu item.
        function test_sketchMenuItemHidden() {
            const sketchItem = findChild(buttonId, "sketchMenuItem")
            verify(sketchItem === null, "sketchMenuItem should be hidden")
        }

        // function test_sketchMenuItemEmitsSignal() {
        //     const sketchItem = findChild(buttonId, "sketchMenuItem")
        //     sketchItem.triggered()
        //     compare(sketchSpy.count, 1, "sketchRequested should fire once")
        //     compare(filesSpy.count, 0, "filesRequested should not fire")
        // }

        function test_filesMenuItemOpensDialog() {
            const filesItem = findChild(buttonId, "filesMenuItem")
            filesItem.triggered()
            compare(sketchSpy.count, 0, "sketchRequested should not fire")
        }
    }
}
