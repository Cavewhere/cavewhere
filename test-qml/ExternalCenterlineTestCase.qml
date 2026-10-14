import QtQuick
import cavewherelib
import cw.TestLib

// Shared fixtures for the external-centerline test files
// (tst_CavernOutputPageExternal, tst_FileMenuExportDisable, ...).
CWTestCase {

    // Creates a fresh cave + trip and saves the project as .cwproj so
    // the attach orchestrator's temporary-project guard passes.
    // Returns the trip, still Native - the attach-dialog tests drive
    // the attach through the UI themselves.
    function makeSavedTrip(projectBaseName) {
        RootData.region.addCave()
        const cave = RootData.region.cave(0)
        cave.addTrip()
        const trip = cave.trip(0)

        saveProjectAs(projectBaseName)
        return trip
    }

    // Gives the project an author and a .cwproj on disk, which is what
    // the attach orchestrator's temporary-project guard requires.
    function saveProjectAs(projectBaseName) {
        RootData.account.name = "External Test"
        RootData.account.email = "external.test@example.com"

        const tmpPath = RootData.urlToLocal(TestHelper.tempDirectoryUrl())
        const projectPath = tmpPath + "/" + projectBaseName + ".cwproj"
        verify(RootData.project.saveAs(projectPath), "saveAs should succeed")
        TestHelper.waitForProjectSaveToFinish(RootData.project)
    }

    // Runs `attachVerb` and waits for the attachment it starts to land.
    // Counts rows from where the model already stood, so a project that
    // holds earlier attachments still waits for the new one.
    function attachAndWaitForRow(attachVerb) {
        const model = RootData.externalCenterlineManager.attachedCenterlinesModel
        const rowsBefore = model.rowCount()
        attachVerb()
        tryVerify(() => model.rowCount() > rowsBefore,
                  10000, "attach should land a row in the attached model")
        RootData.futureManagerModel.waitForFinished()
    }

    // Attaches source to trip through the cwRootData wrapper and waits
    // for this attachment to land.
    function attachSourceToTrip(trip, source) {
        attachAndWaitForRow(() => RootData.attachTripCenterline(trip, source))
    }

    // Attaches `source` as a source root under the region's root and waits
    // for the attachment to land. Returns the root the attach built.
    function attachSourceToRegion(source) {
        const regionRoot = RootData.region.rootNode
        const childCountBefore = regionRoot.childNodeCount
        attachAndWaitForRow(() => RootData.attachSource(regionRoot, source))
        verify(regionRoot.childNodeCount > childCountBefore, "the attach adds a source root")
        return regionRoot.childNode(regionRoot.childNodeCount - 1)
    }

    // Attaches `source` as a source root under the region's root, renames
    // the root to `caveName`, and returns it.
    function makeAttachedCave(caveName, source) {
        const sourceRoot = attachSourceToRegion(source)
        sourceRoot.name = caveName
        return sourceRoot
    }

    // Saves the fresh project as .cwproj and attaches `fixture` - a path
    // relative to testcases/datasets - to a new cave via the cwRootData
    // wrapper. Returns the attached source root. The fixture is copied to a
    // temp folder on its own, so pass a single self-contained file; a
    // fixture that *includes its siblings needs testcasesDatasetSourcePath
    // and attachSourceToRegion directly.
    function makeSavedCaveAttach(projectBaseName, fixture) {
        saveProjectAs(projectBaseName)
        return makeAttachedCave("AttachedCave",
                                TestHelper.testcasesDatasetPath(fixture))
    }

    // Saves the fresh project as .cwproj and attaches compass_project at the
    // top level: the root PROJECT holds Alpha (ALPHA1, ALPHA2), Bravo
    // (BRAVO1), and Charlie (CHARLIE1). Returns the root.
    function attachCompassProject(projectBaseName) {
        saveProjectAs(projectBaseName)
        return attachSourceToRegion(
                    TestHelper.testcasesDatasetSourcePath(
                        "external-centerlines/compass_project/PROJECT.MAK"))
    }

    function childNamed(node, name) {
        for (let i = 0; i < node.childNodeCount; i++) {
            if (node.childNode(i).name === name) {
                return node.childNode(i)
            }
        }
        return null
    }

    function tripNamed(node, name) {
        for (let i = 0; i < node.tripCount; i++) {
            if (node.trip(i).name === name) {
                return node.trip(i)
            }
        }
        return null
    }

    // A DeclinationReadout's text as it reads on screen: its caption and
    // angle left to right, or the em dash.
    function declinationText(readout) {
        const emDash = findChild(readout, "declinationEmDash")
        if (emDash.visible) {
            return emDash.text
        }
        return [findChild(readout, "declinationMode"), findChild(readout, "value")]
                .sort((a, b) => a.mapToItem(readout, 0, 0).x - b.mapToItem(readout, 0, 0).x)
                .map(label => label.text)
                .join(" ")
    }

    // An angle to two decimals, a rounded negative zero reading as 0.00.
    function twoDecimals(angle) {
        return (Number(angle.toFixed(2)) + 0).toFixed(2)
    }

    // Saves the fresh project as .cwproj and attaches the
    // survex_simple.svx fixture to a new trip via the cwRootData
    // wrapper. Returns { trip, source }.
    function attachFixtureTrip(projectBaseName) {
        const trip = makeSavedTrip(projectBaseName)

        const source = TestHelper.testcasesDatasetPath(
            "external-centerlines/survex_simple.svx")
        attachSourceToTrip(trip, source)
        return { trip: trip, source: source }
    }

    // Right-clicks the empty strip at the right end of an attached header's
    // file row and returns the file menu's area once the menu is open. The
    // whole row carries the menu, so the strip reaches it too.
    function openAttachedFileMenu(header) {
        const fileRow = findChild(header, "attachedHeaderFileRow")
        verify(fileRow !== null, "attachedHeaderFileRow must exist")
        const menuArea = findChild(header, "attachedFileContextMenu")
        verify(menuArea !== null, "attachedFileContextMenu must exist")

        // A just-shown header has no layout until the next frame.
        waitForRendering(header)
        const stripInset = 2
        mouseClick(fileRow, fileRow.width - stripInset, fileRow.height / 2,
                   Qt.RightButton)
        tryVerify(() => menuArea.menu.opened, 5000,
                  "right-clicking the file row's empty strip opens the file menu")
        return menuArea
    }

    function closeMenu(menuArea) {
        menuArea.menu.dismiss()
        tryVerify(() => !menuArea.menu.opened, 5000, "the menu closes")
    }
}
