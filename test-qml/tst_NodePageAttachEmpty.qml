import QtQuick as QQ
import QtQuick.Controls as QC
import QtTest
import cavewherelib
import cw.TestLib

// Issue #729: the node page hosts the Add survey file dialog, so it opens
// on an empty cave, whose page hides the survey tree, and every way out of it
// or of the Add trip from survey file dialog leaves the window usable.
MainWindowTest {
    id: rootId

    ExternalCenterlineTestCase {
        name: "NodePageAttachEmpty"
        when: windowShown

        function init() {
            RootData.project.newProject()
            RootData.pageSelectionModel.currentPageAddress = "View"
        }

        function cleanup() {
            // A modal dialog left open by a failed assertion would eat the
            // next test's clicks.
            const attachDialog = findChild(mainWindow, "attachSurveyFileDialog")
            if (attachDialog !== null) {
                attachDialog.close()
            }
            const addDialog = findChild(mainWindow, "attachExternalCenterlineDialog")
            if (addDialog !== null) {
                addDialog.close()
            }
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.project.newProject()
        }

        function addNode(parent, kind, name) {
            const node = RootData.region.addNode(parent, kind)
            node.name = name
            return node
        }

        function gotoNodePage(address, node) {
            RootData.pageSelectionModel.currentPageAddress = address
            tryVerify(() => {
                          const page = RootData.pageView.currentPageItem
                          return page !== null && page.objectName === "cavePage"
                              && page.currentNode === node
                      }, 5000, "the page for " + address + " should be showing")
            const page = RootData.pageView.currentPageItem
            waitForRendering(page)
            return page
        }

        function openAddCaret(page) {
            const addBar = findChild(page, "addTrip")
            verify(addBar !== null, "the Add bar must exist")
            const menuButton = findChild(addBar, "menuButton")
            verify(menuButton !== null, "the split button must show its caret")
            mouseClick(menuButton)

            let menu = null
            tryVerify(() => {
                          menu = findChild(page, "addTripMenu")
                          return menu !== null && menu.opened
                      }, 5000, "the caret must open the Add menu")
            return menu
        }

        // What still floats above the window: a modal dialog's dim
        // background, or a popup.
        function visibleOverlayChildren() {
            const overlay = rootId.QC.Overlay.overlay
            const shown = []
            for (let i = 0; i < overlay.children.length; i++) {
                const child = overlay.children[i]
                if (child.visible) {
                    shown.push(String(child) + " " + child.width + "x" + child.height)
                }
            }
            return shown
        }

        function verifyOverlayDrains(message) {
            tryVerify(() => visibleOverlayChildren().length === 0, 5000,
                      message + ", still showing: " + JSON.stringify(visibleOverlayChildren()))
        }

        function emptyCavePage() {
            const cave = addNode(null, SurveyNodeKind.Cave, "Empty Cave")
            const page = gotoNodePage("Source/Data/Node=Empty Cave", cave)
            verify(page.isEmpty, "a new cave's page starts empty")
            return { cave: cave, page: page }
        }

        function attachDialog(page) {
            const dialog = findChild(page, "attachSurveyFileDialog")
            verify(dialog !== null, "the page hosts the Add survey file dialog")
            return dialog
        }

        // Add caret → Add Survey File…
        function triggerAttachFromCaret(page) {
            const menu = openAddCaret(page)
            const item = findChild(menu, "attachSurveyFileMenuItem")
            verify(item !== null && item.visible, "the Add menu offers Add Survey File…")
            mouseClick(item)
        }

        function cancel(dialog) {
            const button = findChild(dialog, "cancelButton")
            tryVerify(() => button !== null && button.visible && button.enabled, 5000,
                      "the dialog's Cancel button is ready")
            waitForRendering(rootId)
            mouseClick(button)
        }

        // Add caret → Add Survey File…, waiting for the dialog to show.
        function openAttachFromCaret(page) {
            triggerAttachFromCaret(page)
            const dialog = attachDialog(page)
            const pathField = findChild(dialog, "sourcePathField")
            tryVerify(() => pathField.visible, 5000, "the Attach dialog opens")
            return { dialog: dialog, pathField: pathField }
        }

        function cancelAndWaitClosed(dialog, pathField) {
            cancel(dialog)
            tryVerify(() => !pathField.visible, 5000, "Cancel closes the dialog")
        }

        function test_attachOpensOnAnEmptyCave() {
            const empty = emptyCavePage()

            triggerAttachFromCaret(empty.page)

            const dialog = attachDialog(empty.page)
            compare(dialog.target, empty.cave, "the file goes under the page's cave")
            tryVerify(() => findChild(dialog, "sourcePathField").visible, 5000,
                      "Add Survey File… opens its dialog on an empty cave")
        }

        function test_cancelOnAnEmptyCaveLeavesNoModalBackground() {
            const empty = emptyCavePage()
            verifyOverlayDrains("the window starts with nothing above it")

            const attach = openAttachFromCaret(empty.page)

            cancelAndWaitClosed(attach.dialog, attach.pathField)
            verifyOverlayDrains("Cancel takes the modal background with it")
        }

        // The Add dialog adds its trip before it opens, which brings the
        // tree back, and Cancel removes that trip, which hides the tree again.
        function test_addTripFromFileShowsOnlyTheAddDialog() {
            const empty = emptyCavePage()

            const attach = openAttachFromCaret(empty.page)
            cancelAndWaitClosed(attach.dialog, attach.pathField)

            empty.page.addTripFromSurveyFileWithDialog()
            const addDialog = findChild(empty.page, "attachExternalCenterlineDialog")
            verify(addDialog !== null, "the page hosts the Add trip from survey file dialog")
            tryVerify(() => findChild(addDialog, "sourcePathField").visible, 5000,
                      "the Add dialog opens")
            waitForRendering(rootId)
            verify(!attach.pathField.visible,
                   "the canceled Attach dialog stays closed beside the Add dialog")
            cancel(addDialog)
        }

        function test_attachThenAddTripFromFileCancelLeavesNoModalBackground() {
            const empty = emptyCavePage()
            verifyOverlayDrains("the window starts with nothing above it")

            const attach = openAttachFromCaret(empty.page)
            cancelAndWaitClosed(attach.dialog, attach.pathField)

            empty.page.addTripFromSurveyFileWithDialog()
            const addDialog = findChild(empty.page, "attachExternalCenterlineDialog")
            verify(addDialog !== null, "the page hosts the Add trip from survey file dialog")
            const addField = findChild(addDialog, "sourcePathField")
            tryVerify(() => addField.visible, 5000, "the Add dialog opens")

            cancel(addDialog)
            tryVerify(() => !addField.visible, 5000, "Cancel closes the Add dialog")
            tryCompare(empty.cave, "tripCount", 0, 5000, "Cancel removes the trip the Add dialog made")
            verifyOverlayDrains("both dialogs leave the window usable")
        }

        // A cave row's Add submenu reaches the page's dialog through the
        // tree's request.
        function test_rowAddSubmenuOpensThePageDialog() {
            const folder = addNode(null, SurveyNodeKind.Folder, "Range")
            const cave = addNode(folder, SurveyNodeKind.Cave, "Row Cave")
            const page = gotoNodePage("Source/Data/Node=Range", folder)

            let row = null
            tryVerify(() => {
                          row = findChild(page, "caveDelegate0")
                          return row !== null && row.object === cave
                      }, 5000, "the cave's row shows")
            row.showContextMenu(Theme.delegatePadding, row.height)

            let menu = null
            tryVerify(() => {
                          menu = findChild(row, "surveyItemContextMenu")
                          return menu !== null && menu.visible
                      }, 5000, "the cave row opens its context menu")
            const submenu = menu.menuAt(0)
            compare(submenu.objectName, "addVerbsSubmenu")
            mouseClick(menu.itemAt(0))
            tryVerify(() => submenu.opened, 5000, "Add opens its verbs")
            mouseClick(findChild(submenu, "attachSurveyFileMenuItem"))

            const dialog = attachDialog(page)
            const pathField = findChild(dialog, "sourcePathField")
            tryVerify(() => pathField.visible, 5000, "the row's Attach opens the page's dialog")
            compare(dialog.target, cave, "the file goes under the row's cave")

            cancelAndWaitClosed(dialog, pathField)
            verifyOverlayDrains("Cancel takes the modal background with it")
        }
    }
}
