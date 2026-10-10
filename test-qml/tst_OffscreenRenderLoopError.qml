import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// Its own file because tst_OffscreenRenderColorBy loads Phake Cave 3000 once.
// network.cw has a loop a1-a2-a3-a4-a1 plus the dead end a3-a5-a6; its loop
// closes exactly, so the test repeats one loop leg with a slightly longer tape
// to give cavern a small misclosure to report.
MainWindowTest {
    id: rootId

    TestCase {
        name: "OffscreenRenderLoopError"
        when: windowShown

        // Tighter tolerance for "no red": the ramp's dark red end is far from
        // pure red, and MSAA edges blend toward the background.
        readonly property int noRedTolerance: 40
        readonly property int rampTolerance: 40
        readonly property int grayTolerance: 16
        readonly property int renderAttempts: 20
        readonly property int rhiWaitAttempts: 40
        readonly property int pollIntervalMs: 50
        readonly property int pngWriteTimeoutMs: 10000
        readonly property int solveTimeoutMs: 20000
        readonly property size outputSize: Qt.size(800, 600)
        // kNoDataColor in LinePlot.frag
        readonly property double noDataGray: 0.55
        // Tape added to the repeated a1-a2 leg, in meters: a small misclosure
        // that stays near the ramp's low end
        readonly property double repeatShotOffset: 0.1
        // The low end of cwColorRamp::loopErrorRamp: dark green at 0, light green at 1 sigma
        readonly property QQ.color rampZeroSigma: "#1a9850"
        readonly property QQ.color rampOneSigma: "#a6d96a"

        function loadAndFindViewer() {
            TestHelper.loadProjectFromFile(RootData.project,
                TestHelper.testcasesDatasetPath("network.cw"));
            tryVerify(() => RootData.linePlotManager.loopClosureStats.length > 0, solveTimeoutMs,
                      "cavern wrote loop closure stats for network.cw");

            let exactStats = RootData.linePlotManager.loopClosureStats;
            let chunk = RootData.region.cave(0).trip(0).chunk(0);
            verify(chunk, "found network.cw's first chunk");
            TestHelper.addRepeatShot(chunk, 0, repeatShotOffset);
            tryVerify(() => RootData.linePlotManager.loopClosureStats !== exactStats, solveTimeoutMs,
                      "the repeated leg re-solves with a misclosure");

            let glTerrain = ObjectFinder.findObjectByChain(rootId.mainWindow,
                "rootId->viewPage->SplitView->renderer");
            verify(glTerrain, "found the GLTerrainRenderer");
            let regionViewer = glTerrain.renderer;
            verify(regionViewer, "found the RegionViewer");

            for (let i = 0; i < rhiWaitAttempts && !OffscreenRenderTester.windowHasRhi(regionViewer); ++i) {
                wait(pollIntervalMs);
            }
            if (!OffscreenRenderTester.windowHasRhi(regionViewer)) {
                skip("no QRhi on this platform (headless offscreen); run with a GPU-backed platform");
            }
            return regionViewer;
        }

        // Renders until done(path) holds, since the uniform reaches the render
        // thread a frame or two after the property change.
        function renderUntil(regionViewer, path, done) {
            for (let attempt = 0; attempt < renderAttempts; ++attempt) {
                OffscreenRenderTester.removeFile(path);
                OffscreenRenderTester.renderToImage(regionViewer, path, outputSize);
                tryVerify(function() { return OffscreenRenderTester.fileExists(path); }, pngWriteTimeoutMs,
                          "offscreen PNG was written");
                if (done(path)) {
                    return;
                }
                wait(pollIntervalMs);
            }
        }

        function redFraction(path) {
            return OffscreenRenderTester.colorFraction(path, Qt.rgba(1.0, 0.0, 0.0, 1.0), noRedTolerance);
        }

        function lowRampFraction(path) {
            return OffscreenRenderTester.colorFraction(path, rampZeroSigma, rampTolerance)
                + OffscreenRenderTester.colorFraction(path, rampOneSigma, rampTolerance);
        }

        function grayFraction(path) {
            return OffscreenRenderTester.colorFraction(path, Qt.rgba(noDataGray, noDataGray, noDataGray, 1.0),
                                                       grayTolerance);
        }

        function cleanup() {
            RootData.regionSceneManager.colorBy.source = ColorBy.None;
        }

        function test_loopErrorColorsLoopLegs() {
            let regionViewer = loadAndFindViewer();
            console.log("network.cw loop closure stats: " + RootData.linePlotManager.loopClosureStats);
            let colorBy = RootData.regionSceneManager.colorBy;
            colorBy.source = ColorBy.LoopError;

            let path = OffscreenRenderTester.tempPngPath("offscreen_colorby_loop_error");
            renderUntil(regionViewer, path, function(p) {
                return redFraction(p) === 0.0 && lowRampFraction(p) > 0.0;
            });

            let red = redFraction(path);
            compare(red, 0.0, "no centerline red under loop error coloring (got " + red + ")");
            let low = lowRampFraction(path);
            verify(low > 0.0, "the loop legs draw near the ramp's low end (got " + low + ")");
            let gray = grayFraction(path);
            verify(gray > 0.0, "the dead end a3-a5-a6 draws in the no-loop gray (got " + gray + ")");
            // The PNG stays in the temp dir so the checkpoint report can show it.
            console.log("color-by loop error render: " + path);
        }
    }
}
