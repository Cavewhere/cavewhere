import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib

// The page of a source root, the node an attached survey file builds its
// subtree under (SURVEY_TREE_PLAN C7.4b): the source card, and the paperclip
// and file declination the derived trip rows below it carry, which each
// trip's own page reports in the same words.
MainWindowTest {
    id: rootId

    ExternalCenterlineTestCase {
        name: "CavePageExternalCave"
        when: windowShown

        function init() {
            RootData.project.newProject()
            RootData.pageSelectionModel.currentPageAddress = "View"
        }

        function cleanup() {
            RootData.project.newProject()
            rootId.width = 1200
            rootId.height = 700
        }

        // Every item under `item` answering to `objectName`. The trip rows
        // are delegates, so there is one of each name per row.
        function collectByName(item, objectName, found) {
            if (item.objectName === objectName) {
                found.push(item)
            }
            const kids = item.children
            if (kids !== undefined) {
                for (let i = 0; i < kids.length; i++) {
                    collectByName(kids[i], objectName, found)
                }
            }
            return found
        }

        function gotoCavePage(cave) {
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Node=" + cave.name
            tryVerify(() => RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.objectName === "cavePage",
                      10000, "the cave page opens")
            waitForRendering(rootId)
            return RootData.pageView.currentPageItem
        }

        // survex_blocks.svx as a source root named AttachedCave: the block
        // doghill and, below it, big-passage with east and the empty sump.
        // doghill and big-passage each hold a trip for their own shots; east,
        // a leaf block, is one trip.
        function attachedCave(projectBaseName) {
            return makeSavedCaveAttach(
                        projectBaseName,
                        "external-centerlines/survex_blocks.svx")
        }

        // The page's tree with every row open, so the derived trips under the
        // root's nodes have rows: `rows` of them once every one is drawn.
        function expandedTree(cavePage, rows) {
            let firstRow = null
            tryVerify(() => {
                          firstRow = findChild(cavePage, "caveDelegate0")
                          return firstRow !== null
                      }, 5000, "the root's page draws its first node row")
            const view = firstRow.treeView
            //Each opened row brings its children in below it, so the walk
            //reaches every depth. Asked again on each try: the attach's own
            //landing can reset the rows under the first expansion.
            tryVerify(() => {
                          for (let row = 0; row < view.rows; row++) {
                              view.expand(row)
                          }
                          return view.rows === rows
                      }, 5000, "every node of the subtree is open; rows: " + view.rows)
            waitForRendering(rootId)
            return view
        }

        // The Decl cell of every drawn trip row: the angle, then its caption.
        // Waits for `count` of them.
        function declinationTexts(cavePage, count) {
            const modes = cellsNamed(cavePage, "declinationMode", count)
            const texts = []
            for (let i = 0; i < modes.length; i++) {
                const row = modes[i].parent
                const value = collectByName(row, "value", [])[0]
                const emDash = collectByName(row, "declinationEmDash", [])[0]
                texts.push(emDash.visible
                           ? emDash.text
                           : value.text + " " + (modes[i].visible ? modes[i].text : ""))
            }
            return texts
        }

        // The cells named `objectName` once `count` of them are drawn.
        function cellsNamed(cavePage, objectName, count) {
            let cells = []
            tryVerify(() => {
                          cells = collectByName(cavePage, objectName, [])
                          return cells.length === count
                      }, 5000, count + " " + objectName + " cells should be drawn")
            return cells
        }

        // A Decl readout's text: the angle, then its caption, or the em dash.
        function declinationText(readout) {
            const emDash = findChild(readout, "declinationEmDash")
            return emDash.visible
                    ? emDash.text
                    : findChild(readout, "value").text + " "
                      + findChild(readout, "declinationMode").text
        }

        // What each drawn trip row's Decl cell shows, keyed by the trip's
        // name. Waits for `tripCount`.
        function declinationByTrip(cavePage, view, tripCount) {
            let cells = {}
            tryVerify(() => {
                          cells = {}
                          let found = 0
                          for (let row = 0; row < view.rows; row++) {
                              const nameCell = findChild(cavePage, "tripDelegate" + row)
                              const declCell = findChild(cavePage, "tripDecl" + row)
                              if (nameCell === null || declCell === null) {
                                  continue
                              }
                              const name = findChild(nameCell, "tripLink").text.replace("📎 ", "")
                              cells[name] = declinationText(declCell)
                              found++
                          }
                          return found === tripCount
                      }, 5000, tripCount + " trip rows should draw their Decl cell")
            return cells
        }

        // The Decl readout on the trip page of the root's trip `tripName`.
        function tripPageDeclination(cave, tripName) {
            RootData.pageSelectionModel.currentPageAddress =
                    "Source/Data/Node=" + cave.name + "/Trip=" + tripName
            let page = null
            tryVerify(() => {
                          page = RootData.pageView.currentPageItem
                          return page !== null && page.objectName === "tripPage"
                          && page.currentTrip !== null && page.currentTrip.name === tripName
                      }, 10000, "the trip page of " + tripName + " opens")
            let readout = null
            tryVerify(() => {
                          readout = findChild(page, "tripMetadataDeclinationReadout")
                          return readout !== null && readout.visible
                      }, 5000, "the trip page of " + tripName + " reports its declination")
            compare(findChild(page, "tripMetadataDeclination").visible, false,
                    "cavern owns " + tripName + "'s declination, so nothing edits it")
            return declinationText(readout)
        }

        // A derived trip's page reports the declination its tree row shows,
        // in the same words: a file's value, the 0 of a block that sets none,
        // and the em dash of a block with no one value.
        function test_tripPageDeclinationMatchesTheTreeCell() {
            const cave = makeSavedCaveAttach(
                           "cavepage-trip-declination",
                           "external-centerlines/survex_declination/declination.svx")
            const cavePage = gotoCavePage(cave)
            //parent, its trip child, and the root's five trips
            const view = expandedTree(cavePage, 7)
            const treeTexts = declinationByTrip(cavePage, view, 6)
            compare(treeTexts["fileset"], "-2.5° file", "*calibrate declination 2.5 applies -2.5")
            compare(treeTexts["silent"], "0° none", "a block that sets nothing applies 0")
            compare(treeTexts["mixed"], "—", "a block that changes declination has no one value")

            const trips = ["fileset", "silent", "mixed"]
            for (let i = 0; i < trips.length; i++) {
                compare(tripPageDeclination(cave, trips[i]), treeTexts[trips[i]],
                        trips[i] + "'s page and tree row read the same declination")
            }
        }

        function test_cardVisibleOnlyForAttachedCave() {
            const cave = attachedCave("cavepage-external-card")
            let cavePage = gotoCavePage(cave)

            const card = findChild(cavePage, "sourceRootCard")
            verify(card !== null, "a source root shows its source card")
            tryVerify(() => card.visible, 5000, "the card renders on the page")

            const header = findChild(card, "attachedHeader")
            verify(header !== null, "the card carries the attached header")
            compare(header.fileName, "survex_blocks.svx",
                    "the header names the root's entry file")

            // A cave that holds its own trips has nothing attached, so the
            // page keeps its native shape.
            RootData.region.addCave()
            const nativeCave = RootData.region.cave(RootData.region.rowCount() - 1)
            nativeCave.name = "NativeCave"
            nativeCave.addTrip()

            cavePage = gotoCavePage(nativeCave)
            const nativeCard = findChild(cavePage, "sourceRootCard")
            verify(nativeCard === null || !nativeCard.visible,
                   "a native cave shows no source card")
        }

        function test_derivedTripRowsShowPaperclipAndFileDeclination() {
            const cave = attachedCave("cavepage-external-rows")
            compare(cave.tripCount, 0, "the root's shots all sit inside doghill")
            compare(cave.childNodeCount, 1, "doghill is the root's one block")

            const cavePage = gotoCavePage(cave)
            //doghill, big-passage, sump, and the three trips
            expandedTree(cavePage, 6)

            const nameLinks = cellsNamed(cavePage, "tripLink", 3)
            for (let i = 0; i < nameLinks.length; i++) {
                verify(nameLinks[i].text.indexOf("📎 ") === 0,
                       "a derived trip row is marked as externally backed: "
                       + nameLinks[i].text)
            }

            //survex_blocks.svx sets no declination, so cavern applies 0.
            compare(declinationTexts(cavePage, 3), ["0° none", "0° none", "0° none"],
                    "each derived row shows the 0 cavern applies")
        }

        // A file whose shots sit outside every *begin gives the root a unit of
        // its own, which windows the whole file beside the side block's trip
        // (§7.2 step 6), and that row is externally backed like any other: same
        // paperclip, same file declination.
        function test_wholeNodeWindowRowIsMarkedLikeAnyDerivedRow() {
            const cave = makeSavedCaveAttach(
                           "cavepage-whole-cave-row",
                           "external-centerlines/survex_root_and_block.svx")
            compare(cave.tripCount, 2, "the root holds its own unit and the leaf block side")
            compare(cave.childNodeCount, 0, "a leaf block is a trip, not a node")
            compare([cave.trip(0).windowsWholeNode, cave.trip(1).windowsWholeNode].filter(w => w).length,
                    1, "only the root's own unit windows the whole file")

            const cavePage = gotoCavePage(cave)

            const nameLinks = cellsNamed(cavePage, "tripLink", 2)
            for (let i = 0; i < nameLinks.length; i++) {
                verify(nameLinks[i].text.indexOf("📎 ") === 0,
                       "a window row is marked as externally backed: "
                       + nameLinks[i].text)
            }

            //survex_root_and_block.svx sets no declination either.
            compare(declinationTexts(cavePage, 2), ["0° none", "0° none"],
                    "each window row shows the 0 cavern applies")
        }

        // externallyBacked covers a trip-level attachment too, so its row on a
        // native cave's page is marked the same way. survex_simple.svx sets no
        // declination, so the driver injects the trip's own calibration and the
        // row reads like a native trip's.
        function test_phase2AttachedTripRowShowsPaperclipToo() {
            const fixture = attachFixtureTrip("cavepage-external-p2")
            verify(fixture.trip.externallyBacked, "the attached trip is externally backed")

            const cave = RootData.region.cave(0)
            const cavePage = gotoCavePage(cave)

            const nameLinks = collectByName(cavePage, "tripLink", [])
            compare(nameLinks.length, 1, "the cave holds the one attached trip")
            verify(nameLinks[0].text.indexOf("📎 ") === 0,
                   "an Attached trip row is marked too: " + nameLinks[0].text)

            const calibration = fixture.trip.calibration
            const caption = calibration.autoDeclination && calibration.autoDeclinationAvailable
                    ? "auto" : "manual"
            compare(declinationTexts(cavePage, 1),
                    [Number(calibration.declination.toFixed(2)).toString() + "° " + caption],
                    "a silent file's trip row shows the calibration the driver injects")
        }

        // Each derived trip's row shows the date its block wrote, not the day
        // of the import: big-passage and the east block that inherits its
        // date read the fixture's date, doghill writes none and reads today.
        function test_tripRowsShowTheirBlockDates() {
            const dayOfAttach = Qt.formatDate(new Date(), "yyyy-MM-dd")
            const cave = attachedCave("cavepage-external-dates")
            const cavePage = gotoCavePage(cave)
            const view = expandedTree(cavePage, 6)

            //A trip row's Date cell names itself by its view row, and draws
            //the date as its "value"; node rows sit between the trip rows.
            const dateCells = []
            tryVerify(() => {
                          dateCells.length = 0
                          for (let row = 0; row < view.rows; row++) {
                              const cell = findChild(cavePage, "tripDate" + row)
                              const value = cell === null ? null : findChild(cell, "value")
                              if (value !== null) {
                                  dateCells.push(value)
                              }
                          }
                          return dateCells.length === 3
                      }, 5000, "every derived trip row must draw its date")

            let seeded = 0
            let today = 0
            // A run that crosses midnight between the attach and this check
            // sees either side of the boundary, so both days count as today.
            const todayText = Qt.formatDate(new Date(), "yyyy-MM-dd")
            for (let i = 0; i < dateCells.length; i++) {
                if (dateCells[i].text === "2024-01-05") {
                    seeded++
                } else if (dateCells[i].text === todayText
                           || dateCells[i].text === dayOfAttach) {
                    today++
                }
            }
            compare(seeded, 2, "the dated block and the block inheriting it")
            compare(today, 1, "the block that writes no date keeps today")
        }

        // The root's Length and Depth come from the solved file, folded over
        // its subtree. A node that resolved nothing measures 0, never a
        // sentinel.
        function test_statsShowSolvedNumbers() {
            const cave = attachedCave("cavepage-external-stats")
            const cavePage = gotoCavePage(cave)

            tryVerify(() => cave.length.value > 0, 20000,
                      "the attached file gives the root a length")

            const valueCells = collectByName(cavePage, "value", [])
            verify(valueCells.length >= 2, "the page shows the Length and Depth stats")
            for (let i = 0; i < valueCells.length; i++) {
                verify(!valueCells[i].text.startsWith("-1"),
                       "a stat cell reads a measurement: " + valueCells[i].text)
            }
        }

        function test_replaceReachableFromCard() {
            const cave = attachedCave("cavepage-external-replace")
            const cavePage = gotoCavePage(cave)

            const card = findChild(cavePage, "sourceRootCard")
            verify(card !== null, "the card must exist")
            tryVerify(() => card.visible, 5000)

            verify(findChild(card, "replaceCenterlineDialog") === null,
                   "the card defers the dialog until Replace… is chosen")

            compare(findChild(card, "replaceButton"), null, "Replace… is no button on the card")
            const menuArea = openAttachedFileMenu(findChild(card, "attachedHeader"))
            const replaceItem = findChild(menuArea, "replaceAction")
            verify(replaceItem !== null, "the card's header menu holds replaceAction")
            verify(replaceItem.enabled, "an idle root offers Replace…")
            closeMenu(menuArea)
            replaceItem.triggered()

            const dialog = findChild(card, "replaceCenterlineDialog")
            verify(dialog !== null, "Replace… opens the replace dialog")
            const dialogPopup = findChild(dialog, "replaceDialog")
            verify(dialogPopup !== null, "replaceDialog must exist")
            tryVerify(() => dialogPopup.visible, 5000, "the dialog opens")

            const currentEntry = findChild(dialog, "currentEntryLabel")
            verify(currentEntry !== null, "currentEntryLabel must exist")
            verify(currentEntry.text.indexOf("survex_blocks.svx") >= 0,
                   "the dialog names the root's current file: " + currentEntry.text)

            const cancelButton = findChild(dialog, "replaceCancelButton")
            verify(cancelButton !== null, "replaceCancelButton must exist")
            mouseClick(cancelButton)
            tryVerify(() => findChild(card, "replaceCenterlineDialog") === null,
                      5000, "closing the dialog frees it")
        }
    }
}
