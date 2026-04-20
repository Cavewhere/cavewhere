import QtQuick
import QtTest
import cavewherelib

Item {
    id: rootId
    objectName: "rootId"

    width: 600
    height: 400

    // Declarative `Sketch {}` would persist across tests; any reset path that touches viewState flips viewInitialized.
    Component {
        id: sketchComponent
        Sketch {}
    }

    SketchItem {
        id: sketchItemId
        objectName: "sketchItem"
        anchors.fill: parent
    }

    TestCase {
        id: testCaseId
        name: "SketchViewState"
        when: windowShown

        property var sketchA: null
        property var sketchB: null

        function init() {
            sketchA = sketchComponent.createObject(rootId)
            sketchB = sketchComponent.createObject(rootId)
            sketchItemId.sketch = sketchA
        }

        function cleanup() {
            sketchItemId.sketch = null
            if (sketchA) { sketchA.destroy(); sketchA = null }
            if (sketchB) { sketchB.destroy(); sketchB = null }
        }

        function zoomWithWheel(steps) {
            for (let i = 0; i < steps; i++) {
                mouseWheel(sketchItemId, 300, 200, 0, 120,
                           Qt.NoButton, Qt.NoModifier, 5)
            }
        }

        // First move activates DragHandler and captures lastPosition; second move drives the pan delta.
        function rightDrag(from, to) {
            mousePress(sketchItemId, from.x, from.y, Qt.RightButton)
            mouseMove(sketchItemId, from.x + 5, from.y + 5, 0, Qt.RightButton)
            mouseMove(sketchItemId, to.x, to.y, 0, Qt.RightButton)
            mouseRelease(sketchItemId, to.x, to.y, Qt.RightButton)
        }

        function test_freshSketchIsCenteredAtUnitZoom() {
            verify(!sketchA.viewState.viewInitialized,
                   "A fresh sketch should not be marked initialized")
            compare(sketchItemId.zoom, 1.0)
            compare(sketchItemId.pan, Qt.point(300, 200))
        }

        function test_wheelZoomUpdatesViewState() {
            zoomWithWheel(3)
            tryVerify(() => sketchA.viewState.viewInitialized,
                      2000, "Wheel event should mark view initialized")
            tryVerify(() => sketchA.viewState.zoom > 1.0,
                      2000, "Zoom should have increased after wheel up")
            tryCompare(sketchItemId, "zoom", sketchA.viewState.zoom)
        }

        function test_dragPanUpdatesViewState() {
            rightDrag(Qt.point(100, 100), Qt.point(200, 150))
            tryVerify(() => sketchA.viewState.viewInitialized,
                      2000, "Drag should mark view initialized")
            tryVerify(() => sketchA.viewState.pan.x !== 0
                         || sketchA.viewState.pan.y !== 0,
                      2000, "Pan should move after right-button drag")
            tryCompare(sketchItemId, "pan", sketchA.viewState.pan)
        }

        function test_switchingSketchResetsToCenter() {
            zoomWithWheel(2)
            rightDrag(Qt.point(100, 100), Qt.point(150, 120))
            tryVerify(() => sketchA.viewState.viewInitialized, 2000)

            sketchItemId.sketch = sketchB
            tryCompare(sketchItemId, "zoom", 1.0)
            tryCompare(sketchItemId, "pan", Qt.point(300, 200))
            verify(!sketchB.viewState.viewInitialized,
                   "sketchB is still fresh — its flag should stay false")
        }

        function test_switchingBackRestoresPreviousView() {
            zoomWithWheel(2)
            rightDrag(Qt.point(100, 100), Qt.point(150, 120))
            tryVerify(() => sketchA.viewState.viewInitialized, 2000)

            const savedZoom = sketchA.viewState.zoom
            const savedPan  = sketchA.viewState.pan

            sketchItemId.sketch = sketchB
            tryCompare(sketchItemId, "zoom", 1.0)

            sketchItemId.sketch = sketchA
            tryCompare(sketchItemId, "zoom", savedZoom)
            tryCompare(sketchItemId, "pan", savedPan)
        }

        function test_independentState() {
            zoomWithWheel(3)
            tryVerify(() => sketchA.viewState.viewInitialized, 2000)
            const aZoom = sketchA.viewState.zoom

            sketchItemId.sketch = sketchB
            zoomWithWheel(5)
            tryVerify(() => sketchB.viewState.viewInitialized, 2000)
            verify(sketchB.viewState.zoom !== aZoom,
                   "B should have zoomed differently from A")

            sketchItemId.sketch = sketchA
            tryCompare(sketchItemId, "zoom", aZoom)
            compare(sketchA.viewState.zoom, aZoom,
                    "A's stored zoom should be untouched by B's gestures")
        }
    }
}
