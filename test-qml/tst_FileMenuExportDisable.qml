import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// The Survex/Compass/Chipdata export menu items live in
// ExportImportButtons (on DataMainPage); this covers their
// cwSurveyExportManager::canExport / exportDisabledReason wire-up.
MainWindowTest {
    id: rootId

    TestCase {
        name: "FileMenuExportDisable"
        when: windowShown

        function init() {
            RootData.futureManagerModel.waitForFinished()
            RootData.newProject()
            RootData.futureManagerModel.waitForFinished()
        }

        function cleanup() {
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.newProject()
        }

        function gotoDataMainPage() {
            RootData.pageSelectionModel.currentPageAddress = "Source/Data"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "dataMainPage",
                      5000, "should land on dataMainPage")
            return RootData.pageView.currentPageItem
        }

        function attachFixtureTrip(projectBaseName) {
            RootData.account.name = "Export Disable Test"
            RootData.account.email = "export.disable@example.com"

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
        }

        function test_nativeRegionExportsEnabled() {
            RootData.region.addCave()
            const dataPage = gotoDataMainPage()

            const regionItem = findChild(dataPage, "survexRegionExportMenuItem")
            verify(regionItem !== null, "region export item must exist")
            verify(regionItem.enabled, "region export enabled on a native region")

            const compassItem = findChild(dataPage, "compassCaveExportMenuItem")
            verify(compassItem !== null, "compass export item must exist")
            verify(compassItem.exportAllowed,
                   "compass export gate open on a native region")
            compare(compassItem.disabledReason, "")
        }

        function test_attachmentDisablesExports() {
            attachFixtureTrip("export-disable")
            const dataPage = gotoDataMainPage()

            const regionItem = findChild(dataPage, "survexRegionExportMenuItem")
            verify(regionItem !== null, "region export item must exist")
            tryVerify(() => !regionItem.enabled, 5000,
                      "region export disabled once an attachment exists")

            const tripItem = findChild(dataPage, "survexTripExportMenuItem")
            verify(tripItem !== null, "trip export item must exist")
            verify(!tripItem.exportAllowed, "trip export gate closed")
            verify(!tripItem.enabled, "trip export item disabled")

            const compassItem = findChild(dataPage, "compassCaveExportMenuItem")
            verify(compassItem !== null, "compass export item must exist")
            verify(!compassItem.exportAllowed, "compass export gate closed")
            verify(!compassItem.enabled, "compass export item disabled")

            // The tooltip carries cwSurveyExportManager::exportDisabledReason.
            verify(compassItem.disabledReason.indexOf("Cannot export") === 0,
                   "tooltip reason bound; got: " + compassItem.disabledReason)
            compare(tripItem.disabledReason, compassItem.disabledReason)
        }
    }
}
