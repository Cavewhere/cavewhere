import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// The fix-station pick: the crosshair on a fix row hands the fix to the 3D
// view, the view's tool writes the clicked point back to that row, and the user
// lands where they started. Every way a pick can end without an answer is
// exercised here too — Escape, navigating away, and asking for a second pick
// while one stands.
MainWindowTest {
    id: rootId

    TestCase {
        name: "FixStationPickTool"
        when: windowShown

        property int defaultWindowWidth: 0

        Component.onCompleted: defaultWindowWidth = rootId.width

        //! The frame the anchoring fix derives, in the conterminous US — so the
        //! project's plate-fixed datum is NAD83(2011) rather than the WGS84
        //! fallback, and the pick has a datum of its own to land on.
        readonly property double anchorLatitude: 37.0
        readonly property double anchorLongitude: -84.0
        readonly property string plateFixedDatum: "EPSG:6318"

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
            rootId.width = defaultWindowWidth
            FixStationPick.cancel()
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.newProject()
        }

        //! A cave on its Fix Stations page with two rows: row 0 anchors the
        //! project's frame, row 1 is the blank row every test here picks for.
        function gotoFixStations() {
            RootData.region.addCave()
            const cave = RootData.region.cave(RootData.region.caveCount - 1)
            cave.name = "PickCave"

            const model = cave.fixStations
            // The system goes in before the numbers, always — it decides which
            // axis the coordinate leads with.
            compare(model.addFixStation("A1"), 0)
            model.setData(model.index(0), "EPSG:4326", FixStationModel.InputCSRole)
            model.setData(model.index(0), anchorLongitude, FixStationModel.EastingRole)
            model.setData(model.index(0), anchorLatitude, FixStationModel.NorthingRole)
            model.setData(model.index(0), 300.0, FixStationModel.ElevationRole)

            compare(model.addFixStation("B2"), 1)

            tryVerify(() => RootData.region.geoReference.hasCoordinateSystem, 5000,
                      "the anchoring fix should have given the project a frame")

            RootData.pageSelectionModel.currentPageAddress =
                    "Source/Data/Cave=" + cave.name + "/Fix Stations"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "fixStationPage",
                      5000, "should land on fixStationPage")
            return cave
        }

        function findPickButton(rowIndex) {
            let button = null
            tryVerify(() => {
                button = findChild(RootData.pageView.currentPageItem,
                                   "pickFromViewButton." + rowIndex)
                return button !== null
            }, 5000, "row " + rowIndex + " pick button should be reachable")
            return button
        }

        function renderer() {
            let view = null
            tryVerify(() => {
                view = ObjectFinder.findObjectByChain(rootId.mainWindow,
                    "rootId->viewPage->SplitView->renderer")
                return view !== null
            }, 5000, "renderer should be reachable")
            return view
        }

        //! Steps the terrain probe walks the view in, and the top strip the
        //! tool's banner hangs in — a click landing there would be answered by
        //! the banner rather than by the scene.
        readonly property int probeStep: 20
        readonly property int bannerBand: 120

        //! A point on the terrain, in renderer coordinates, that the picker's
        //! ray-cast lands on. Probed rather than assumed: what the camera frames
        //! depends on the project and on the size of the window. Leaves the page
        //! it was called from current.
        function findTerrainPoint() {
            const address = RootData.pageSelectionModel.currentPageAddress
            RootData.pageSelectionModel.currentPageAddress = "View"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "viewPage",
                      5000, "should reach the view page to probe it")

            const view = renderer()
            RootData.futureManagerModel.waitForFinished()
            waitForRendering(rootId)

            const picker = view.coordinatePickerInteraction
            let found = null
            for (let x = probeStep; x < view.width && found === null; x += probeStep) {
                for (let y = probeStep + bannerBand; y < view.height; y += probeStep) {
                    picker.pick(Qt.point(x, y))
                    if (picker.hasPick) {
                        found = Qt.point(x, y)
                        break
                    }
                }
            }
            picker.clearPick()
            verify(found !== null, "the loaded project should put geometry under the camera")

            RootData.pageSelectionModel.currentPageAddress = address
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "fixStationPage",
                      5000, "should be back on the page the probe was started from")
            return found
        }

        function pickTool() {
            const tool = findChild(renderer(), "fixStationPickTool")
            verify(tool !== null, "the renderer should host a fix station pick tool")
            return tool
        }

        //! Starts a pick for row \a rowIndex through the button the user sees,
        //! and hands back the armed tool.
        function startPick(rowIndex) {
            waitForRendering(rootId)
            const button = findPickButton(rowIndex)
            verify(button.canPick, "the pick button needs a frame before it can pick")
            mouseClick(button)

            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "viewPage",
                      5000, "the pick should jump to the 3D view")

            const tool = pickTool()
            tryVerify(() => tool.visible, 5000, "the pick tool should be showing")
            return tool
        }

        function test_theButtonBeginsAPickAndArmsThePicker() {
            const cave = gotoFixStations()

            compare(FixStationPick.active, false)

            const tool = startPick(1)

            compare(FixStationPick.active, true)
            compare(FixStationPick.cave, cave)
            compare(FixStationPick.stationName, "B2")
            verify(FixStationPick.fixId !== "", "the pick should name the fix it places")

            // The generic seam answered the ask and holds nothing afterward.
            compare(ViewTools.pendingTool, "")

            const view = renderer()
            tryCompare(view.interactionManager, "activeInteraction",
                       view.coordinatePickerInteraction, 5000)

            // The banner names the station the next click places.
            const help = findChild(tool, "fixStationPickHelpBox")
            verify(help !== null, "the tool should carry a banner")
            compare(help.text, "Click the terrain to place B2. Esc cancels.")
        }

        function test_aPickWritesTheRowAndComesBack() {
            const cave = gotoFixStations()
            const model = cave.fixStations
            const tool = startPick(1)

            tool.commitPick(Qt.vector3d(0, 0, 300))

            // The scene origin is the frame origin, which the anchoring fix put
            // at the coordinate above — so this pick lands back on it, expressed
            // on the project's own datum rather than on WGS84.
            const row = model.index(1)
            compare(model.data(row, FixStationModel.InputCSRole), plateFixedDatum)
            fuzzyCompare(model.data(row, FixStationModel.NorthingRole), anchorLatitude, 1e-6)
            fuzzyCompare(model.data(row, FixStationModel.EastingRole), anchorLongitude, 1e-6)
            fuzzyCompare(model.data(row, FixStationModel.ElevationRole), 300.0, 1e-4)

            // A ray-cast hands back every bit the double has. The row keeps the
            // millimeter a cave surveyor can act on (cwUnits::lengthDecimals),
            // the same precision the picker's own readout offers for copying.
            const text = model.data(row, FixStationModel.CoordinateTextRole)
            verify(/, -?\d+\.\d{3}m$/.test(text),
                   "the picked elevation reads to the millimeter: " + text)

            // Nothing in this project declares a vertical datum, so the pick
            // claims no more about its elevation than a row typed by hand does.
            // (Which reference a pick over lidar tiles writes is a question for
            // the C++ tests, where a tile's compound WKT can be set up.)
            compare(model.data(row, FixStationModel.ElevationReferenceRole),
                    model.data(model.index(0), FixStationModel.ElevationReferenceRole))

            compare(FixStationPick.active, false)
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "fixStationPage",
                      5000, "the pick should return to the page it was started from")
        }

        function test_escapeCancelsThePick() {
            const cave = gotoFixStations()
            const model = cave.fixStations
            const tool = startPick(1)
            compare(FixStationPick.active, true)

            // Aim at the terrain first, which is what a user does between
            // arming the pick and giving up on it. Moving the pointer over the
            // view hands the focus to GLTerrainRenderer — it wants the keyboard
            // for hold-P + wheel — so this is the focus an Escape really
            // arrives on, and a focus-scoped handler on the picker would be
            // dead by now.
            const view = renderer()
            mouseMove(view, view.width / 2, view.height / 2)
            tryVerify(() => view.activeFocus, 5000,
                      "hovering the view should hand it the keyboard focus")
            verify(!tool.picker.activeFocus,
                   "so the picker is not the item Escape is delivered to")

            keyClick(Qt.Key_Escape)

            tryCompare(FixStationPick, "active", false, 5000)
            compare(model.data(model.index(1), FixStationModel.CoordinateTextRole), "")
            tryVerify(() => !tool.visible, 5000, "and the tool stands down")
        }

        function test_theBannerHangsFromTheTopOfTheView() {
            gotoFixStations()
            const tool = startPick(1)

            const help = findChild(tool, "fixStationPickHelpBox")
            verify(help !== null, "the tool should carry a banner")
            // Where every banner over the 3D scene sits — the measurement tool's
            // and the clip tool's included. HelpBox itself defaults to the
            // bottom, which is the notes editor's convention, not this view's.
            compare(help.y, tool.bannerMargin)
            verify(help.y + help.height < tool.height / 2,
                   "the banner belongs in the top half of the view")
        }

        function test_navigatingAwayCancelsThePick() {
            const cave = gotoFixStations()
            startPick(1)

            RootData.pageSelectionModel.currentPageAddress =
                    "Source/Data/Cave=" + cave.name + "/Fix Stations"

            tryCompare(FixStationPick, "active", false, 5000)

            const tool = pickTool()
            tryVerify(() => !tool.visible, 5000, "the tool should stand down")
        }

        function test_arrivingOnTheViewPageDoesNotCancelThePick() {
            gotoFixStations()
            const tool = startPick(1)

            // The jump the button itself makes changes the current address while
            // the pick stands; only a jump somewhere else may cancel it.
            compare(FixStationPick.active, true)
            compare(tool.visible, true)
        }

        function test_askingForASecondPickRetargetsTheOne() {
            const cave = gotoFixStations()
            const model = cave.fixStations

            startPick(1)
            const firstFix = FixStationPick.fixId

            // Back to the page, and ask for the other row instead.
            RootData.pageSelectionModel.back()
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "fixStationPage",
                      5000, "should be back on the fix station page")

            const tool = startPick(0)
            compare(FixStationPick.active, true)
            verify(FixStationPick.fixId !== firstFix,
                   "the pick should now name the row asked for second")
            compare(FixStationPick.stationName, "A1")

            tool.commitPick(Qt.vector3d(0, 0, 300))

            // One pick means one write: the row abandoned along the way is
            // still blank.
            compare(FixStationPick.active, false)
            compare(model.data(model.index(1), FixStationModel.CoordinateTextRole), "")
        }

        function test_theNarrowLayoutOffersThePickToo() {
            rootId.width = Theme.breakpointPanelCollapse - 100

            gotoFixStations()
            waitForRendering(rootId)

            const button = findPickButton(1)
            verify(button.canPick, "the narrow layout's pick button should be usable")
            mouseClick(button)

            tryCompare(FixStationPick, "active", true, 5000)
            compare(FixStationPick.stationName, "B2")
        }

        function test_thePickButtonWaitsForAFrame() {
            RootData.region.addCave()
            const cave = RootData.region.cave(RootData.region.caveCount - 1)
            cave.name = "NoFrameCave"
            compare(cave.fixStations.addFixStation("A1"), 0)

            RootData.pageSelectionModel.currentPageAddress =
                    "Source/Data/Cave=" + cave.name + "/Fix Stations"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "fixStationPage",
                      5000, "should land on fixStationPage")

            verify(!RootData.region.geoReference.hasCoordinateSystem,
                   "a fix with no coordinate anchors no frame")

            const button = findPickButton(0)
            tryCompare(button, "canPick", false, 5000)

            // A grayed-out crosshair with no explanation is the state a user is
            // most likely to be stuck in, so the tooltip has to say what gives
            // the project a frame.
            verify(button.helpText.indexOf("fix station") >= 0,
                   "the disabled tooltip should name typing a coordinate: " + button.helpText)
            verify(button.helpText.indexOf("georeferenced layer") >= 0,
                   "and adding a georeferenced layer: " + button.helpText)

            // Qt delivers no hover to a disabled item, so the crosshair the user
            // can't use has to stay enabled itself and disable the button inside
            // it — otherwise the tooltip explaining the gray never comes up.
            compare(button.enabled, true,
                    "the wrapper stays enabled so the tooltip still answers a hover")
        }

        function test_theEnabledCrosshairSaysWhatItDoes() {
            gotoFixStations()
            const button = findPickButton(1)
            tryCompare(button, "canPick", true, 5000)

            verify(button.helpText.indexOf("3D view") >= 0,
                   "the enabled tooltip should say where the click goes: " + button.helpText)
        }

        //! The click a user makes, start to finish: a project with survey
        //! geometry under the camera, a real press on the terrain, and the
        //! answer landing on the row. This is the one test that goes through the
        //! picker's own tap handler and the tool's pickChanged wiring — the
        //! others hand commitPick() a point, which says nothing about whether a
        //! click ever reaches it.
        function test_aClickOnTheTerrainWritesTheRow() {
            TestHelper.loadProjectFromFile(RootData.project,
                TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"))

            const cave = gotoFixStations()
            const model = cave.fixStations
            const terrainPoint = findTerrainPoint()

            const view = renderer()
            const popup = findChild(view, "coordinatePickerPopup")
            verify(popup !== null, "the renderer should host the picker popup")
            popupVisibleSpy.target = popup
            popupVisibleSpy.clear()

            startPick(1)
            mouseClick(view, terrainPoint.x, terrainPoint.y)

            tryVerify(() => model.data(model.index(1),
                                       FixStationModel.CoordinateTextRole) !== "",
                      5000, "the click should place the fix it was started for")
            compare(model.data(model.index(1), FixStationModel.InputCSRole),
                    RootData.region.defaultFixDatum)

            compare(FixStationPick.active, false)
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "fixStationPage",
                      5000, "the click should return to the page it was started from")

            // The readout the Pick tool shows for copying numbers out of would
            // only flash on the way back here, so it never comes up at all.
            compare(popupVisibleSpy.count, 0,
                    "the picker popup should stay down for the whole pick")
        }

        SignalSpy {
            id: popupVisibleSpy
            signalName: "visibleChanged"
        }
    }
}
