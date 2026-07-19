import QtQuick
import cavewherelib
import cw.TestLib

// Shared fixtures for the external-centerline test files
// (tst_CavernOutputPageExternal, tst_FileMenuExportDisable, ...).
CWTestCase {

    // Saves the fresh project as .cwproj and attaches the
    // survex_simple.svx fixture to a new trip via the cwRootData
    // wrapper. Returns { trip, source }.
    function attachFixtureTrip(projectBaseName) {
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

        const source = TestHelper.testcasesDatasetPath(
            "external-centerlines/survex_simple.svx")
        RootData.attachTripCenterline(trip, source)
        tryVerify(() => RootData.externalCenterlineManager
                            .attachedCenterlinesModel.rowCount() > 0,
                  10000, "attach should land a row in the attached model")
        RootData.futureManagerModel.waitForFinished()
        return { trip: trip, source: source }
    }
}
