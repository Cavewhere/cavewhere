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

            sketchItemId.zoom = 2.0
            sketchItemId.pan = Qt.point(50, 50)

            tryVerify(() => sketchCanvas.grid.viewScale === 0.5,
                      2000, "grid viewScale should be inverse of sketch zoom")
            tryVerify(() => sketchCanvas.grid.gridOrigin.x === 50
                          && sketchCanvas.grid.gridOrigin.y === 50,
                      2000, "grid origin should track sketch pan")
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
