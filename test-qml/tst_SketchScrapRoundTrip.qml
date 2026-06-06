import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

MainWindowTest {
    id: rootId

    SignalSpy {
        id: derivedScrapsSpy
        target: RootData.scrapManager
        signalName: "sketchDerivedScrapsUpdated"
    }

    TestCase {
        name: "SketchScrapRoundTrip"
        when: windowShown

        function init() {
            RootData.project.newProject()
            RootData.pageSelectionModel.currentPageAddress = "View"
            // newProject() triggers async cleanup across futures, scrap manager,
            // and page reloads. tryVerify on viewPage is too narrow to catch them.
            wait(1000)
            derivedScrapsSpy.clear()
        }

        function cleanup() {
            RootData.project.newProject()
            wait(500)
        }

        function test_wallOutlineProducesScrapVisibleInView() {
            // --- Create cave, trip, and a single 10m north shot 1 → 2 ---
            RootData.region.addCave()
            let cave = RootData.region.cave(0)
            cave.name = "TestCave"

            cave.addTrip()
            let trip = cave.trip(0)
            trip.name = "TestTrip"

            trip.addNewChunk()
            let chunk = trip.chunk(0)
            chunk.setData(SurveyChunk.StationNameRole, 0, "1")
            chunk.setData(SurveyChunk.StationNameRole, 1, "2")
            chunk.setData(SurveyChunk.ShotDistanceRole, 0, "10")
            chunk.setData(SurveyChunk.ShotCompassRole, 0, "0")
            chunk.setData(SurveyChunk.ShotClinoRole, 0, "0")

            // Let the line plot compute station positions
            RootData.futureManagerModel.waitForFinished()

            // --- Add a plan sketch anchored at station 1 ---
            let sketch = trip.notesSketch.addSketch(Sketch.Plan)
            verify(sketch !== null, "addSketch should return a Sketch")
            sketch.name = "RoundTripSketch"
            sketch.anchorStation = "1"

            // Wait for the sketch-manager line plot acquisition before drawing
            RootData.futureManagerModel.waitForFinished()

            verify(RootData.scrapManager.derivedScrapCount(sketch) === 0,
                   "no scraps should exist before drawing")

            derivedScrapsSpy.clear()

            // --- Draw a closed Wall polygon surrounding both stations ---
            // Station 1 sits at trip-local (0, 0); station 2 is 10m north
            // (compass=0) so at (0, 10). The box spans well past both in X/Y
            // meters regardless of survey handedness, so stationsForOutline()
            // always picks up at least one station.
            const strokeIdx = sketch.beginStroke("wall")
            const ring = [
                Qt.point(-10, -10),
                Qt.point( 10, -10),
                Qt.point( 10,  20),
                Qt.point(-10,  20),
                Qt.point(-10, -10)
            ]
            for (let i = 0; i < ring.length; i++) {
                sketch.appendPoint(strokeIdx, ring[i], 1.0, i * 10)
            }
            sketch.endStroke()

            tryVerify(() => derivedScrapsSpy.count > 0,
                      3000, "sketchDerivedScrapsUpdated should fire after endStroke")

            tryVerify(() => RootData.scrapManager.derivedScrapCount(sketch) === 1,
                      3000, "one scrap should be derived from the closed Wall stroke")

            RootData.futureManagerModel.waitForFinished()

            // --- Switch back to the View and confirm the scrap is wired to rendering ---
            RootData.pageSelectionModel.currentPageAddress = "View"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                        && RootData.pageView.currentPageItem.objectName === "viewPage",
                      2000, "should land on viewPage")
            waitForRendering(rootId)

            tryVerify(() => RootData.scrapManager.renderScrapCount() >= 1,
                      2000, "scrap should be attached to the 3D render layer")

            compare(RootData.scrapManager.derivedScrapCount(sketch), 1,
                    "scrap remains derived after switching to the View")
        }
    }
}
