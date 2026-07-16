import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// Per-object appearance overrides carried ON the offscreen render job
// (cwOffscreenRenderParameters::appearanceOverrides). A job attaches a
// cwPointCloudAppearance for a cloud; the renderer acquires a transient appearance
// slot, uploads the payload, and draws the cloud at an overridden world radius —
// the live view is untouched, and two jobs overriding the same cloud in one batch
// get distinct slots that don't collide.
MainWindowTest {
    id: rootId

    TestCase {
        name: "OffscreenRenderAppearanceSlot"
        when: windowShown

        // Load the project, add one synthetic cloud, and return the RegionViewer
        // once its window has a live QRhi; null means this platform is headless
        // (no readback) and the caller should skip. Each call reloads the project,
        // so exactly one cloud is present per test.
        function prepareRegionViewer() {
            TestHelper.loadProjectFromFile(RootData.project,
                TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));

            let glTerrain = ObjectFinder.findObjectByChain(rootId.mainWindow,
                "rootId->viewPage->SplitView->renderer");
            verify(glTerrain, "found the GLTerrainRenderer");
            let regionViewer = glTerrain.renderer;
            verify(regionViewer, "found the RegionViewer");

            // The readback path needs a live QRhi; the headless offscreen QPA has none.
            for (let i = 0; i < 40 && !OffscreenRenderTester.windowHasRhi(regionViewer); ++i) {
                wait(50);
            }
            if (!OffscreenRenderTester.windowHasRhi(regionViewer)) {
                return null;
            }

            verify(OffscreenRenderTester.addSyntheticPointCloud(RootData), "added a synthetic point cloud");

            // The framed-render helpers frame clouds.first(); the override is keyed to
            // that same cloud. Assert the precondition the tests rely on — exactly one
            // cloud — rather than trusting the project reload to have cleared prior ones
            // (tests run alphabetically and each reloads the project).
            compare(OffscreenRenderTester.visiblePointCloudCount(RootData.regionSceneManager), 1,
                    "exactly one visible point cloud after setup");
            return regionViewer;
        }

        function test_jobOverrideEnlargesPointCloud() {
            let regionViewer = prepareRegionViewer();
            if (!regionViewer) {
                skip("no QRhi on this platform (headless offscreen); run with a GPU-backed platform");
                return;
            }

            let sceneManager = RootData.regionSceneManager;
            let liveRadius = sceneManager.lazLayersSceneNode.worldRadius;

            let livePath = OffscreenRenderTester.tempPngPath("appearance_live");
            let overridePath = OffscreenRenderTester.tempPngPath("appearance_override");
            OffscreenRenderTester.removeFile(livePath);
            OffscreenRenderTester.removeFile(overridePath);

            let outputSize = Qt.size(400, 400);
            // <= 0 = no override on the job (live radius); > 0 attaches the on-job payload.
            OffscreenRenderTester.renderPointCloudFramed(regionViewer, sceneManager, livePath, outputSize, 0.0);
            OffscreenRenderTester.renderPointCloudFramed(regionViewer, sceneManager, overridePath, outputSize,
                                                         liveRadius * 12.0);

            tryVerify(function() { return OffscreenRenderTester.fileExists(livePath); }, 10000,
                      "live point-cloud PNG was written");
            tryVerify(function() { return OffscreenRenderTester.fileExists(overridePath); }, 10000,
                      "override point-cloud PNG was written");

            let liveOpaque = OffscreenRenderTester.opaqueFraction(livePath);
            let overrideOpaque = OffscreenRenderTester.opaqueFraction(overridePath);

            verify(liveOpaque > 0.005,
                   "the cloud is clearly in frame at the live radius (opaque fraction " + liveOpaque + ")");
            verify(overrideOpaque > liveOpaque * 1.25,
                   "the on-job radius override draws substantially more cloud (override " + overrideOpaque
                   + " vs live " + liveOpaque + ")");

            OffscreenRenderTester.removeFile(livePath);
            OffscreenRenderTester.removeFile(overridePath);
        }

        function test_concurrentOverridesDoNotInterfere() {
            let regionViewer = prepareRegionViewer();
            if (!regionViewer) {
                skip("no QRhi on this platform (headless offscreen); run with a GPU-backed platform");
                return;
            }

            let sceneManager = RootData.regionSceneManager;
            let liveRadius = sceneManager.lazLayersSceneNode.worldRadius;

            let smallPath = OffscreenRenderTester.tempPngPath("appearance_pair_small");
            let largePath = OffscreenRenderTester.tempPngPath("appearance_pair_large");
            OffscreenRenderTester.removeFile(smallPath);
            OffscreenRenderTester.removeFile(largePath);

            // Two jobs overriding the SAME cloud, issued together so they batch through
            // the atlas path with both override slots live at once. If their slots
            // collided, both would read the same radius and draw the same coverage.
            let written = OffscreenRenderTester.renderPointCloudFramedPair(
                regionViewer, sceneManager,
                smallPath, liveRadius * 2.0,
                largePath, liveRadius * 12.0,
                Qt.size(400, 400));
            compare(written, 2, "both batched override renders were written");

            let smallOpaque = OffscreenRenderTester.opaqueFraction(smallPath);
            let largeOpaque = OffscreenRenderTester.opaqueFraction(largePath);

            verify(smallOpaque > 0.005,
                   "the small-radius job drew the cloud (opaque fraction " + smallOpaque + ")");
            verify(largeOpaque > smallOpaque * 1.25,
                   "the large-radius job in the same batch drew more than the small-radius job, so their "
                   + "appearance slots did not collide (large " + largeOpaque + " vs small " + smallOpaque + ")");

            OffscreenRenderTester.removeFile(smallPath);
            OffscreenRenderTester.removeFile(largePath);
        }
    }
}
