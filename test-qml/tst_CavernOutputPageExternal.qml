import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "CavernOutputPageExternal"
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

        function gotoCavernOutput() {
            RootData.pageSelectionModel.gotoPageByName(null, "Cavern")
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "cavernOutputPage",
                      5000, "should land on cavernOutputPage")
            return RootData.pageView.currentPageItem
        }

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

        function test_nativeProjectHidesExternalSections() {
            const page = gotoCavernOutput()

            const banner = findChild(page, "missingSourceBanner")
            verify(banner !== null, "missingSourceBanner must exist")
            verify(!banner.visible, "banner hidden with no attachments")

            const section = findChild(page, "attachedCenterlinesSection")
            verify(section !== null, "attachedCenterlinesSection must exist")
            verify(!section.visible, "attached section hidden with no attachments")

            const inputButton = findChild(page, "cavernInputButton")
            verify(inputButton !== null, "cavernInputButton must exist")
            const textArea = findChild(page, "cavernTextArea")
            verify(textArea !== null, "cavernTextArea must exist")

            mouseClick(inputButton)
            compare(textArea.text, "",
                    "cavern input empty on a fresh project (init()'s newProject "
                    + "republishes an empty result)")

            // Restore the default selection for later tests — the page
            // item is cached across tests.
            mouseClick(findChild(page, "cavernOutputButton"))
        }

        function test_attachShowsAttachedCenterlinesSection() {
            attachFixtureTrip("cavern-attached-section")
            const page = gotoCavernOutput()

            const section = findChild(page, "attachedCenterlinesSection")
            verify(section !== null, "attachedCenterlinesSection must exist")
            tryVerify(() => section.visible, 5000,
                      "attached section becomes visible after attach")

            const rowLabel = findChild(page, "attachedRowLabel")
            verify(rowLabel !== null, "attached row must exist")
            verify(rowLabel.text.indexOf("survex_simple.svx") >= 0,
                   "row shows the entry file; got: " + rowLabel.text)
        }

        function test_cavernInputShowsDriverSource() {
            attachFixtureTrip("cavern-driver-source")

            // The attach's recompute chains a solve; the generated survex
            // input is published with the cavern output.
            tryVerify(() => RootData.linePlotManager.driverSource.length > 0,
                      10000, "solve should publish the survex input")

            const page = gotoCavernOutput()
            const inputButton = findChild(page, "cavernInputButton")
            verify(inputButton !== null, "cavernInputButton must exist")
            mouseClick(inputButton)

            const textArea = findChild(page, "cavernTextArea")
            verify(textArea !== null, "cavernTextArea must exist")
            tryVerify(() => textArea.text.indexOf("*include") >= 0, 5000,
                      "cavern input shows the attachment *include; got: "
                      + textArea.text.substring(0, 200))

            // Restore the default selection for later tests — the page
            // item is cached across tests.
            mouseClick(findChild(page, "cavernOutputButton"))
        }

        function test_statusLabelShowsSolveStats() {
            TestHelper.loadProjectFromFile(
                RootData.project,
                TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"))
            RootData.futureManagerModel.waitForFinished()

            tryVerify(() => RootData.linePlotManager.lastSolveDuration >= 0,
                      10000, "a solve should have stamped the duration")
            verify(RootData.linePlotManager.lastSolveStationCount > 0,
                   "solve should report stations")

            const page = gotoCavernOutput()
            const status = findChild(page, "statusLabel")
            verify(status !== null, "statusLabel must exist")
            const statsPattern = /^Last solve completed successfully in \d+(\.\d+)? s — \d+ stations, \d+ warnings\.$/
            tryVerify(() => statsPattern.test(status.text), 5000,
                      "status shows live stats; got: " + status.text)
        }

        function test_missingSourceRaisesBanner() {
            const fixture = attachFixtureTrip("cavern-missing-source")
            const page = gotoCavernOutput()

            const banner = findChild(page, "missingSourceBanner")
            verify(banner !== null, "missingSourceBanner must exist")
            verify(!banner.visible, "banner hidden while the source exists")

            // Deleting the remembered source fires the watcher, which
            // reclassifies the owner as missing on the next recompute.
            TestHelper.removeFile(TestHelper.toLocalUrl(fixture.source))
            tryVerify(() => banner.visible, 10000,
                      "banner appears once the watcher reports the missing source")
        }

        function test_inheritedStructureStillPresent() {
            // Smoke: the extension didn't break the pre-existing page
            // structure owned by tst_CavernOutputPage.
            const page = gotoCavernOutput()
            verify(findChild(page, "statusLabel") !== null)
            verify(findChild(page, "rerunSolveButton") !== null)
            verify(findChild(page, "cavernInputGroupBox") !== null)
            verify(findChild(page, "cavernOutputGroupBox") !== null)
            verify(findChild(page, "cavernInputButton") !== null)
            verify(findChild(page, "cavernOutputButton") !== null)
            verify(findChild(page, "loopClosureButton") !== null)
            verify(findChild(page, "cavernTextArea") !== null)
        }
    }
}
