import QtQuick as QQ
import QtTest
import QmlTestRecorder
import cavewherelib
import cw.TestLib

// A colored ground rather than the default one, so a ring that composites
// opaquely shows up as a black square instead of blending into the window.
QQ.Rectangle {
    id: rootId
    objectName: "rootId"

    width: 200
    height: 200

    color: "#ff00ff"

    TaskProgressRing {
        id: ringId
        objectName: "ring"
        anchors.centerIn: parent
    }

    TestCase {
        name: "TaskProgressRing"
        when: windowShown

        function init() {
            // Driven directly rather than left bound to the live task model, so a
            // job the rest of the suite happens to be running can't move it.
            ringId.progress = -1
            ringId.count = 0
            ringId.showCount = false
        }

        function find(name) {
            return ObjectFinder.findObjectByChain(rootId, "rootId->ring->" + name)
        }

        // The ring is a mark over whatever is behind it, never a tile. A
        // QCanvasPainterItem fills its whole rect with fillColor — opaque black
        // by default — and composites that opaquely unless alphaBlending is on,
        // so either half alone still leaves a black square under the ring.
        function test_theRingBlendsWithWhatIsBehindIt() {
            let canvas = find("progressRingCanvas")
            verify(canvas !== null, "progressRingCanvas not found")

            compare(canvas.fillColor.a, 0, "nothing is painted behind the ring")
            verify(canvas.alphaBlending, "and what is painted is composited over the ground")

            // paint() only ever runs against a real QRhi, and the offscreen
            // platform the suite runs under in CI has none. Ask the platform
            // rather than inferring it from the pixels: "nothing was drawn" is
            // also what a broken renderer looks like, and this test should fail
            // on that, not skip.
            if (!OffscreenRenderTester.windowHasRhi(canvas)) {
                skip("no QRhi on this platform (headless offscreen); run with a GPU-backed platform")
                return
            }

            let image = grabImage(rootId)
            let scale = image.width / rootId.width
            let pixelAt = function(x, y) {
                let point = rootId.mapFromItem(canvas, x, y)
                return image.pixel(Math.round(point.x * scale),
                                   Math.round(point.y * scale))
            }

            // Twelve o'clock, on the track's centerline: the ring's radius is
            // 0.375 of its side. Proves the ring drew at all, so the corner
            // check below cannot agree with an item that painted nothing.
            verify(!Qt.colorEqual(pixelAt(canvas.width * 0.5,
                                          canvas.height * (0.5 - 0.375)),
                                  rootId.color),
                   "the ring drew its track")

            // The corner, which is well outside the track the ring drew on.
            compare(pixelAt(1, 1), rootId.color,
                    "the ground shows through where the ring drew nothing")
        }

        // Most jobs report no step count at all, so this is the state the ring is
        // in most of the time. A still ring would read as work that has stalled.
        function test_theRingSpinsWhileNothingReportsSteps() {
            let canvas = find("progressRingCanvas")
            verify(canvas !== null, "progressRingCanvas not found")

            let start = canvas.spinAngle
            tryVerify(function() { return canvas.spinAngle !== start },
                      2000, "the indeterminate ring is moving")
        }

        // A measured job that has finished no step yet reports 0, which is not a
        // number the ring can draw — there is no arc at 0%. Left as determinate
        // it would be a bare track, which reads as stalled rather than starting.
        function test_theRingKeepsMovingAtZeroPercent() {
            let canvas = find("progressRingCanvas")
            verify(canvas !== null, "progressRingCanvas not found")

            ringId.progress = 0
            compare(canvas.progress, 0, "the ring took the zero")

            let start = canvas.spinAngle
            tryVerify(function() { return canvas.spinAngle !== start },
                      2000, "and is still moving on it")
        }

        // The spin has to stop once there is a real number to draw, or the arc
        // would be swept away from the fraction it is supposed to be showing.
        function test_theRingStopsSpinningOnceProgressIsKnown() {
            let canvas = find("progressRingCanvas")
            verify(canvas !== null, "progressRingCanvas not found")

            ringId.progress = 0.5
            compare(canvas.progress, 0.5, "the first real number is taken as it is")

            // Two frames apart: an animation still running would have moved it.
            let settled = canvas.spinAngle
            wait(100)
            compare(canvas.spinAngle, settled, "the spin stopped")
        }

        // The aggregate really does run backward — a job at 0.9 finishing next to
        // one at 0.1 takes the mean from 0.50 to 0.10 — so the arc slides to the
        // new number instead of jumping to it.
        function test_theArcSlidesWhenTheAggregateDrops() {
            let canvas = find("progressRingCanvas")
            verify(canvas !== null, "progressRingCanvas not found")

            ringId.progress = 0.9
            compare(canvas.progress, 0.9, "settled on the first number")

            ringId.progress = 0.1
            verify(canvas.progress > 0.1,
                   "the arc is still on its way down, not already there")
            tryCompare(canvas, "progress", 0.1, 2000, "and it gets there")
        }

        // Going back to indeterminate is not a drop to zero — it is the ring
        // giving up on a number, so there is nothing to slide through.
        function test_losingTheNumberDoesNotSlide() {
            let canvas = find("progressRingCanvas")
            verify(canvas !== null, "progressRingCanvas not found")

            ringId.progress = 0.6
            compare(canvas.progress, 0.6, "settled on a real number")

            ringId.progress = -1
            compare(canvas.progress, -1, "and drops straight back to spinning")
        }

        // At Medium the footer's label is hidden and the ring is the only thing
        // left that can carry the count; at Wide the label still has it.
        function test_theCountOnlyAppearsWhenAskedFor() {
            ringId.count = 3

            let label = find("progressRingCount")
            verify(label !== null, "progressRingCount not found")
            verify(!label.visible, "the count stays out of the ring by default")

            ringId.showCount = true
            verify(label.visible, "showCount puts it inside the ring")
            compare(label.text, "3")
        }

        // A cascade between pipelines is busy with nothing registered yet — an
        // empty ring is honest there, a "0" is not.
        function test_aZeroCountIsNotDrawn() {
            ringId.showCount = true
            ringId.count = 0

            verify(!find("progressRingCount").visible, "no count when there is none")

            ringId.count = 1
            verify(find("progressRingCount").visible, "and it appears with the first job")
        }
    }
}
