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

        // Regression for issue #457: toggling the zoom-lock button on should
        // return the view to its fresh-sketch default — 1× zoom AND centered
        // pan. The bug was that only zoom was animated, so a panned view
        // snapped back to 1× at the user's last pan offset, dropping the
        // world origin at an apparently random screen location.
        function test_zoomLockResetsViewToCentered() {
            const center = Qt.point(sketchItemId.width / 2, sketchItemId.height / 2)
            const lockBtn = findChild(sketchItemId, "zoomLockButton")
            verify(lockBtn !== null, "zoomLockButton should exist")
            verify(!lockBtn.checked,
                   "Zoom lock defaults to off; the button should start unchecked")

            // Already unlocked, so wheel/drag are enabled — pan + zoom off-center.
            zoomWithWheel(3)
            rightDrag(Qt.point(100, 100), Qt.point(250, 220))
            tryVerify(() => sketchA.viewState.viewInitialized, 2000)
            tryVerify(() => sketchA.viewState.zoom !== 1.0, 2000,
                      "Zoom should have moved off 1× before re-locking")
            tryVerify(() => sketchA.viewState.pan.x !== center.x
                         || sketchA.viewState.pan.y !== center.y,
                      2000,
                      "Pan should be off-center before re-locking")

            mouseClick(lockBtn)
            tryVerify(() => sketchA.viewState.zoomLocked, 2000,
                      "Clicking the lock button should lock zoom")

            tryCompare(sketchItemId, "zoom", 1.0, 2000,
                       "Zoom lock should animate zoom back to 1×")
            tryCompare(sketchItemId, "pan", center, 2000,
                       "Zoom lock should also recenter the pan, "
                       + "not leave the world origin where the user dragged it")
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
