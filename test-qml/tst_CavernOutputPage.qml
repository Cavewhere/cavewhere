import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
        name: "CavernOutputPage"
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

        function gotoDataMainPage() {
            RootData.pageSelectionModel.currentPageAddress = "Source/Data"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "dataMainPage",
                      5000, "should land on dataMainPage")
            return RootData.pageView.currentPageItem
        }

        function test_dataMainPageMenuNavigatesToCavernOutput() {
            // The Cavern Output page is reached from DataMainPage's top-right
            // hamburger menu (Solver-status info is an advanced-user concern
            // and was removed from the page body). Open the menu and click
            // the "Cavern Output" item.
            const dataPage = gotoDataMainPage()

            const menuButton = findChild(dataPage, "regionContextMenu")
            verify(menuButton !== null, "regionContextMenu must exist on DataMainPage")
            mouseClick(menuButton)

            const cavernItem = findChild(mainWindow, "cavernOutputMenuItem")
            verify(cavernItem !== null,
                   "Cavern Output menu item must exist in the regionContextMenu")
            mouseClick(cavernItem)

            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "cavernOutputPage",
                      5000, "clicking 'Cavern Output' must land on the Cavern page")
        }

        function test_emptyProjectShowsSuccessStatus() {
            // No solve has run yet on a freshly created project — cavernLog
            // empty, no SolveError, status reads "successful".
            const page = gotoCavernOutput()

            const status = findChild(page, "statusLabel")
            verify(status !== null, "statusLabel must exist")
            compare(status.text, qsTr("Last solve completed successfully."))

            const errorLabel = findChild(page, "errorMessageLabel")
            verify(errorLabel !== null, "errorMessageLabel must exist")
            verify(!errorLabel.visible, "error message hidden when no solve error")

            const textArea = findChild(page, "cavernTextArea")
            verify(textArea !== null, "cavernTextArea must exist")
            compare(textArea.text, "")
        }

        function test_solveCompletionPopulatesCavernLog() {
            // Loading a project triggers cwLinePlotManager → cavern → log.
            // Verify the cavern log Q_PROPERTY reaches the TextArea.
            TestHelper.loadProjectFromFile(
                RootData.project,
                TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"))
            RootData.futureManagerModel.waitForFinished()

            const page = gotoCavernOutput()
            const textArea = findChild(page, "cavernTextArea")
            verify(textArea !== null, "cavernTextArea must exist")

            // With "Cavern Output" selected (the default) the shared
            // TextArea binds to manager.cavernLog. After a real solve, both
            // the manager state and the visible text must be populated with
            // cavern's diagnostic output.
            tryVerify(() => RootData.linePlotManager.cavernLog.length > 0,
                      5000, "cavernLog Q_PROPERTY should be populated")
            tryVerify(() => textArea.text.length > 0,
                      5000, "TextArea must reflect the published cavernLog")

            // cavern emits a stats summary on success — sanity check the
            // shape of what landed in the TextArea.
            verify(textArea.text.indexOf("Survey contains") >= 0,
                   "log should contain cavern's 'Survey contains' stats line; got: "
                   + textArea.text.substring(0, 200))

            // hasSolveError stays false on a clean solve.
            verify(!RootData.linePlotManager.hasSolveError,
                   "hasSolveError should be false on a clean solve")

            // After a real solve the status line carries the live stats
            // suffix ("… in 0.4 s — 214 stations, 0 warnings.").
            const status = findChild(page, "statusLabel")
            verify(status.text.indexOf(qsTr("Last solve completed successfully")) === 0,
                   "status leads with the success text; got: " + status.text)
        }

        function test_switcherButtonsAreExclusive() {
            // Cavern Input / Cavern Output / Loop Closure act as a tab
            // switcher driving the single shared TextArea. On a fresh
            // project the output (log) is selected and loop closure is
            // disabled (no loops).
            const page = gotoCavernOutput()

            const inputButton = findChild(page, "cavernInputButton")
            verify(inputButton !== null, "cavernInputButton must exist")
            const outputButton = findChild(page, "cavernOutputButton")
            verify(outputButton !== null, "cavernOutputButton must exist")
            const loopButton = findChild(page, "loopClosureButton")
            verify(loopButton !== null, "loopClosureButton must exist")

            verify(outputButton.checked, "output selected by default")
            verify(!loopButton.enabled,
                   "loop closure disabled when the cave has no loops")

            mouseClick(inputButton)
            verify(inputButton.checked, "input selected after click")
            verify(!outputButton.checked, "output deselected — exclusive")

            // The page item is cached across tests — restore the default
            // selection so later tests see the pristine page.
            mouseClick(outputButton)
            verify(outputButton.checked, "output re-selected")
        }

        function test_rerunButtonInvokesManager() {
            // The Re-run solve button calls linePlotManager.rerunSurvex();
            // observe stationPositionInCavesChanged (fires per successful
            // solve regardless of whether log content changed) as proof a
            // new solve happened.
            TestHelper.loadProjectFromFile(
                RootData.project,
                TestHelper.testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"))
            RootData.futureManagerModel.waitForFinished()

            const page = gotoCavernOutput()
            const button = findChild(page, "rerunSolveButton")
            verify(button !== null, "rerunSolveButton must exist")
            verify(button.enabled, "rerunSolveButton should be enabled")

            const spy = signalSpyComponent.createObject(rootId, {
                target: RootData.linePlotManager,
                signalName: "stationPositionInCavesChanged"
            })
            verify(spy !== null, "could not create SignalSpy")

            mouseClick(button)
            RootData.futureManagerModel.waitForFinished()

            tryVerify(() => spy.count >= 1, 5000,
                      "stationPositionInCavesChanged should fire after rerunSurvex()")

            // After the rerun the cavern log is still populated (unchanged or
            // identical content — either way the manager state must hold).
            verify(RootData.linePlotManager.cavernLog.length > 0,
                   "cavernLog should still be populated after rerun")

            spy.destroy()
        }

        Component {
            id: signalSpyComponent
            SignalSpy { }
        }
    }
}
