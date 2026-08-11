import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// Isolation tests for the ExternalCenterlineTripPanel sub-components
// (commit 10). The panel itself assembles them in commit 11.
MainWindowTest {
    id: rootId

    property Trip trip: null

    ScopeStationListModel {
        id: scopeModelId
        trip: rootId.trip
    }

    QQ.Column {
        id: componentColumnId
        width: 500
        spacing: 8

        ExternalCenterlineAttachedHeader {
            id: attachedHeaderId
            width: parent.width
            trip: rootId.trip
            externalSourceSettings: RootData.externalSourceSettings
        }

        ExternalCenterlineSolveStatus {
            id: solveStatusId
            width: parent.width
        }

        ExternalCenterlineStationsList {
            id: stationsListId
            width: parent.width
            height: 150
            stationModel: scopeModelId
        }

        ExternalCenterlineTripMetadata {
            id: tripMetadataId
            width: parent.width
            trip: rootId.trip
        }

        // No trip, so it never has a row — which is the state the banner's
        // "nothing to tie to" copy exists for.
        TieSuggestionModel {
            id: emptySuggestionsId
        }

        FloatingSurveyBanner {
            id: floatingBannerId
            width: parent.width
            floating: false
        }

        ExternalCenterlineFileErrorBanner {
            id: fileErrorBannerId
            width: parent.width
        }
    }

    ReplaceCenterlineDialog {
        id: replaceDialogId
        trip: rootId.trip
    }

    ExternalCenterlineTestCase {
        name: "ExternalCenterlinePanelComponents"
        when: windowShown

        SignalSpy {
            id: viewOutputSpyId
            target: solveStatusId
            signalName: "viewCavernOutputRequested"
        }

        SignalSpy {
            id: stationClickSpyId
            target: stationsListId
            signalName: "stationClicked"
        }

        SignalSpy {
            id: dateSpyId
            signalName: "dateChanged"
        }

        function init() {
            RootData.futureManagerModel.waitForFinished()
            RootData.newProject()
            RootData.futureManagerModel.waitForFinished()
            rootId.trip = null
            viewOutputSpyId.clear()
            stationClickSpyId.clear()
            dateSpyId.clear()
        }

        function cleanup() {
            // A failure inside the Replace test can leave its modal dialog
            // up, and a modal overlay swallows every later test's clicks.
            const replaceCancel = findChild(replaceDialogId, "replaceCancelButton")
            if (replaceCancel !== null) {
                replaceCancel.clicked()
            }
            RootData.newProject()
        }

        function test_attachedHeaderShowsFileFormatAndSource() {
            const fixture = attachFixtureTrip("panel-header")
            rootId.trip = fixture.trip

            const fileLabel = findChild(attachedHeaderId, "attachedFileLabel")
            verify(fileLabel !== null, "attachedFileLabel must exist")
            tryCompare(fileLabel, "text", "survex_simple.svx")

            const formatLabel = findChild(attachedHeaderId, "attachedFormatLabel")
            verify(formatLabel !== null, "attachedFormatLabel must exist")
            compare(formatLabel.text, "Survex")
            verify(formatLabel.visible)

            const sourceLabel = findChild(attachedHeaderId, "sourceModeLabel")
            verify(sourceLabel !== null, "sourceModeLabel must exist")
            verify(sourceLabel.text.indexOf("Source:") === 0,
                   "remembered source renders; got: " + sourceLabel.text)
            verify(sourceLabel.text.indexOf("survex_simple.svx") >= 0,
                   "source line carries the path; got: " + sourceLabel.text)

            RootData.externalSourceSettings.clearBreadcrumb(fixture.trip.id)
            tryVerify(() => sourceLabel.text === "Source forgotten (this machine)",
                      5000, "forgotten-source line renders after clearing; got: "
                            + sourceLabel.text)
        }

        // The commit-4 gate (plans/EXTERNAL_FILE_LIVE_LINK_RETIREMENT.html
        // §6): the breadcrumb is the Replace dialog's starting folder and
        // nothing else. Forgetting it drops the dialog back on the default
        // folder, with no error and no other visible change.
        function test_replaceDialogBrowsesFromTheBreadcrumbFolder() {
            const fixture = attachFixtureTrip("panel-replace-breadcrumb")
            rootId.trip = fixture.trip

            const breadcrumb = RootData.externalSourceSettings
                                   .breadcrumbPath(fixture.trip.id)
            verify(breadcrumb.length > 0, "the attach remembered a source path")
            const breadcrumbFolder = breadcrumb.substring(0, breadcrumb.lastIndexOf("/"))
            // Both folders below have to be distinct for either compare to
            // mean anything.
            verify(RootData.urlToLocal(RootData.lastDirectory) !== breadcrumbFolder,
                   "the default folder differs from the breadcrumb's")

            replaceDialogId.open()
            const fileDialog = findChild(replaceDialogId, "entryFileDialog")
            verify(fileDialog !== null, "entryFileDialog must exist")
            compare(RootData.urlToLocal(fileDialog.currentFolder), breadcrumbFolder,
                    "Browse starts in the folder the trip was attached from")

            const pathField = findChild(replaceDialogId, "sourcePathField")
            verify(pathField !== null, "sourcePathField must exist")
            compare(pathField.text, "",
                    "the breadcrumb chooses a folder, never a replacement file")

            const cancelButton = findChild(replaceDialogId, "replaceCancelButton")
            verify(cancelButton !== null, "replaceCancelButton must exist")
            cancelButton.clicked()
            tryVerify(() => !pathField.visible, 5000, "the dialog closes on cancel")

            RootData.externalSourceSettings.clearBreadcrumb(fixture.trip.id)
            compare(RootData.externalSourceSettings.breadcrumbPath(fixture.trip.id), "",
                    "clearing drops the breadcrumb")

            replaceDialogId.open()
            tryVerify(() => pathField.visible, 5000, "the dialog reopens")
            compare(String(fileDialog.currentFolder), String(RootData.lastDirectory),
                    "a forgotten breadcrumb falls back to the default folder")

            const errorLabel = findChild(replaceDialogId, "scanErrorLabel")
            verify(errorLabel !== null, "scanErrorLabel must exist")
            verify(!errorLabel.visible,
                   "a forgotten breadcrumb is not an error; got: " + errorLabel.text)

            cancelButton.clicked()
            tryVerify(() => !pathField.visible, 5000, "the dialog closes on cancel")
        }

        function test_solveStatusDotColorsAndLink() {
            solveStatusId.hasError = false
            solveStatusId.warningCount = 0
            solveStatusId.stationCount = 42

            const dot = findChild(solveStatusId, "solveStatusDot")
            verify(dot !== null, "solveStatusDot must exist")
            verify(Qt.colorEqual(dot.color, Theme.success),
                   "clean solve renders green; got: " + dot.color)

            const label = findChild(solveStatusId, "solveStatusLabel")
            verify(label !== null, "solveStatusLabel must exist")
            verify(label.text.indexOf("42") >= 0,
                   "station count renders; got: " + label.text)

            const pill = findChild(solveStatusId, "solveWarningPill")
            verify(pill !== null, "solveWarningPill must exist")
            verify(!pill.visible, "no warning pill on a clean solve")

            solveStatusId.warningCount = 1
            verify(Qt.colorEqual(dot.color, Theme.warning),
                   "a warning renders yellow; got: " + dot.color)
            verify(pill.visible, "warning pill appears")
            verify(pill.text.indexOf("1") >= 0,
                   "pill carries the count; got: " + pill.text)

            solveStatusId.hasError = true
            verify(Qt.colorEqual(dot.color, Theme.danger),
                   "an error renders red; got: " + dot.color)
            compare(label.text, "Solve failed")

            solveStatusId.hasError = false
            solveStatusId.warningCount = 0
            solveStatusId.stationCount = 0

            const link = findChild(solveStatusId, "viewCavernOutputLink")
            verify(link !== null, "viewCavernOutputLink must exist")
            mouseClick(link)
            compare(viewOutputSpyId.count, 1, "link raises the deep-link signal")
        }

        function test_stationsListShowsRowsAndClickEmits() {
            const fixture = attachFixtureTrip("panel-stations")
            //The scope model lists one trip's stations, so it needs the trip
            rootId.trip = fixture.trip

            tryVerify(() => RootData.linePlotManager.lastSolveStationCount > 0,
                      10000, "the attach-chained solve should publish stations")
            tryVerify(() => stationsListId.count > 0, 10000,
                      "rows appear once the solve lands in the cave's lookup")

            const listView = findChild(stationsListId, "stationsListView")
            verify(listView !== null, "stationsListView must exist")
            tryVerify(() => listView.itemAtIndex(0) !== null, 5000,
                      "first delegate must materialize")

            mouseClick(listView.itemAtIndex(0))
            compare(stationClickSpyId.count, 1, "clicking a station emits stationClicked")
            const handle = stationClickSpyId.signalArguments[0][0]
            compare(handle.scope, CwStationHandle.Trip,
                    "an attached trip's stations live in the trip's scope")
            // QUuid reaches JS as an opaque wrapper, so compare the string forms.
            compare(String(handle.containerId), String(rootId.trip.id),
                    "the handle names the clicked station's trip")
            compare(handle.tail, listView.itemAtIndex(0).text,
                    "the handle carries the row's scope-relative tail")
        }

        function test_tripMetadataDateAndDeclination() {
            RootData.region.addCave()
            const cave = RootData.region.cave(0)

            // Fix station + date make auto-declination available, so the
            // declination editor shows its Auto/Manual mode combo.
            cave.fixStations.addFixStation()
            const fixModel = cave.fixStations
            const fixIndex = fixModel.index(0)
            fixModel.setData(fixIndex, "a1", FixStationModel.StationNameRole)
            fixModel.setData(fixIndex, "EPSG:32613", FixStationModel.InputCSRole)
            fixModel.setData(fixIndex, 478000.0, FixStationModel.EastingRole)
            fixModel.setData(fixIndex, 4430000.0, FixStationModel.NorthingRole)
            fixModel.setData(fixIndex, 1655.0, FixStationModel.ElevationRole)

            cave.addTrip()
            const trip = cave.trip(0)
            trip.date = new Date(2024, 5, 1)
            rootId.trip = trip

            dateSpyId.target = trip
            const dateInput = findChild(tripMetadataId, "tripMetadataDate")
            verify(dateInput !== null, "tripMetadataDate must exist")
            dateInput.finishedEditting("2025-03-04")
            tryVerify(() => dateSpyId.count === 1, 1000,
                      "date edit emits trip.dateChanged")
            compare(Qt.formatDate(trip.date, "yyyy-MM-dd"), "2025-03-04")

            const declEditor = findChild(tripMetadataId, "tripMetadataDeclination")
            verify(declEditor !== null, "tripMetadataDeclination must exist")
            verify(declEditor.visible, "editor shown when the file has no declination")

            tryVerify(() => trip.calibration.autoDeclinationAvailable, 5000,
                      "fix station + date make auto-declination available")
            verify(trip.calibration.autoDeclination, "trips default to auto")

            const modeCombo = findChild(declEditor, "declinationModeCombo")
            verify(modeCombo !== null, "declinationModeCombo must exist")
            modeCombo.activated(1)
            tryVerify(() => !trip.calibration.autoDeclination, 1000,
                      "editing declination to Manual flips autoDeclination off")

            const declInput = findChild(declEditor, "declinationEdit")
            verify(declInput !== null, "declinationEdit must exist")
            declInput.finishedEditting("3.25")
            tryVerify(() => Math.abs(trip.calibration.declinationManual - 3.25) < 1e-6,
                      1000, "manual edit writes declinationManual")

            tripMetadataId.fileOwnsDeclination = true
            verify(!declEditor.visible, "file-owned declination hides the editor")
            const hint = findChild(tripMetadataId, "fileOwnsDeclinationHint")
            verify(hint !== null, "fileOwnsDeclinationHint must exist")
            verify(hint.visible, "file-owned declination shows the hint")

            tripMetadataId.fileOwnsDeclination = false
            dateSpyId.target = null
        }

        function test_floatingBannerSpeaksForBothWaysASurveyFloats() {
            verify(!floatingBannerId.visible, "a survey that is tied in shows nothing")

            floatingBannerId.floating = true
            floatingBannerId.stations = ["a1", "a2"]

            verify(floatingBannerId.visible, "a floating survey banners itself")
            const detail = findChild(floatingBannerId, "floatingSurveyDetail")
            verify(detail !== null, "floatingSurveyDetail must exist")
            verify(detail.text.indexOf("can't be placed with the others") >= 0,
                   "adrift-with-names copy; got: " + detail.text)

            const stations = findChild(floatingBannerId, "floatingSurveyStations")
            verify(stations !== null, "floatingSurveyStations must exist")
            verify(stations.visible, "the stations line lists what floats")
            verify(stations.text.indexOf("a1, a2") >= 0,
                   "stations render in the survey's own namespace; got: " + stations.text)

            const noTies = findChild(floatingBannerId, "tieSuggestionsEmpty")
            verify(noTies !== null, "tieSuggestionsEmpty must exist")
            verify(!noTies.visible,
                   "a surface that offers no suggestions at all stays read-only")

            floatingBannerId.suggestions = emptySuggestionsId
            verify(noTies.visible, "a suggester that found nothing says so")
            // Pinned because it is a claim about what a rename buys, not a
            // label: an attachment is always scoped, so matching names stay
            // separate stations until a tie is actually made.
            verify(noTies.text.indexOf("Renaming one to match will offer a one-click tie") >= 0,
                   "no-tie copy promises a suggestion, not a connection; got: " + noTies.text)

            // The other shape: a survey whose file cannot be read at all floats
            // with no station to name, since neither the solve nor the scan's
            // harvest could learn one. Which is why the banner is driven by
            // `floating` and not by the station list.
            floatingBannerId.stations = []
            verify(floatingBannerId.visible, "a survey with no readable names still banners")
            verify(detail.text.indexOf("couldn't be read from its file") >= 0,
                   "unreadable copy; got: " + detail.text)
            verify(!stations.visible, "no stations line when no name could be read")
            verify(!noTies.visible,
                   "no names to tie means nothing to say about renaming them")

            floatingBannerId.suggestions = null
            floatingBannerId.floating = false
            verify(!floatingBannerId.visible)
        }

        function test_fileErrorBannerCarriesCavernsOwnText() {
            verify(!fileErrorBannerId.visible,
                   "a file cavern read without complaint banners nothing")

            const complaint = "broken.svx:3: Expecting numeric field"
            fileErrorBannerId.errorMessage = complaint
            verify(fileErrorBannerId.visible, "a complaint shows itself")

            const title = findChild(fileErrorBannerId, "fileErrorTitle")
            verify(title !== null && title.visible, "fileErrorTitle must render")

            // Verbatim, and not summarized: the line and column cavern gives are
            // the whole reason this beats the region-level log.
            const message = findChild(fileErrorBannerId, "fileErrorMessage")
            verify(message !== null, "fileErrorMessage must exist")
            compare(message.text, complaint)

            // A file cavern hates produces a complaint per problem, and the
            // panel's way out of that state — Reload, Replace — sits below this
            // banner. So the log scrolls past a point instead of growing.
            const shortHeight = fileErrorBannerId.height
            let manyComplaints = []
            for (let i = 1; i <= 60; i++) {
                manyComplaints.push("broken.svx:" + i + ": Expecting numeric field")
            }
            fileErrorBannerId.errorMessage = manyComplaints.join("\n")

            const messageArea = findChild(fileErrorBannerId, "fileErrorMessageArea")
            verify(messageArea !== null, "fileErrorMessageArea must exist")
            tryVerify(() => messageArea.contentHeight > messageArea.height, 1000,
                      "60 complaints overflow the area, so there is something to scroll")
            verify(fileErrorBannerId.height
                   < shortHeight + fileErrorBannerId.maximumMessageHeight,
                   "the banner stops growing; got: " + fileErrorBannerId.height)
            compare(message.text, manyComplaints.join("\n"),
                    "and every line is still there to scroll to")

            fileErrorBannerId.errorMessage = ""
            verify(!fileErrorBannerId.visible, "fixing the file takes the banner down")
        }
    }
}
