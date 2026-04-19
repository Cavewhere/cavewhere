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
        id: imageSpy
        target: buttonId
        signalName: "imageRequested"
    }

    SignalSpy {
        id: sketchSpy
        target: buttonId
        signalName: "sketchRequested"
    }

    SignalSpy {
        id: lidarSpy
        target: buttonId
        signalName: "lidarRequested"
    }

    TestCase {
        name: "AddNoteMenuButton"
        when: windowShown

        function cleanup() {
            imageSpy.clear()
            sketchSpy.clear()
            lidarSpy.clear()
        }

        function test_menuItemsExist() {
            const imageItem = findChild(buttonId, "imageMenuItem")
            const sketchItem = findChild(buttonId, "sketchMenuItem")
            const lidarItem = findChild(buttonId, "lidarMenuItem")
            verify(imageItem !== null, "imageMenuItem should exist")
            verify(sketchItem !== null, "sketchMenuItem should exist")
            verify(lidarItem !== null, "lidarMenuItem should exist")
            compare(imageItem.text, "Image")
            compare(sketchItem.text, "Sketch")
            compare(lidarItem.text, "LiDAR")
        }

        function test_sketchMenuItemEmitsSignal() {
            const sketchItem = findChild(buttonId, "sketchMenuItem")
            sketchItem.triggered()
            compare(sketchSpy.count, 1, "sketchRequested should fire once")
            compare(imageSpy.count, 0, "imageRequested should not fire")
            compare(lidarSpy.count, 0, "lidarRequested should not fire")
        }

        function test_imageMenuItemOpensDialog() {
            const imageItem = findChild(buttonId, "imageMenuItem")
            imageItem.triggered()
            compare(sketchSpy.count, 0, "sketchRequested should not fire")
        }

        function test_lidarMenuItemOpensDialog() {
            const lidarItem = findChild(buttonId, "lidarMenuItem")
            lidarItem.triggered()
            compare(sketchSpy.count, 0, "sketchRequested should not fire")
        }
    }
}
