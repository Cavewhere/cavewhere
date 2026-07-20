import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// Tests for AddSurveyFileDialog (Phase-2 commit 12 + the Add Trip UX
// plan's unified rework): the scan preview gating both choice buttons,
// attach through the manager's completion-signal bridge, import
// through cwSurveyImportManager's importFinished bridge, format-gated
// Import (Survex only), and the cancel-during-scan path.
MainWindowTest {
    id: rootId

    property Trip trip: null

    AddSurveyFileDialog {
        id: attachDialogId
        trip: rootId.trip
    }

    ExternalCenterlineTestCase {
        name: "AddSurveyFileDialog"
        when: windowShown

        SignalSpy {
            id: attachCompletedSpyId
            target: RootData.externalCenterlineManager
            signalName: "attachCompleted"
        }

        SignalSpy {
            id: importedSpyId
            target: attachDialogId
            signalName: "imported"
        }

        function init() {
            attachDialogId.close()
            RootData.futureManagerModel.waitForFinished()
            RootData.newProject()
            RootData.futureManagerModel.waitForFinished()
            rootId.trip = null
            attachCompletedSpyId.clear()
            importedSpyId.clear()
        }

        function cleanup() {
            attachDialogId.close()
            RootData.newProject()
        }

        // Opens the dialog on a fresh saved trip and returns the path
        // field, ready for a test to type a source path into.
        function openWithTrip(projectBaseName) {
            rootId.trip = makeSavedTrip(projectBaseName)
            attachDialogId.open()
            const pathField = findChild(attachDialogId, "sourcePathField")
            verify(pathField !== null, "sourcePathField must exist")
            tryVerify(() => pathField.visible, 5000, "dialog opens")
            return pathField
        }

        function test_attachFailureShowsInlineErrorAndStaysOpen() {
            const pathField = openWithTrip("attach-dialog-fail")
            const source = TestHelper.testcasesDatasetPath(
                "external-centerlines/survex_simple.svx")
            pathField.text = source

            const attachButton = findChild(attachDialogId, "attachButton")
            tryVerify(() => attachButton.enabled, 10000, "preview scan lands")

            // Make the owner busy through the wrapper, then click Attach
            // in the same turn - the dialog's own attach is refused with
            // the "in progress" failure report, deterministically
            // exercising the error-stays-open branch of the bridge
            // handler.
            RootData.attachTripCenterline(rootId.trip, source)
            attachButton.clicked()

            const errorLabel = findChild(attachDialogId, "scanErrorLabel")
            tryVerify(() => errorLabel.visible, 10000,
                      "the refused attach reports inline")
            verify(errorLabel.text.indexOf("in progress") >= 0,
                   "error names the refusal; got: " + errorLabel.text)
            verify(pathField.visible, "dialog stays open on failure")
            verify(!attachDialogId.attaching, "attaching resets on failure")

            // The direct attach still completes with its own success
            // report; the dialog (no longer attaching) must ignore it
            // and keep showing the failure.
            tryVerify(() => attachCompletedSpyId.count === 2, 10000,
                      "both reports drain")
            verify(errorLabel.visible, "sibling success report is not ours")
            verify(pathField.visible, "dialog still open")
            RootData.futureManagerModel.waitForFinished()
        }

        function test_attachHappyPath() {
            const pathField = openWithTrip("attach-dialog-happy")
            const source = TestHelper.testcasesDatasetPath(
                "external-centerlines/survex_simple.svx")
            pathField.text = source

            const formatLabel = findChild(attachDialogId, "formatLabel")
            verify(formatLabel !== null, "formatLabel must exist")
            tryCompare(formatLabel, "text", "Format: Survex (auto-detected)")

            const summaryLabel = findChild(attachDialogId, "scanSummaryLabel")
            tryVerify(() => summaryLabel.visible, 10000, "preview scan lands")
            verify(summaryLabel.text.indexOf("Found 1 ") >= 0,
                   "summary counts the single fixture file; got: " + summaryLabel.text)

            const errorLabel = findChild(attachDialogId, "scanErrorLabel")
            const warningLabel = findChild(attachDialogId, "scanWarningLabel")
            verify(!errorLabel.visible, "no error for a clean fixture")
            verify(!warningLabel.visible, "no warnings for a clean fixture")

            const attachButton = findChild(attachDialogId, "attachButton")
            tryVerify(() => attachButton.enabled, 5000, "Attach enables on a valid scan")
            mouseClick(attachButton)

            tryVerify(() => rootId.trip.externalCenterline.entryFile === "survex_simple.svx",
                      10000, "the trip becomes Attached")
            tryVerify(() => attachCompletedSpyId.count === 1, 10000,
                      "the bridge reports exactly once")
            const report = attachCompletedSpyId.signalArguments[0][0]
            verify(report.success, "the attach reports success")
            compare(report.warnings.length, 0, "no warnings for a clean fixture")

            // Source always remembered (direction change - no mode toggle).
            compare(RootData.externalSourceSettings.sourcePathFor(rootId.trip.id),
                    source)

            tryVerify(() => !pathField.visible, 5000, "dialog closes on success")
            RootData.futureManagerModel.waitForFinished()
        }

        function test_nonexistentPathShowsErrorAndDisablesAttach() {
            const pathField = openWithTrip("attach-dialog-missing")
            pathField.text = RootData.urlToLocal(TestHelper.tempDirectoryUrl())
                    + "/does_not_exist.svx"

            const errorLabel = findChild(attachDialogId, "scanErrorLabel")
            tryVerify(() => errorLabel.visible, 10000, "scan error shows")
            verify(errorLabel.text.length > 0, "error text is populated")

            const attachButton = findChild(attachDialogId, "attachButton")
            verify(!attachButton.enabled, "Attach stays disabled on a failed scan")

            // Cancel before any attach just dismisses.
            const cancelButton = findChild(attachDialogId, "cancelButton")
            mouseClick(cancelButton)
            tryVerify(() => !pathField.visible, 5000, "Cancel closes the dialog")
        }

        function test_missingIncludeWarnsButAttaches() {
            const pathField = openWithTrip("attach-dialog-warn")
            const source = TestHelper.testcasesDatasetPath(
                "external-centerlines/survex_with_missing_include.svx")
            pathField.text = source

            const warningLabel = findChild(attachDialogId, "scanWarningLabel")
            tryVerify(() => warningLabel.visible, 10000, "scan warning shows")
            verify(warningLabel.text.indexOf("missing *include") >= 0,
                   "warning names the missing include; got: " + warningLabel.text)

            const attachButton = findChild(attachDialogId, "attachButton")
            tryVerify(() => attachButton.enabled, 5000,
                      "warnings do not disable Attach (q18)")
            mouseClick(attachButton)

            tryVerify(() => attachCompletedSpyId.count === 1, 10000,
                      "the bridge reports exactly once")
            const report = attachCompletedSpyId.signalArguments[0][0]
            verify(report.success, "a warned attach still succeeds")
            verify(report.warnings.length > 0, "the bridge carries the scan warnings")
            verify(report.warnings.join("\n").indexOf("missing *include") >= 0,
                   "bridge warnings name the missing include; got: "
                   + report.warnings.join("\n"))
            tryVerify(() => rootId.trip.externalCenterline.entryFile.length > 0,
                      10000, "the trip becomes Attached")
            RootData.futureManagerModel.waitForFinished()
        }

        function test_importHappyPath() {
            const pathField = openWithTrip("import-dialog-happy")
            const source = TestHelper.testcasesDatasetPath(
                "external-centerlines/survex_simple.svx")
            pathField.text = source

            const importButton = findChild(attachDialogId, "importButton")
            verify(importButton !== null, "importButton must exist")
            tryVerify(() => importButton.enabled, 10000,
                      "Import enables on a valid Survex scan")

            // Direct invocation: the handler runs synchronously, so the
            // busy state is provably up before the async task lands.
            importButton.clicked()
            verify(attachDialogId.importing, "import busy state is up")
            const cancelButton = findChild(attachDialogId, "cancelButton")
            verify(!cancelButton.enabled, "no cancel path during import")

            tryVerify(() => importedSpyId.count === 1, 10000,
                      "importFinished bridges to exactly one imported()")
            compare(importedSpyId.signalArguments[0][0], source,
                    "imported() carries the picked path for host naming")
            verify(!attachDialogId.importing, "busy state resets")
            tryVerify(() => !pathField.visible, 5000, "dialog closes on import")

            tryVerify(() => rootId.trip.chunkCount > 0, 10000,
                      "the survex data lands in the trip as native chunks")
            compare(rootId.trip.externalCenterline.entryFile, "",
                    "an imported trip stays Native")
            RootData.futureManagerModel.waitForFinished()
        }

        function test_compassFileDisablesImportOnly() {
            const pathField = openWithTrip("import-dialog-compass")
            pathField.text = TestHelper.testcasesDatasetPath(
                "external-centerlines/compass_simple.dat")

            const attachButton = findChild(attachDialogId, "attachButton")
            tryVerify(() => attachButton.enabled, 10000,
                      "Compass scans are attachable")

            const importButton = findChild(attachDialogId, "importButton")
            verify(!importButton.enabled, "Import is Survex-only")
            const formatNote = findChild(attachDialogId, "importFormatNote")
            verify(formatNote.visible, "the disabled Import explains itself")

            const cancelButton = findChild(attachDialogId, "cancelButton")
            mouseClick(cancelButton)
            tryVerify(() => !pathField.visible, 5000, "Cancel closes the dialog")
        }

        function test_cancelDuringScanLeavesTripNative() {
            const pathField = openWithTrip("attach-dialog-cancel")
            const source = TestHelper.testcasesDatasetPath(
                "external-centerlines/survex_simple.svx")
            pathField.text = source

            const attachButton = findChild(attachDialogId, "attachButton")
            tryVerify(() => attachButton.enabled, 10000, "preview scan lands")
            const cancelButton = findChild(attachDialogId, "cancelButton")

            // Invoke the handlers directly instead of mouseClick: the
            // two must run back-to-back with no event processing in
            // between, so the cancel flag is provably set before the
            // attach's scan continuation can land - mouseClick spins
            // the event loop and would race the (fast) fixture scan.
            attachButton.clicked()
            verify(attachDialogId.attaching, "attach started")
            cancelButton.clicked()

            tryVerify(() => attachCompletedSpyId.count === 1, 10000,
                      "the bridge reports exactly once")
            const report = attachCompletedSpyId.signalArguments[0][0]
            verify(report.canceled, "the report is the canceled shape")
            verify(!report.success, "a canceled attach is not a success")
            compare(report.errorMessage, "", "a canceled attach is not an error")
            // Pin the uuid shape first so the string compare below can't
            // go vacuous through a generic Object toString (panel-test
            // idiom).
            verify(String(report.ownerId).indexOf("{") === 0,
                   "ownerId stringifies to a uuid; got: " + String(report.ownerId))
            compare(String(report.ownerId), String(rootId.trip.id))

            compare(rootId.trip.externalCenterline.entryFile, "",
                    "no data-model change")
            compare(RootData.externalSourceSettings.sourcePathFor(rootId.trip.id),
                    "", "no source remembered")
            tryVerify(() => !RootData.externalCenterlineManager.isOwnerBusy(rootId.trip.id),
                      5000, "the busy token releases")
            tryVerify(() => !pathField.visible, 5000, "dialog closes on cancel")
            RootData.futureManagerModel.waitForFinished()
        }
    }
}
