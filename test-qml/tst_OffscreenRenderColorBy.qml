import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "OffscreenRenderColorBy"
        when: windowShown

        // Tolerance 70 for red, as in tst_OffscreenRenderSplays: MSAA blends
        // line edges toward the background.
        readonly property int redTolerance: 70
        // Tighter tolerance for "no red": the ramp's tan/brown end sits close
        // enough to red that a loose tolerance would count it.
        readonly property int noRedTolerance: 40
        readonly property int rampTolerance: 40
        readonly property int renderAttempts: 20
        readonly property int rhiWaitAttempts: 40
        readonly property int pollIntervalMs: 50
        readonly property int pngWriteTimeoutMs: 10000
        readonly property size outputSize: Qt.size(800, 600)
        // Phake Cave 3000's scrap paper renders as a neutral gray. The tint
        // recolors it toward the ramp; only the scraps near the pivot stay gray.
        readonly property double paperGray: 0.66
        readonly property int paperTolerance: 16
        readonly property double fullScrapStrength: 1.0
        property double savedScrapStrength: -1.0
        // Loaded once for the whole file: reloading the same project fails to
        // read its note image, and the scrap's streamed texture then never
        // becomes resident, which stalls every offscreen render.
        property bool projectLoaded: false

        function loadAndFindViewer() {
            if (!projectLoaded) {
                TestHelper.loadProjectFromFile(RootData.project,
                    TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));
                projectLoaded = true;
            }

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

        function redFraction(path, tolerance) {
            return OffscreenRenderTester.colorFraction(path, Qt.rgba(1.0, 0.0, 0.0, 1.0), tolerance);
        }

        function paperFraction(path) {
            return OffscreenRenderTester.colorFraction(path, Qt.rgba(paperGray, paperGray, paperGray, 1.0),
                                                       paperTolerance);
        }

        function cleanup() {
            let colorBy = RootData.regionSceneManager.colorBy;
            colorBy.source = ColorBy.None;
            if (savedScrapStrength >= 0.0) {
                colorBy.scrapStrength = savedScrapStrength;
                savedScrapStrength = -1.0;
            }
        }

        function test_defaultStaysRed() {
            let regionViewer = loadAndFindViewer();
            RootData.regionSceneManager.colorBy.source = ColorBy.None;

            let path = OffscreenRenderTester.tempPngPath("offscreen_colorby_default");
            renderUntil(regionViewer, path, function(p) { return redFraction(p, redTolerance) > 0.0; });

            let fraction = redFraction(path, redTolerance);
            verify(fraction > 0.0, "centerline red is in the render (got " + fraction + ")");

            OffscreenRenderTester.removeFile(path);
        }

        function test_depthRemovesRedAndAddsRamp() {
            let regionViewer = loadAndFindViewer();
            let colorBy = RootData.regionSceneManager.colorBy;
            colorBy.rampId = "depth";
            colorBy.pivot = 0;
            colorBy.source = ColorBy.Depth;

            let path = OffscreenRenderTester.tempPngPath("offscreen_colorby_depth");
            let white = Qt.rgba(1.0, 1.0, 1.0, 1.0);
            let navy = Qt.rgba(0.118, 0.106, 0.294, 1.0);
            let rampFraction = function(p) {
                return OffscreenRenderTester.colorFraction(p, white, rampTolerance)
                    + OffscreenRenderTester.colorFraction(p, navy, rampTolerance);
            };
            renderUntil(regionViewer, path, function(p) {
                return redFraction(p, noRedTolerance) === 0.0 && rampFraction(p) > 0.0;
            });

            let red = redFraction(path, noRedTolerance);
            compare(red, 0.0, "no centerline red under depth coloring (got " + red + ")");
            verify(OffscreenRenderTester.imageIsNonUniform(path), "the render is non-uniform");
            let ramp = rampFraction(path);
            verify(ramp > 0.0, "white or deep navy from the depth ramp is in the render (got " + ramp + ")");
            // The PNG stays in the temp dir so the checkpoint report can show it.
            console.log("color-by depth render: " + path);
        }

        function test_scrapTintFollowsDepth() {
            let regionViewer = loadAndFindViewer();
            // Scraps triangulate after the solve; render only once they exist.
            TestHelper.waitForFutureManagerToFinish(RootData.futureManagerModel);
            let colorBy = RootData.regionSceneManager.colorBy;
            savedScrapStrength = colorBy.scrapStrength;
            colorBy.scrapStrength = fullScrapStrength;
            colorBy.rampId = "depth";
            colorBy.pivot = 0;

            colorBy.source = ColorBy.None;
            let offPath = OffscreenRenderTester.tempPngPath("offscreen_colorby_scrap_off");
            renderUntil(regionViewer, offPath, function(p) {
                return redFraction(p, redTolerance) > 0.0 && paperFraction(p) > 0.0;
            });
            let offPaper = paperFraction(offPath);
            verify(offPaper > 0.0, "untinted scrap paper is in the render (got " + offPaper + ")");

            colorBy.source = ColorBy.Depth;
            let onPath = OffscreenRenderTester.tempPngPath("offscreen_colorby_scrap_on");
            renderUntil(regionViewer, onPath, function(p) {
                return redFraction(p, noRedTolerance) === 0.0 && paperFraction(p) < offPaper;
            });

            verify(!OffscreenRenderTester.imagesEqual(offPath, onPath), "depth coloring changes the render");
            let onPaper = paperFraction(onPath);
            verify(onPaper < offPaper,
                   "depth tints the scrap paper (paper fraction off " + offPaper + ", on " + onPaper + ")");
            // Both PNGs stay in the temp dir so the checkpoint report can show them.
            console.log("color-by scrap render off: " + offPath);
            console.log("color-by scrap render on: " + onPath);
        }
    }
}
