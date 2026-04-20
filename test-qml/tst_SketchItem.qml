import QtQuick
import QtTest
import QmlTestRecorder
import cavewherelib

Item {
    id: rootId
    objectName: "rootId"

    width: 600
    height: 400

    Sketch {
        id: sketchId
    }

    SketchItem {
        id: sketchItemId
        objectName: "sketchItem"
        anchors.fill: parent
        sketch: sketchId
    }

    TestCase {
        name: "SketchItem"
        when: windowShown

        function drawStroke(points) {
            mousePress(sketchItemId, points[0].x, points[0].y, Qt.LeftButton)
            for (let i = 1; i < points.length; i++) {
                mouseMove(sketchItemId, points[i].x, points[i].y, 0, Qt.LeftButton)
            }
            const last = points[points.length - 1]
            mouseRelease(sketchItemId, last.x, last.y, Qt.LeftButton)
        }

        function test_gridTracksPanZoom() {
            let sketchCanvas = findChild(sketchItemId, "sketchCanvas")
            verify(sketchCanvas !== null, "sketchCanvas should exist")
            verify(sketchCanvas.grid !== null,
                   "sketchCanvas.grid should be wired")

            // Baseline gridOrigin with pan=(0,0) so we can assert movement.
            sketchId.viewState.zoom = 1.0
            sketchId.viewState.pan = Qt.point(0, 0)
            tryVerify(() => sketchCanvas.grid.gridOrigin.x === 0
                          && sketchCanvas.grid.gridOrigin.y === 0,
                      2000, "gridOrigin is world (0,0) when pan is zero")

            sketchId.viewState.zoom = 2.0
            sketchId.viewState.pan = Qt.point(50, 50)

            tryVerify(() => sketchCanvas.grid.viewScale === 0.5,
                      2000, "grid viewScale should be inverse of sketch zoom")
            // gridOrigin is now the world-meter point that lands at item (0,0).
            // With pan=(50,50) the world origin sits at screen (50,50), so
            // item (0,0) maps to a negative world point; exact value depends
            // on Screen.pixelDensity so just assert it moved.
            tryVerify(() => sketchCanvas.grid.gridOrigin.x !== 0
                          && sketchCanvas.grid.gridOrigin.y !== 0,
                      2000, "gridOrigin should shift when pan changes")
            tryVerify(() => sketchCanvas.grid.majorGridModel.rowCount() > 0,
                      2000, "major grid model should populate for the viewport")
        }

        function test_stylusInputAndUndo() {
            const initial = sketchId.strokeModel.rowCount()

            drawStroke([Qt.point(100, 100),
                        Qt.point(150, 120),
                        Qt.point(200, 140),
                        Qt.point(250, 160)])
            tryVerify(() => sketchId.strokeModel.rowCount() === initial + 1,
                      2000, "First drag should produce a stroke")

            const strokeIdx = sketchId.strokeModel.index(initial, 0)
            const points = sketchId.strokeModel.data(strokeIdx, PenStrokeModel.PointsRole)
            verify(points !== undefined && points !== null,
                   "Stroke points should be retrievable")
            verify(points.length >= 2,
                   "Stroke should contain at least two points, got "
                   + points.length)

            drawStroke([Qt.point(300, 100), Qt.point(400, 200)])
            tryVerify(() => sketchId.strokeModel.rowCount() === initial + 2,
                      2000, "Second drag should produce another stroke")

            const undoBtn = findChild(rootId, "undoButton")
            verify(undoBtn !== null, "Undo button should exist")
            verify(undoBtn.enabled, "Undo button should be enabled")
            mouseClick(undoBtn)

            tryVerify(() => sketchId.strokeModel.rowCount() === initial + 1,
                      2000, "Undo should remove the most recent stroke")
        }
    }
}
