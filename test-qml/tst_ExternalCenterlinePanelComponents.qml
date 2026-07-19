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
        scopePrefix: "cave_"
        network: RootData.linePlotManager.regionNetwork
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

        ExternalCenterlineMissingSourceBanner {
            id: bannerId
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
    }

    ExternalCenterlineTestCase {
        name: "ExternalCenterlinePanelComponents"
        when: windowShown

        SignalSpy {
            id: forgetSpyId
            target: bannerId
            signalName: "forgetSourceRequested"
        }

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
            forgetSpyId.clear()
            viewOutputSpyId.clear()
            stationClickSpyId.clear()
            dateSpyId.clear()
        }

        function cleanup() {
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

            RootData.externalSourceSettings.clearSourcePath(fixture.trip.id)
            tryVerify(() => sourceLabel.text === "Source forgotten (this machine)",
                      5000, "forgotten-source line renders after clearing; got: "
                            + sourceLabel.text)
        }

        function test_missingSourceBannerLightsUpAndForgets() {
            const fixture = attachFixtureTrip("panel-banner")
            rootId.trip = fixture.trip
            bannerId.sourcePath =
                RootData.externalSourceSettings.sourcePathFor(fixture.trip.id)

            verify(!bannerId.visible, "banner hidden by default")

            // The panel's binding (commit 11): membership re-evaluated on
            // missingSourceOwnersChanged via the property dependency.
            bannerId.visible = Qt.binding(() => {
                const manager = RootData.externalCenterlineManager
                manager.missingSourceOwners
                return manager.isSourceMissing(fixture.trip.id)
            })
            verify(!bannerId.visible, "still hidden while the source exists")

            TestHelper.removeFile(TestHelper.toLocalUrl(fixture.source))
            tryVerify(() => bannerId.visible, 10000,
                      "banner appears once the watcher reports the missing source")

            const label = findChild(bannerId, "missingSourceLabel")
            verify(label !== null, "missingSourceLabel must exist")
            verify(label.text.indexOf("survex_simple.svx") >= 0,
                   "banner names the missing path; got: " + label.text)

            const relinkButton = findChild(bannerId, "relinkButton")
            verify(relinkButton !== null, "relinkButton must exist")
            verify(!relinkButton.enabled, "Re-link is stubbed disabled")
            verify(bannerId.relinkDisabledReason.indexOf("future release") >= 0,
                   "tooltip explains the stub; got: " + bannerId.relinkDisabledReason)

            const forgetButton = findChild(bannerId, "forgetSourceButton")
            verify(forgetButton !== null, "forgetSourceButton must exist")
            mouseClick(forgetButton)
            compare(forgetSpyId.count, 1, "Forget source raises the signal")

            // Break the fixture-bound binding — the trip dies with cleanup's
            // newProject but the banner item is cached across tests.
            bannerId.visible = false
            bannerId.sourcePath = ""
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
            attachFixtureTrip("panel-stations")

            tryVerify(() => RootData.linePlotManager.lastSolveStationCount > 0,
                      10000, "the attach-chained solve should publish stations")
            tryVerify(() => stationsListId.count > 0, 10000,
                      "rows appear once the network lands in the scope model")

            const listView = findChild(stationsListId, "stationsListView")
            verify(listView !== null, "stationsListView must exist")
            tryVerify(() => listView.itemAtIndex(0) !== null, 5000,
                      "first delegate must materialize")

            mouseClick(listView.itemAtIndex(0))
            compare(stationClickSpyId.count, 1, "clicking a station emits stationClicked")
            const qualifiedName = stationClickSpyId.signalArguments[0][0]
            verify(qualifiedName.indexOf("cave_") === 0,
                   "signal carries the qualified name; got: " + qualifiedName)
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
    }
}
