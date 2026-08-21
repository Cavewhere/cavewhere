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
        RootData.account.name = "External Test"
        RootData.account.email = "external.test@example.com"

        RootData.region.addCave()
        const cave = RootData.region.cave(0)
        cave.addTrip()
        const trip = cave.trip(0)

        const tmpPath = RootData.urlToLocal(TestHelper.tempDirectoryUrl())
        const projectPath = tmpPath + "/" + projectBaseName + ".cwproj"
        verify(RootData.project.saveAs(projectPath), "saveAs should succeed")
        TestHelper.waitForProjectSaveToFinish(RootData.project)
        return trip
    }

    // Attaches source to trip through the cwRootData wrapper and waits
    // for this attachment to land. Counts rows from where the model
    // already stood, so a project that holds earlier attachments still
    // waits for the new one.
    function attachSourceToTrip(trip, source) {
        const model = RootData.externalCenterlineManager.attachedCenterlinesModel
        const rowsBefore = model.rowCount()
        RootData.attachTripCenterline(trip, source)
        tryVerify(() => model.rowCount() > rowsBefore,
                  10000, "attach should land a row in the attached model")
        RootData.futureManagerModel.waitForFinished()
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
}
