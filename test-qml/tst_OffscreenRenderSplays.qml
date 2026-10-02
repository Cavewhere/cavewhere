import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "OffscreenRenderSplays"
        when: windowShown

        // Renders the loaded project to a PNG and asserts both line-plot
        // colors reached the output: the 2 px red centerline and the 1 px
        // light-red splays added to the first station. This is the end-to-end
        // check that the instanced quad-expansion pipeline draws splay
        // segments with their own color.
        function test_splaysRenderInSplayColor() {
            TestHelper.loadProjectFromFile(RootData.project,
                TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));

            let glTerrain = ObjectFinder.findObjectByChain(rootId.mainWindow,
                "rootId->viewPage->SplitView->renderer");
            verify(glTerrain, "found the GLTerrainRenderer");
            let regionViewer = glTerrain.renderer;
            verify(regionViewer, "found the RegionViewer");

            for (let i = 0; i < 40 && !OffscreenRenderTester.windowHasRhi(regionViewer); ++i) {
                wait(50);
            }
            if (!OffscreenRenderTester.windowHasRhi(regionViewer)) {
                skip("no QRhi on this platform (headless offscreen); run with a GPU-backed platform");
                return;
            }

            verify(OffscreenRenderTester.addSplaysToFirstStation(RootData),
                   "added splays to the first station and re-solved");

            let path = OffscreenRenderTester.tempPngPath("offscreen_splays");
            let outputSize = Qt.size(800, 600);

            // The re-solved geometry reaches the render thread a frame or two
            // later, so render-and-check until both colors land rather than
            // trusting one fixed delay.
            // Tolerance 70: a 1 px diagonal splay rarely fully covers a pixel
            // under MSAA, so its pixels blend toward the background. The two
            // targets stay exclusive — green differs by 153 between them.
            let centerlineFraction = 0.0;
            let splayFraction = 0.0;
            for (let attempt = 0; attempt < 20; ++attempt) {
                OffscreenRenderTester.removeFile(path);
                OffscreenRenderTester.renderToImage(regionViewer, path, outputSize);
                tryVerify(function() { return OffscreenRenderTester.fileExists(path); }, 10000,
                          "offscreen PNG was written");

                centerlineFraction = OffscreenRenderTester.colorFraction(
                    path, Qt.rgba(1.0, 0.0, 0.0, 1.0), 70);
                splayFraction = OffscreenRenderTester.colorFraction(
                    path, Qt.rgba(1.0, 0.6, 0.6, 1.0), 70);
                if (centerlineFraction > 0.0 && splayFraction > 0.0) {
                    break;
                }
                wait(50);
            }

            verify(centerlineFraction > 0.0,
                   "centerline red is in the render (got " + centerlineFraction + ")");
            verify(splayFraction > 0.0,
                   "splay light-red is in the render (got " + splayFraction + ")");

            OffscreenRenderTester.removeFile(path);
        }
    }
}
