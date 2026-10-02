import QtQuick
import QtQuick.Controls as QC
import QtTest
import cavewherelib

MainWindowTest {
    id: rootId

    Component {
        id: indicatorComponent
        QC.BusyIndicator {
            running: true
        }
    }

    TestCase {
        name: "BusyRing"
        when: windowShown

        // The style's default size plays the droplet; the app's 16 px uses skip it.
        function sizes() {
            return [{ tag: "default", size: 0 },
                    { tag: "small", size: 16 }]
        }

        function createIndicator(data) {
            const indicator = createTemporaryObject(indicatorComponent, rootId)
            verify(indicator !== null, "the busy indicator should be created")
            if (data.size > 0) {
                indicator.width = data.size
                indicator.height = data.size
            }
            return indicator
        }

        function test_spinsWhileRunning_data() { return sizes() }
        function test_spinsWhileRunning(data) {
            const indicator = createIndicator(data)
            const ring = findChild(indicator, "busyRing")
            verify(ring !== null, "the style's busy indicator should hold a BusyRing")

            // The ring settles at full size and opacity once any opening has played.
            tryVerify(() => ring.parent.opacity === 1 && ring.parent.scale === 1, 2000,
                      "the ring should finish its opening")
            compare(indicator.contentItem.opacity, 1)

            const startRotation = ring.rotation
            tryVerify(() => ring.rotation !== startRotation, 1000, "the ring should turn")
        }

        function test_stopFadesOut_data() { return sizes() }
        function test_stopFadesOut(data) {
            const indicator = createIndicator(data)
            tryCompare(indicator.contentItem, "opacity", 1)

            indicator.running = false
            tryCompare(indicator.contentItem, "opacity", 0, 2000)

            const ring = findChild(indicator, "busyRing")
            indicator.running = true
            if (data.size === 0) {
                tryVerify(() => ring.parent.scale < 1, 1000, "the droplet opening should grow the ring in")
            } else {
                compare(ring.parent.scale, 1, "a small ring skips the droplet")
            }
            tryCompare(indicator.contentItem, "opacity", 1, 2000)
            tryVerify(() => ring.parent.opacity === 1 && ring.parent.scale === 1, 2000,
                      "a restart should bring the ring back to full size")
        }
    }
}
