import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    ExternalCenterlineTestCase {
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

        function test_nativeProjectHidesExternalSections() {
            const page = gotoCavernOutput()

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

        function test_attachedRowShowsTheExclusionReason() {
            const attached = attachFixtureTrip("cavern-attached-error")

            // Edit the in-project copy the way a user's editor would, into
            // an *include that reaches outside the project's data root —
            // the solve drops the owner and the row is where it says so.
            const copyPath = TestHelper.externalCenterlineCopyPath(
                RootData.project, attached.trip, "survex_simple.svx")
            verify(copyPath.length > 0, "the in-project copy must have a path")
            const copyDir = copyPath.substring(0, copyPath.lastIndexOf("/"))

            const escapingTarget = "../../../../../outside.svx"
            verify(TestHelper.writeTextFile(
                       copyDir + "/" + escapingTarget,
                       "*begin Included\n*fix I1 0 0 0\n*end Included\n"),
                   "the escaping include target must be written")
            verify(TestHelper.writeTextFile(
                       copyPath,
                       "*begin Entry\n*include \"" + escapingTarget + "\"\n*end Entry\n"),
                   "the entry copy must be rewritten")

            RootData.externalCenterlineManager.rescanAttachments()

            const page = gotoCavernOutput()
            const section = findChild(page, "attachedCenterlinesSection")
            verify(section !== null, "attachedCenterlinesSection must exist")
            tryVerify(() => section.visible, 5000,
                      "attached section becomes visible after attach")

            // The delegate is looked up under tryVerify: the page item is
            // cached across tests, so it may still be laying out on the frame
            // the page first shows.
            let errorLabel = null
            tryVerify(() => {
                          errorLabel = findChild(page, "attachedRowError")
                          return errorLabel !== null
                      }, 5000, "attached row must carry an error label")
            tryVerify(() => errorLabel.visible
                            && errorLabel.text.indexOf("outside.svx") >= 0,
                      10000,
                      "row shows the containment reason; got: " + errorLabel.text)
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
