import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib

// CavePage for a cave whose centerline comes from an attached survey
// file (plans/EXTERNAL_FILE_PHASE3.html P3.8): the cave-level summary
// card, the paperclip and em-dash the externally-backed trip rows
// carry, and the Detach flow.
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
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=" + cave.name
            tryVerify(() => RootData.pageView.currentPageItem !== null
                      && RootData.pageView.currentPageItem.objectName === "cavePage",
                      10000, "the cave page opens")
            waitForRendering(rootId)
            return RootData.pageView.currentPageItem
        }

        function attachedCave(projectBaseName) {
            return makeSavedCaveAttach(
                        projectBaseName,
                        "external-centerlines/survex_blocks.svx")
        }

        function test_cardVisibleOnlyForAttachedCave() {
            const cave = attachedCave("cavepage-external-card")
            let cavePage = gotoCavePage(cave)

            const card = findChild(cavePage, "externalCaveSummary")
            verify(card !== null, "an attached cave shows its summary card")
            tryVerify(() => card.visible, 5000, "the card renders on the page")

            const header = findChild(card, "attachedHeader")
            verify(header !== null, "the card carries the attached header")
            compare(header.fileName, "survex_blocks.svx",
                    "the header names the cave's entry file")

            // A cave that holds its own trips has nothing attached, so the
            // page keeps its native shape.
            RootData.region.addCave()
            const nativeCave = RootData.region.cave(RootData.region.rowCount() - 1)
            nativeCave.name = "NativeCave"
            nativeCave.addTrip()

            cavePage = gotoCavePage(nativeCave)
            const nativeCard = findChild(cavePage, "externalCaveSummary")
            verify(nativeCard === null || !nativeCard.visible,
                   "a native cave shows no attachment card")
        }

        function test_scopeRowsShowPaperclipAndEmDash() {
            const cave = attachedCave("cavepage-external-rows")
            compare(cave.rowCount(), 3, "the attach created one trip per block")

            const cavePage = gotoCavePage(cave)

            const nameLinks = collectByName(cavePage, "tripNameLink", [])
            compare(nameLinks.length, 3, "every trip has a name cell")
            for (let i = 0; i < nameLinks.length; i++) {
                verify(nameLinks[i].text.indexOf("📎 ") === 0,
                       "a Scope row is marked as externally backed: "
                       + nameLinks[i].text)
            }

            const emDashes = collectByName(cavePage, "declinationEmDash", [])
            compare(emDashes.length, 3, "every trip has a declination cell")
            for (let j = 0; j < emDashes.length; j++) {
                verify(emDashes[j].visible,
                       "the file owns the declination, so the row shows an em dash")
            }
        }

        // externallyBacked covers a Phase-2 trip-level attachment too, so
        // its row on the cave page is marked the same way.
        function test_phase2AttachedTripRowShowsPaperclipToo() {
            const fixture = attachFixtureTrip("cavepage-external-p2")
            verify(fixture.trip.externallyBacked, "the attached trip is externally backed")

            const cave = RootData.region.cave(0)
            const cavePage = gotoCavePage(cave)

            const nameLinks = collectByName(cavePage, "tripNameLink", [])
            compare(nameLinks.length, 1, "the cave holds the one attached trip")
            verify(nameLinks[0].text.indexOf("📎 ") === 0,
                   "an Attached trip row is marked too: " + nameLinks[0].text)

            const emDashes = collectByName(cavePage, "declinationEmDash", [])
            compare(emDashes.length, 1, "the row has a declination cell")
            verify(emDashes[0].visible, "the file owns its declination")
        }

        // Each Scope trip's row shows the date its block wrote, not the day
        // of the import: big-passage and the east block that inherits its
        // date read the fixture's date, doghill writes none and reads today.
        function test_tripRowsShowTheirBlockDates() {
            const dayOfAttach = Qt.formatDate(new Date(), "yyyy-MM-dd")
            const cave = attachedCave("cavepage-external-dates")
            const cavePage = gotoCavePage(cave)

            const dateCells = collectByName(cavePage, "tripDateLabel", [])
            compare(dateCells.length, 3, "every trip has a date cell")

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

        // The cave's Length and Depth come from the solved external centerline
        // (P3.13). A cave that resolved nothing measures 0, never a sentinel.
        function test_statsShowSolvedNumbers() {
            const cave = attachedCave("cavepage-external-stats")
            const cavePage = gotoCavePage(cave)

            tryVerify(() => cave.length.value > 0, 20000,
                      "the attached centerline gives the cave a length")

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

            const card = findChild(cavePage, "externalCaveSummary")
            verify(card !== null, "the card must exist")
            tryVerify(() => card.visible, 5000)

            verify(findChild(card, "replaceCenterlineDialog") === null,
                   "the card defers the dialog until Replace is clicked")

            const replaceButton = findChild(card, "replaceButton")
            verify(replaceButton !== null, "replaceButton must exist")
            mouseClick(replaceButton)

            const dialog = findChild(card, "replaceCenterlineDialog")
            verify(dialog !== null, "Replace… opens the replace dialog")
            const dialogPopup = findChild(dialog, "replaceDialog")
            verify(dialogPopup !== null, "replaceDialog must exist")
            tryVerify(() => dialogPopup.visible, 5000, "the dialog opens")

            const currentEntry = findChild(dialog, "currentEntryLabel")
            verify(currentEntry !== null, "currentEntryLabel must exist")
            verify(currentEntry.text.indexOf("survex_blocks.svx") >= 0,
                   "the dialog names the cave's current file: " + currentEntry.text)

            const cancelButton = findChild(dialog, "replaceCancelButton")
            verify(cancelButton !== null, "replaceCancelButton must exist")
            mouseClick(cancelButton)
            tryVerify(() => findChild(card, "replaceCenterlineDialog") === null,
                      5000, "closing the dialog frees it")
        }
    }
}
