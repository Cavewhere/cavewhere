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

            compare(RootData.region.geoReference.globalCoordinateSystem, "",
                    "globalCoordinateSystem must stay empty for a no-CS LAZ")
            tryVerify(() => noCSHelp.visible, 2000,
                      "no-CS hint should appear once a no-CS layer is present")
        }

        function test_toggleEnabledViaContextMenu() {
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

            // QC.Menu's child MenuItems live in `contentData`, which isn't
            // reachable through findChild before the popup is materialized.
            // Reach the LazLayerContextMenu wrapper (a QQ.Item) by objectName
            // and grab the first menu item (the Enable/Disable toggle)
            // through its `menu` alias.
            const ctxMenu = findChild(tableView, "lazLayerContextMenu")
            verify(ctxMenu !== null, "lazLayerContextMenu must exist")
            verify(ctxMenu.menu.count >= 1, "context menu must have at least one item")
            const toggleItem = ctxMenu.menu.itemAt(0)
            verify(toggleItem !== null, "first menu item must exist")
            compare(toggleItem.objectName, "enabledToggleMenuItem",
                    "first item must be the enable/disable toggle")
            tryCompare(toggleItem, "text", "Disable")

            toggleItem.triggered()
            tryCompare(layer, "enabled", false)
            tryCompare(toggleItem, "text", "Enable")

            toggleItem.triggered()
            tryCompare(layer, "enabled", true)
            tryCompare(toggleItem, "text", "Disable")
        }

        function test_removeLayerViaContextMenu() {
            gotoGeospatialLayers()

            const lazPath = TestHelper.writeMinimalLazInTempDir("removeviactxmenu")
            RootData.region.lazLayers.addFromFiles([Qt.url("file://" + lazPath)])
            tryCompare(RootData.region.lazLayers, "count", 1)
            waitForLazLoadsToFinish()

            const page = RootData.pageView.currentPageItem
            const tableView = findChild(page, "geospatialLayerTableView")
            tryCompare(tableView, "count", 1)

            // QC.Menu's child MenuItems live in `contentData`; reach them
            // through the LazLayerContextMenu wrapper's `menu` alias. Item 0
            // is the Enable/Disable toggle; item 1 is "Remove <name>".
            const ctxMenu = findChild(tableView, "lazLayerContextMenu")
            verify(ctxMenu !== null, "lazLayerContextMenu must exist")
            verify(ctxMenu.menu.count >= 2, "context menu must have at least two items")
            const removeItem = ctxMenu.menu.itemAt(1)
            verify(removeItem !== null, "remove menu item must exist")
            verify(removeItem.text.indexOf("Remove") === 0,
                   "second item should be 'Remove ...'")

            // Trigger the MenuItem directly — popup() on --platform offscreen
            // is unreliable, but triggered() executes the handler that wires
            // RemoveAskBox.
            removeItem.triggered()

            const removeAskBox = findChild(page, "removeChallange")
            verify(removeAskBox !== null, "RemoveAskBox must exist")
            tryVerify(() => removeAskBox.visible, 2000,
                      "RemoveAskBox should open after context-menu Remove")
            compare(removeAskBox.indexToRemove, 0,
                    "indexToRemove should match the delegate row")

            const removeButton = findChild(removeAskBox, "removeButton")
            mouseClick(removeButton)
            tryVerify(() => !removeAskBox.visible)

            tryCompare(RootData.region.lazLayers, "count", 0)
        }

        function test_rightClickOpensContextMenu() {
            gotoGeospatialLayers()

            const lazPath = TestHelper.writeMinimalLazInTempDir("rightclickopen")
            RootData.region.lazLayers.addFromFiles([Qt.url("file://" + lazPath)])
            tryCompare(RootData.region.lazLayers, "count", 1)
            waitForLazLoadsToFinish()

            const page = RootData.pageView.currentPageItem
            const tableView = findChild(page, "geospatialLayerTableView")
            tryCompare(tableView, "count", 1)

            // Wait for the row delegate to materialize before clicking it.
            let delegate = null
            tryVerify(() => {
                delegate = tableView.itemAtIndex(0)
                return delegate !== null && delegate.width > 0 && delegate.height > 0
            }, 2000, "row delegate at index 0 should materialize")

            const ctxMenu = findChild(tableView, "lazLayerContextMenu")
            verify(ctxMenu !== null, "lazLayerContextMenu must exist")
            verify(!ctxMenu.menu.opened, "context menu should start closed")

            // Right-button TapHandler in LazLayerContextMenu listens for
            // Mouse | TouchPad. mouseClick with Qt.RightButton drives it.
            mouseClick(delegate, delegate.width / 2, delegate.height / 2,
                       Qt.RightButton)

            tryVerify(() => ctxMenu.menu.opened, 2000,
                      "context menu should open after right-click")

            ctxMenu.menu.close()
            tryVerify(() => !ctxMenu.menu.opened, 2000,
                      "context menu should close cleanly")
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

        function test_formatPointCount_data() {
            return [
                { tag: "zero",          value: 0,          expected: "0" },
                { tag: "under-thousand", value: 999,        expected: "999" },
                { tag: "thousand",      value: 1000,       expected: "1,000" },
                { tag: "ten-thousand",  value: 12847,      expected: "12,847" },
                { tag: "hundreds-k",    value: 530200,     expected: "530,200" },
                { tag: "millions",      value: 1423000,    expected: "1,423,000" },
                { tag: "max-int",       value: 2147483647, expected: "2,147,483,647" },
                // Beyond signed 32-bit — pointCount is qint64, so large LAZ
                // clouds must not overflow or render in scientific notation.
                { tag: "over-int32",    value: 3000000000, expected: "3,000,000,000" },
                { tag: "ten-billion",   value: 10000000000, expected: "10,000,000,000" },
            ]
        }

        function test_formatPointCount(data) {
            gotoGeospatialLayers()
            const page = RootData.pageView.currentPageItem
            compare(page.formatPointCount(data.value), data.expected,
                    "point count must be comma-grouped")
        }

        function test_pointCountCellIsCommaGrouped() {
            gotoGeospatialLayers()

            const lazPath = TestHelper.writeMinimalLazInTempDir("pointcountcell")
            RootData.region.lazLayers.addFromFiles([Qt.url("file://" + lazPath)])
            tryCompare(RootData.region.lazLayers, "count", 1)
            waitForLazLoadsToFinish()

            const layer = RootData.region.lazLayers.layerAt(0)
            tryCompare(layer, "loadStatus", LazLayer.Loaded)
            verify(layer.pointCount > 0, "minimal LAZ should have points")

            const page = RootData.pageView.currentPageItem
            const tableView = findChild(page, "geospatialLayerTableView")
            tryCompare(tableView, "count", 1)

            // count == 1 only means the model has a row; the delegate's child
            // tree is created lazily. Wait for it before the one-shot findChild.
            tryVerify(() => {
                const d = tableView.itemAtIndex(0)
                return d !== null && d.width > 0 && d.height > 0
            }, 2000, "row delegate at index 0 should materialize")

            // Assert the actual rendered cell, not a re-derivation: the binding
            // must route the layer's point count through formatPointCount (the
            // narrow delegate appends " pts"). This proves the wiring, not just
            // the helper.
            const cell = findChild(tableView, "pointCountCell")
            verify(cell !== null, "point count cell must exist")
            const formatted = page.formatPointCount(layer.pointCount)
            const expectedText = page.isNarrow ? formatted + " pts" : formatted
            tryCompare(cell, "text", expectedText)
            verify(formatted.indexOf("e") === -1 && formatted.indexOf("E") === -1,
                   "formatted count must not use scientific notation")
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
