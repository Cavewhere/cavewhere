import QtQuick
import QtTest
import cavewherelib
import cw.TestLib

MainWindowTest {
    id: rootId

    TestCase {
        name: "GeospatialLayerPage"
        when: windowShown

        function init() {
            if (GlobalShadowTextInput.coreClickInput !== null) {
                GlobalShadowTextInput.coreClickInput.closeEditor()
            }
            GlobalShadowTextInput.enabled = false

            RootData.futureManagerModel.waitForFinished()
            RootData.newProject()
            RootData.pageSelectionModel.currentPageAddress = "View"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "viewPage",
                      5000, "should be on view page at start of test")
        }

        function cleanup() {
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.futureManagerModel.waitForFinished()
            RootData.newProject()
        }

        function gotoGeospatialLayers() {
            // Visit Source/Data first so DataMainPage registers the
            // "Geospatial Layers" sub-page.
            RootData.pageSelectionModel.currentPageAddress = "Source/Data"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "dataMainPage",
                      5000, "should land on dataMainPage")

            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Geospatial Layers"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "geospatialLayerPage",
                      5000, "should land on geospatialLayerPage")
        }

        function waitForLazLoadsToFinish() {
            // LAZ loads are wrapped in cwFutures and registered with the
            // global future manager — wait there instead of polling each layer.
            RootData.futureManagerModel.waitForFinished()
        }

        function test_emptyStateShowsHelpBox() {
            gotoGeospatialLayers()

            const page = RootData.pageView.currentPageItem
            const help = findChild(page, "noGeospatialLayersHelpBox")
            verify(help !== null, "noGeospatialLayersHelpBox must exist")
            tryVerify(() => help.visible, 2000, "help box should be visible for empty layer list")

            const tableView = findChild(page, "geospatialLayerTableView")
            verify(tableView !== null, "geospatialLayerTableView must exist")
            tryCompare(tableView, "count", 0)
        }

        function test_addLayerViaModel() {
            gotoGeospatialLayers()

            const lazPath = TestHelper.writeMinimalLazInTempDir("addviamodel")
            verify(lazPath.length > 0, "fixture should produce a non-empty path")

            RootData.region.lazLayers.addFromFiles([Qt.url("file://" + lazPath)])
            tryCompare(RootData.region.lazLayers, "count", 1)

            waitForLazLoadsToFinish()

            const page = RootData.pageView.currentPageItem
            const tableView = findChild(page, "geospatialLayerTableView")
            tryCompare(tableView, "count", 1)

            const help = findChild(page, "noGeospatialLayersHelpBox")
            tryVerify(() => !help.visible, 2000, "help box should hide once a layer is added")

            // Layer's per-load state populates after the future settles.
            const layer = RootData.region.lazLayers.layerAt(0)
            verify(layer !== null, "layerAt(0) should return a layer")
            tryCompare(layer, "loadStatus", LazLayer.Loaded)
            verify(layer.pointCount > 0, "minimal LAZ should have points")
        }

        function test_addLayerViaFileDialogPath() {
            gotoGeospatialLayers()

            const lazPath = TestHelper.writeMinimalLazInTempDir("addviahelper")
            verify(lazPath.length > 0)

            // The FileDialog itself isn't drivable on --platform offscreen, so
            // exercise the addLazFiles(urls) helper directly. This still
            // covers the importPathFromUrl conversion path.
            const page = RootData.pageView.currentPageItem
            page.addLazFiles([Qt.url("file://" + lazPath)])

            tryCompare(RootData.region.lazLayers, "count", 1)
            waitForLazLoadsToFinish()

            const layer = RootData.region.lazLayers.layerAt(0)
            verify(layer !== null)
            tryCompare(layer, "loadStatus", LazLayer.Loaded)
        }

        function test_removeLayerConfirmed() {
            gotoGeospatialLayers()

            const lazPath = TestHelper.writeMinimalLazInTempDir("removeconfirmed")
            RootData.region.lazLayers.addFromFiles([Qt.url("file://" + lazPath)])
            tryCompare(RootData.region.lazLayers, "count", 1)
            waitForLazLoadsToFinish()

            const page = RootData.pageView.currentPageItem
            const removeAskBox = findChild(page, "removeChallange")
            verify(removeAskBox !== null, "RemoveAskBox must exist on geospatialLayerPage")

            removeAskBox.indexToRemove = 0
            removeAskBox.removeName = "test"
            removeAskBox.show()
            tryVerify(() => removeAskBox.visible)

            const removeButton = findChild(removeAskBox, "removeButton")
            mouseClick(removeButton)
            tryVerify(() => !removeAskBox.visible)

            tryCompare(RootData.region.lazLayers, "count", 0)

            const help = findChild(page, "noGeospatialLayersHelpBox")
            tryVerify(() => help.visible, 2000, "help box returns when last layer removed")
        }

        function test_removeLayerCancelled() {
            gotoGeospatialLayers()

            const lazPath = TestHelper.writeMinimalLazInTempDir("removecancelled")
            RootData.region.lazLayers.addFromFiles([Qt.url("file://" + lazPath)])
            tryCompare(RootData.region.lazLayers, "count", 1)
            waitForLazLoadsToFinish()

            const page = RootData.pageView.currentPageItem
            const removeAskBox = findChild(page, "removeChallange")
            removeAskBox.indexToRemove = 0
            removeAskBox.removeName = "test"
            removeAskBox.show()
            tryVerify(() => removeAskBox.visible)

            mouseClick(findChild(removeAskBox, "cancelButton"))
            tryVerify(() => !removeAskBox.visible)

            compare(RootData.region.lazLayers.count, 1)
        }

        function test_noProjectCSHelpBoxShowsForNoCSLayers() {
            gotoGeospatialLayers()

            const page = RootData.pageView.currentPageItem
            const noCSHelp = findChild(page, "noProjectCSHelpBox")
            verify(noCSHelp !== null, "noProjectCSHelpBox must exist")
            tryVerify(() => !noCSHelp.visible, 2000,
                      "no-CS hint should be hidden on a fresh project")

            // TestHelper writes a no-CS fixture, so auto-adopt sets a worldOrigin
            // but leaves globalCoordinateSystem empty — exactly the state that
            // should surface the hint to the user.
            const lazPath = TestHelper.writeMinimalLazInTempDir("nocshint")
            RootData.region.lazLayers.addFromFiles([Qt.url("file://" + lazPath)])
            tryCompare(RootData.region.lazLayers, "count", 1)
            waitForLazLoadsToFinish()

            compare(RootData.region.globalCoordinateSystem, "",
                    "globalCoordinateSystem must stay empty for a no-CS LAZ")
            tryVerify(() => noCSHelp.visible, 2000,
                      "no-CS hint should appear once a no-CS layer is present")
        }

        function test_toggleEnabledViaCheckBox() {
            gotoGeospatialLayers()

            const lazPath = TestHelper.writeMinimalLazInTempDir("toggleenabled")
            RootData.region.lazLayers.addFromFiles([Qt.url("file://" + lazPath)])
            tryCompare(RootData.region.lazLayers, "count", 1)
            waitForLazLoadsToFinish()

            const layer = RootData.region.lazLayers.layerAt(0)
            verify(layer !== null)
            tryCompare(layer, "enabled", true)

            const page = RootData.pageView.currentPageItem
            const tableView = findChild(page, "geospatialLayerTableView")
            tryCompare(tableView, "count", 1)

            const toggle = findChild(tableView, "enabledToggle")
            verify(toggle !== null, "enabledToggle must exist on the delegate")
            tryCompare(toggle, "checked", true)

            mouseClick(toggle)
            tryCompare(layer, "enabled", false)
            tryCompare(toggle, "checked", false)

            mouseClick(toggle)
            tryCompare(layer, "enabled", true)
            tryCompare(toggle, "checked", true)
        }

        function test_disabledRowDimsAndShowsChip() {
            gotoGeospatialLayers()

            const lazPath = TestHelper.writeMinimalLazInTempDir("disabledchip")
            RootData.region.lazLayers.addFromFiles([Qt.url("file://" + lazPath)])
            tryCompare(RootData.region.lazLayers, "count", 1)
            waitForLazLoadsToFinish()

            const layer = RootData.region.lazLayers.layerAt(0)
            const page = RootData.pageView.currentPageItem
            const tableView = findChild(page, "geospatialLayerTableView")
            tryCompare(tableView, "count", 1)

            const chip = findChild(tableView, "disabledChip")
            verify(chip !== null, "disabledChip must exist on the delegate")
            tryCompare(chip, "visible", false)

            layer.enabled = false
            tryCompare(chip, "visible", true)

            layer.enabled = true
            tryCompare(chip, "visible", false)
        }

        function test_geospatialLayersLinkCount() {
            RootData.pageSelectionModel.currentPageAddress = "Source/Data"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "dataMainPage",
                      5000, "should land on dataMainPage")

            const link = findChild(RootData.pageView.currentPageItem, "geospatialLayersLink")
            verify(link !== null, "geospatialLayersLink must exist on dataMainPage")
            tryCompare(link, "text", "0")

            const lazPathA = TestHelper.writeMinimalLazInTempDir("linkcountA")
            const lazPathB = TestHelper.writeMinimalLazInTempDir("linkcountB")
            RootData.region.lazLayers.addFromFiles([Qt.url("file://" + lazPathA),
                                                    Qt.url("file://" + lazPathB)])
            tryCompare(link, "text", "2")

            waitForLazLoadsToFinish()
        }
    }
}
