import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// Tier-2 per-object appearance rail: an offscreen render job selects an
// appearance slot (cwOffscreenRenderParameters::appearanceSlot) and each render
// object resolves it to its own per-slot uniform. This drives the point cloud's
// per-slot world-radius: a job at an override slot draws the cloud at a different
// sprite size than the live view, with the on-screen view (slot 0) untouched.
MainWindowTest {
    id: rootId

    TestCase {
        name: "OffscreenRenderAppearanceSlot"
        when: windowShown

        function test_appearanceSlotOverridesPointCloudRadius() {
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
                skip("no QRhi on this platform (headless offscreen); run with a GPU-backed platform");
                return;
            }

            verify(OffscreenRenderTester.addSyntheticPointCloud(RootData), "added a synthetic point cloud");

            let sceneManager = RootData.regionSceneManager;
            let liveRadiusBefore = sceneManager.lazLayersSceneNode.worldRadius;

            // Override slot 1 with a much larger radius than the live (slot 0) one.
            const overrideSlot = 1;
            let cloudCount = OffscreenRenderTester.setPointCloudWorldRadiusOverride(
                sceneManager, overrideSlot, liveRadiusBefore * 12.0);
            verify(cloudCount >= 1, "set a world-radius override on at least one cloud (got " + cloudCount + ")");

            // Setting a per-slot override must not touch the live radius.
            compare(sceneManager.lazLayersSceneNode.worldRadius, liveRadiusBefore,
                    "live world-radius unchanged by a per-slot override");

            let livePath = OffscreenRenderTester.tempPngPath("appearance_slot_live");
            let overridePath = OffscreenRenderTester.tempPngPath("appearance_slot_override");
            OffscreenRenderTester.removeFile(livePath);
            OffscreenRenderTester.removeFile(overridePath);

            let outputSize = Qt.size(400, 400);
            OffscreenRenderTester.renderPointCloudFramed(regionViewer, sceneManager, livePath, outputSize, 0);
            OffscreenRenderTester.renderPointCloudFramed(regionViewer, sceneManager, overridePath, outputSize, overrideSlot);

            tryVerify(function() { return OffscreenRenderTester.fileExists(livePath); }, 10000,
                      "live-slot point-cloud PNG was written");
            tryVerify(function() { return OffscreenRenderTester.fileExists(overridePath); }, 10000,
                      "override-slot point-cloud PNG was written");

            let liveOpaque = OffscreenRenderTester.opaqueFraction(livePath);
            let overrideOpaque = OffscreenRenderTester.opaqueFraction(overridePath);

            verify(liveOpaque > 0.005,
                   "the cloud is clearly in frame at the live radius (opaque fraction " + liveOpaque + ")");
            verify(overrideOpaque > liveOpaque * 1.25,
                   "the larger override radius draws substantially more cloud (override " + overrideOpaque
                   + " vs live " + liveOpaque + ")");

            OffscreenRenderTester.removeFile(livePath);
            OffscreenRenderTester.removeFile(overridePath);
        }
    }
}
