import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// tst_Map covers the whole export path but needs a live QRhi and skips
// wholesale under the headless offscreen platform, which is how a dead Select
// Area button reached a release. Arming the tool needs no render, so these
// tests drive the real buttons and run everywhere.
MainWindowTest {
    id: rootId

    CWTestCase {
        name: "SelectExportAreaTool"
        when: windowShown

        readonly property string toolChain: "rootId->viewPage->SplitView->renderer->selectionExportAreaTool"

        function findTool() {
            let tool = ObjectFinder.findObjectByChain(rootId.mainWindow, toolChain)
            verify(tool !== null, "the select export area tool was not found")
            return tool
        }

        function findRenderer() {
            let renderer = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->viewPage->SplitView->renderer")
            verify(renderer !== null, "the renderer was not found")
            return renderer
        }

        // The tool overlays the View page but is built by the Map page, so
        // visiting Map is what brings it into the tree.
        function visitMapPage() {
            RootData.pageSelectionModel.gotoPageByName(null, "Map")
            tryVerify(() => RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.objectName === "mapPage",
                      5000, "the Map page never became current")
        }

        // Arms the tool the way a user does: Add Layer on the Map page, which
        // both shows the tool and returns to the View page it overlays.
        function addLayer() {
            visitMapPage()

            let addLayerButton = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->mapPage->SplitView->addLayerButton")
            verify(addLayerButton !== null, "the Add Layer button was not found")
            waitForRendering(addLayerButton)
            mouseClick(addLayerButton)

            tryVerify(() => RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.objectName === "viewPage",
                      5000, "Add Layer never returned to the View page")
        }

        function cleanup() {
            // Reset here rather than at the end of each test, so a failed
            // assertion cannot leave the tool armed over the View page for
            // everything that runs after it.
            let tool = ObjectFinder.findObjectByChain(rootId.mainWindow, toolChain)
            if (tool !== null) {
                let button = findChild(tool, "selectionToolButton")
                if (button !== null) {
                    button.state = "DEACTIVE"
                }
            }
            RootData.pageSelectionModel.gotoPageByName(null, "View")
        }

        function test_theSelectAreaButtonTakesAClick() {
            addLayer()

            let tool = findTool()
            let button = findChild(tool, "selectionToolButton")
            verify(button !== null, "the Select Area button was not found")
            tryVerify(() => button.visible, 5000, "the Select Area button never appeared")
            waitForRendering(button)

            mouseClick(button)

            // A press that lands on an overlay instead leaves the button in
            // INIT, still drawn, and the tool unusable.
            tryVerify(() => button.state === "ACTIVE_STATE", 5000,
                      "the click never reached the Select Area button")

            let interaction = findChild(tool, "selectAreaInteraction")
            verify(interaction !== null, "the select area interaction was not found")
            tryVerify(() => interaction.visible, 5000,
                      "arming the tool makes its interaction the active one")
        }

        function test_theToolSitsAboveTheMapOverlays() {
            // Arming does not move the tool, so building it is enough to
            // measure where it sits.
            visitMapPage()

            let renderer = findRenderer()
            let tool = findTool()

            verify(renderer.zOverlay > renderer.zLabels,
                   "the chrome level is above the label/lead level")
            compare(tool.z, renderer.zOverlay,
                    "and the area-select tool is chrome, not interaction graphics")
        }
    }
}
