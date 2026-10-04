import QtQuick
import QtQuick.Controls as QC
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "StyleGalleryPage"
        when: windowShown

        function cardNames() {
            return ["cardButtons", "cardToggles", "cardIndicators", "cardFields", "cardProgress",
                    "cardMenus", "cardContainers", "cardData", "cardDialogs", "cardCaveWhere"]
        }

        function modes() {
            return [{ name: "light", scheme: AppearanceSettings.Light, dark: false },
                    { name: "dark", scheme: AppearanceSettings.Dark, dark: true }]
        }

        function cleanup() {
            const page = RootData.pageView.currentPageItem
            if (page !== null && page.objectName === "styleGalleryPage") {
                page.closePopups()
            }
            RootData.settings.appearanceSettings.colorScheme = AppearanceSettings.System
            const entries = RootData.settings.fontSettings.fontEntries
            RootData.settings.fontSettings.fontFamily = entries[0].family
            RootData.pageSelectionModel.currentPageAddress = "View"
        }

        function gotoGallery() {
            RootData.pageSelectionModel.gotoPageByName(null, "Style Gallery")
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "styleGalleryPage",
                      5000, "should land on styleGalleryPage")
            return RootData.pageView.currentPageItem
        }

        // The test window is a plain view, where an existing menu or dialog keeps
        // the palette it was created with; taking its parent again makes it read
        // the window's palette. The application's window type updates popups live.
        function adoptWindowPalette(page, popupName) {
            const popup = findChild(page, popupName)
            verify(popup !== null, popupName + " should exist")
            const popupParent = popup.parent
            popup.parent = null
            popup.parent = popupParent
        }

        function setMode(mode) {
            RootData.settings.appearanceSettings.colorScheme = mode.scheme
            const page = gotoGallery()
            tryVerify(() => Theme.dark === mode.dark, 5000, "Theme.dark should follow " + mode.name)
            tryVerify(() => Qt.colorEqual(page.palette.windowText, Theme.text)
                            && Qt.colorEqual(page.palette.base, Theme.fieldSurface),
                      5000, "the Theme palette should reach the page in " + mode.name)
            for (const popupName of ["galleryMenu", "galleryDialog"]) {
                adoptWindowPalette(page, popupName)
            }
            return page
        }

        function test_loadsInLightAndDark() {
            // Painter items such as TaskProgressRing's ring report a missing QRhi
            // on the offscreen platform; every other warning fails the test.
            failOnWarning(/^(?!No QRhi found for window).*/)

            for (const mode of modes()) {
                const page = setMode(mode)
                for (const cardName of cardNames()) {
                    verify(findChild(page, cardName) !== null, cardName + " should exist in " + mode.name)
                }
            }
        }

        function findButton(item, text) {
            for (let i = 0; i < item.children.length; ++i) {
                const child = item.children[i]
                if (child.highlighted !== undefined && child.text === text) {
                    return child
                }
                const found = findButton(child, text)
                if (found !== null) {
                    return found
                }
            }
            return null
        }

        function test_styleIsCaveWhere() {
            const page = gotoGallery()
            const card = findChild(page, "cardButtons")
            verify(card !== null, "cardButtons should exist")

            const button = findButton(card, "Export")
            verify(button !== null, "cardButtons should hold the plain Export button")
            verify(button.background !== null, "the button should have a background")
            compare(button.background.radius, Theme.controlRadius,
                    "the button background should come from CaveWhereStyle")
        }

        function test_popupsOpen() {
            const page = gotoGallery()

            const menu = findChild(page, "galleryMenu")
            verify(menu !== null, "galleryMenu should exist")
            page.openMenu()
            tryVerify(() => menu.visible, 5000, "the menu should open")
            page.closePopups()
            tryVerify(() => !menu.visible, 5000, "the menu should close")

            const dialog = findChild(page, "galleryDialog")
            verify(dialog !== null, "galleryDialog should exist")
            page.openDialog()
            tryVerify(() => dialog.visible, 5000, "the dialog should open")
            page.closePopups()
            tryVerify(() => !dialog.visible, 5000, "the dialog should close")
        }

        function test_screenshots() {
            if (Qt.platform.pluginName === "offscreen") {
                skip("needs a GPU-backed platform")
            }

            for (const mode of modes()) {
                const page = setMode(mode)
                const scrollView = findChild(page, "styleGalleryScrollView")
                verify(scrollView !== null, "styleGalleryScrollView should exist")
                const flickable = scrollView.contentItem

                let step = 0
                for (let y = 0; y < Math.max(1, flickable.contentHeight); y += flickable.height) {
                    flickable.contentY = Math.min(y, Math.max(0, flickable.contentHeight - flickable.height))
                    tryCompare(flickable, "moving", false)
                    const path = WindowGrabber.grabToFile(page, "style-gallery-" + mode.name + "-" + step)
                    verify(path.length > 0, "grabToFile wrote step " + step + " in " + mode.name)
                    step++
                }
                flickable.contentY = 0

                const menu = findChild(page, "galleryMenu")
                page.openMenu()
                tryVerify(() => menu.opened, 5000, "the menu should finish opening")
                verify(WindowGrabber.grabToFile(page, "style-gallery-" + mode.name + "-menu").length > 0)
                page.closePopups()
                tryVerify(() => !menu.visible)

                const dialog = findChild(page, "galleryDialog")
                page.openDialog()
                tryVerify(() => dialog.opened, 5000, "the dialog should finish opening")
                verify(WindowGrabber.grabToFile(page, "style-gallery-" + mode.name + "-dialog").length > 0)
                page.closePopups()
                tryVerify(() => !dialog.visible)

                screenshotTextEditingMenu(page, mode)
                screenshotMainWindow(mode)
                screenshotLinkBarMenu(mode)
                screenshotKeywordTab(mode)
                screenshotTripPage(mode)
            }
        }

        // The app's frame around a cave page: the sidebar with Data current and
        // the pointer resting on View, and the link bar's chips.
        function screenshotMainWindow(mode) {
            if (RootData.region.caveCount === 0) {
                TestHelper.loadProjectFromFile(RootData.project,
                    TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"))
                tryVerify(() => RootData.region.caveCount > 0, 10000, "the demo cave should load")
            }
            const address = "Source/Data/Cave=" + RootData.region.cave(0).name
            RootData.pageSelectionModel.currentPageAddress = address
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName !== "styleGalleryPage",
                      5000, "should land on the cave page")

            const dataButton = findChild(rootId.mainWindow, "dataButton")
            verify(dataButton !== null && dataButton.troggled, "Data should be the current sidebar page")
            const viewButton = findChild(rootId.mainWindow, "viewButton")
            verify(viewButton !== null, "viewButton should exist")
            mouseMove(viewButton, viewButton.width / 2, viewButton.height / 2)
            const viewFace = findChild(viewButton, "buttonFace")
            verify(viewFace !== null, "the View button should have a face")
            tryVerify(() => Qt.colorEqual(viewFace.color, Theme.hoverOverlay),
                      5000, "the View button should show its hover face")

            verify(WindowGrabber.grabToFile(rootId.mainWindow, "style-gallery-" + mode.name + "-main-window").length > 0)
            mouseMove(rootId.mainWindow, rootId.mainWindow.width - 1, rootId.mainWindow.height - 1)
        }

        // The breadcrumb's right-click menu over the cave page. It is made on
        // first use and keeps its first palette, so it takes its parent again
        // for each mode.
        function screenshotLinkBarMenu(mode) {
            const crumb = findChild(rootId.mainWindow, "linkBarBreadcrumb")
            verify(crumb !== null, "linkBarBreadcrumb should exist")
            const menu = crumb.QC.ContextMenu.menu
            verify(menu !== null, "the breadcrumb should carry a context menu")
            const menuParent = menu.parent
            menu.parent = null
            menu.parent = menuParent
            menu.popup(crumb, crumb.width / 3, crumb.height / 2)
            tryVerify(() => menu.opened, 5000, "the link bar menu should finish opening")
            verify(WindowGrabber.grabToFile(rootId.mainWindow, "style-gallery-" + mode.name + "-link-bar-menu").length > 0)
            menu.close()
            tryVerify(() => !menu.visible)
        }

        // The view page with the Layers tab open, where the keyword filter's
        // "Also Include" button sits at the bottom of the side panel.
        function screenshotKeywordTab(mode) {
            RootData.pageSelectionModel.gotoPageByName(null, "View")
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "viewPage",
                      5000, "should land on the view page")
            const layersTab = findChild(rootId.mainWindow, "layersTabButton")
            verify(layersTab !== null, "layersTabButton should exist")
            mouseClick(layersTab)
            let alsoButton = null
            tryVerify(() => {
                alsoButton = findChild(rootId.mainWindow, "alsoButton")
                return alsoButton !== null && alsoButton.visible && alsoButton.width > 0
            }, 5000, "the Also Include button should show")
            verify(WindowGrabber.grabToFile(rootId.mainWindow, "style-gallery-" + mode.name + "-keyword-tab").length > 0)
        }

        // A trip page's survey editor in three states: at rest, with only the
        // shots banded; one shot holding the keyboard, tinted with its arrow;
        // and a station with an open splay cluster holding it, tinted along
        // with its splays.
        function screenshotTripPage(mode) {
            const cave = RootData.region.cave(0)
            verify(cave.rowCount() > 0, "the demo cave should have a trip")
            const trip = cave.trip(0)
            verify(trip.chunkCount > 0, "the demo trip should have survey data")
            const chunk = trip.chunk(0)
            const shotIndex = 2
            const splayStation = shotIndex + 2
            verify(chunk.stationCount > splayStation, "the demo chunk should have enough stations")

            RootData.pageSelectionModel.currentPageAddress =
                    "Source/Data/Cave=" + cave.name + "/Trip=" + trip.name
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "tripPage",
                      5000, "should land on the trip page")

            let view = null
            tryVerify(() => {
                view = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->surveyEditor->view")
                return view !== null && view.model !== null
            }, 5000, "the survey editor should be reachable")
            const model = view.model

            // The demo data has no splays, so one station takes the a4 set once
            const stationRowIndex = model.rowIndex(chunk, splayStation, SurveyEditorRowIndex.StationRow)
            let stationItem = null
            tryVerify(() => {
                view.positionViewAtIndex(model.toModelRow(stationRowIndex), ListView.Center)
                stationItem = view.itemAtIndex(model.toModelRow(stationRowIndex))
                return stationItem !== null
            }, 5000, "the splay station should have a row")
            if (stationItem.stationSplayCount === 0) {
                for (const splay of TestHelper.a4SplayReadings()) {
                    TestHelper.addStationSplay(chunk, splayStation, splay.distance, splay.compass, splay.clino)
                }
            }
            tryVerify(() => view.itemAtIndex(model.toModelRow(stationRowIndex)).stationSplayCount > 0,
                      5000, "the splay station should carry splays")
            if (!view.itemAtIndex(model.toModelRow(stationRowIndex)).stationSplaysExpanded) {
                model.toggleSplaysExpanded(stationRowIndex)
            }
            tryVerify(() => view.itemAtIndex(model.toModelRow(stationRowIndex)).stationSplaysExpanded,
                      5000, "the splay cluster should be open")

            const shotRow = () => model.toModelRow(model.rowIndex(chunk, shotIndex, SurveyEditorRowIndex.ShotRow))
            verify(shotRow() >= 0, "the shot should have a model row")

            // The page above the table is the list's header, so centering the
            // shot scrolls the table up into the window.
            const centerOnShot = () => {
                view.positionViewAtIndex(shotRow(), ListView.Center)
                tryCompare(view, "moving", false)
            }

            // The editor keeps its focus from the previous mode, so the keyboard
            // moves to another chunk to leave this one at rest
            if (trip.chunkCount < 2) {
                trip.addNewChunk()
            }
            const restRow = model.toModelRow(model.rowIndex(trip.chunk(trip.chunkCount - 1), 0,
                                                            SurveyEditorRowIndex.StationRow))
            model.setFocusedCell(model.cellIndex(restRow, SurveyEditorCellIndex.StationNameCell))
            tryVerify(() => model.focusedRowIndex.chunk !== chunk, 5000,
                      "the keyboard should sit outside the chunk, leaving it at rest")
            centerOnShot()
            verify(WindowGrabber.grabToFile(rootId.mainWindow, "style-gallery-" + mode.name + "-trip-page-rest").length > 0)

            model.setFocusedCell(model.cellIndex(shotRow(), SurveyEditorCellIndex.ShotCompassCell))
            centerOnShot()
            let cell = null
            tryVerify(() => {
                cell = findChild(view, "dataBox." + model.focusedRow + "." + SurveyEditorCellIndex.ShotCompassCell)
                return cell !== null && cell.highlightVisible
            }, 5000, "the shot's compass cell should hold the keyboard")
            tryVerify(() => findChild(view.itemAtIndex(shotRow()), "shotArrow") !== null,
                      5000, "the selected shot should show its arrow")
            verify(WindowGrabber.grabToFile(rootId.mainWindow, "style-gallery-" + mode.name + "-trip-page").length > 0)

            const stationRow = model.toModelRow(stationRowIndex)
            model.setFocusedCell(model.cellIndex(stationRow, SurveyEditorCellIndex.StationLeftCell))
            centerOnShot()
            tryVerify(() => {
                cell = findChild(view, "dataBox." + model.focusedRow + "." + SurveyEditorCellIndex.StationLeftCell)
                return cell !== null && cell.highlightVisible
            }, 5000, "the station's left cell should hold the keyboard")
            verify(WindowGrabber.grabToFile(rootId.mainWindow, "style-gallery-" + mode.name + "-trip-page-station").length > 0)
        }

        // The right-click menu of a text field, drawn into the window so the
        // grab holds it. It is made on first use and keeps its first palette,
        // so it takes its parent again for each mode.
        function screenshotTextEditingMenu(page, mode) {
            const field = findChild(page, "galleryTextField")
            verify(field !== null, "galleryTextField should exist")
            const flickable = findChild(page, "styleGalleryScrollView").contentItem
            const fieldTop = field.mapToItem(flickable.contentItem, 0, 0).y
            flickable.contentY = Math.max(0, Math.min(fieldTop - flickable.height / 4,
                                                      flickable.contentHeight - flickable.height))
            tryCompare(flickable, "moving", false)

            const menu = field.QC.ContextMenu.menu
            verify(menu !== null, "the text field should carry a context menu")
            menu.popupType = QC.Popup.Item
            const menuParent = menu.parent
            menu.parent = null
            menu.parent = menuParent
            field.forceActiveFocus()
            field.select(0, 2)
            menu.popup(field, field.width / 2, field.height / 2)
            tryVerify(() => menu.opened, 5000, "the text editing menu should finish opening")
            verify(WindowGrabber.grabToFile(page, "style-gallery-" + mode.name + "-text-menu").length > 0)
            menu.close()
            tryVerify(() => !menu.visible)
            flickable.contentY = 0
        }
    }
}
