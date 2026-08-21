import QtQuick as QQ
import QtQuick.Controls as QC
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

    // The team a native trip's table edits, kept out of any trip so the
    // read-only comparison stays a property of the table alone.
    Team {
        id: nativeTeamModelId
    }

    QQ.FontMetrics {
        id: fontMetricsId
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeBody
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

        // A TeamTable at its defaults, standing in for a native trip's.
        TeamTable {
            id: nativeTeamTableId
            width: parent.width
            model: nativeTeamModelId
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

    // Stands in for the panel's manager wiring: the header raises the
    // refresh and someone else answers it. Here the answer is the rule
    // the staged paths below exercise — the source has to be on this
    // machine before it can be copied back in.
    QQ.Connections {
        target: attachedHeaderId
        function onSourceEligibilityRefreshRequested() {
            attachedHeaderId.sourceReloadable =
                    RootData.pathExists(attachedHeaderId.rememberedSourcePath)
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

        SignalSpy {
            id: replaceRequestedSpyId
            target: attachedHeaderId
            signalName: "replaceRequested"
        }

        SignalSpy {
            id: reloadFromSourceSpyId
            target: attachedHeaderId
            signalName: "reloadFromSourceRequested"
        }

        function init() {
            RootData.futureManagerModel.waitForFinished()
            RootData.newProject()
            RootData.futureManagerModel.waitForFinished()
            rootId.trip = null
            viewOutputSpyId.clear()
            stationClickSpyId.clear()
            dateSpyId.clear()
            replaceRequestedSpyId.clear()
            attachedHeaderId.actionsEnabled = true
            attachedHeaderId.entryFilePath = ""
            attachedHeaderId.sourceReloadable = false
            attachedHeaderId.sourceChangedSinceCopy = false
            reloadFromSourceSpyId.clear()
            componentColumnId.y = 0
            rootId.closeAnyOpenEditor()
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

        // Attaches a fixture to the panel's trip and returns the stations
        // ListView once its first delegate has materialized.
        function stationsListViewWithRows(fixtureName) {
            const fixture = attachFixtureTrip(fixtureName)
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
            return listView
        }

        // Resolves the in-project copy the way the panel does and hands it
        // to the isolated header, which has no manager of its own.
        function stageEntryFilePath(fixture) {
            attachedHeaderId.entryFilePath = FileRevealer.resolvedPath(
                        RootData.externalCenterlineManager.attachmentDir(fixture.trip.id),
                        fixture.trip.externalCenterline.entryFile)
            return attachedHeaderId.entryFilePath
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
            // §16 B9a: the line reads as provenance — where the copy came
            // from — because every verb on this panel acts on the copy.
            verify(sourceLabel.text.startsWith("Copied from:"),
                   "remembered source renders; got: " + sourceLabel.text)
            verify(sourceLabel.text.includes("survex_simple.svx"),
                   "source line carries the path; got: " + sourceLabel.text)

            RootData.externalSourceSettings.clearBreadcrumb(fixture.trip.id)
            tryVerify(() => sourceLabel.text
                            === "Copied from an unknown location (this machine)",
                      5000, "unknown-origin line renders after clearing; got: "
                            + sourceLabel.text)
        }

        // EXTERNAL_SOURCE_CHANGE_NOTIFY §4: the provenance line carries the
        // "changed since copied" state inline, right where the source
        // context menu's Reload can act on it.
        function test_attachedHeaderMarksAChangedSource() {
            const fixture = attachFixtureTrip("panel-header-changed")
            rootId.trip = fixture.trip

            const sourceLabel = findChild(attachedHeaderId, "sourceModeLabel")
            verify(sourceLabel !== null, "sourceModeLabel must exist")
            tryVerify(() => attachedHeaderId.rememberedSourcePath.length > 0,
                      5000, "the attach remembers a source")
            verify(!sourceLabel.text.includes("changed since copied"),
                   "a source that matches its copy reads plain; got: "
                   + sourceLabel.text)

            attachedHeaderId.sourceChangedSinceCopy = true
            tryVerify(() => sourceLabel.text.includes("changed since copied"),
                      5000, "the suffix follows the state; got: "
                      + sourceLabel.text)
            verify(sourceLabel.text.includes(attachedHeaderId.rememberedSourcePath),
                   "the suffixed line still names the source; got: "
                   + sourceLabel.text)

            attachedHeaderId.sourceChangedSinceCopy = false
            tryVerify(() => !sourceLabel.text.includes("changed since copied"),
                      5000, "the suffix leaves with the state; got: "
                      + sourceLabel.text)

            // An unknown origin names no file that could have changed, so
            // it keeps the plain unknown-origin line.
            attachedHeaderId.sourceChangedSinceCopy = true
            RootData.externalSourceSettings.clearBreadcrumb(fixture.trip.id)
            tryCompare(sourceLabel, "text",
                       "Copied from an unknown location (this machine)")
        }

        // §16 B2a: the header carries the file action and only raises a
        // signal — the panel owns the manager wiring. §16 B9b took Reload
        // off this row: the watcher already covers project-copy edits.
        function test_attachedHeaderReplaceRaisesSignal() {
            verify(findChild(attachedHeaderId, "reloadButton") === null,
                   "the file row carries no Reload button")

            const replaceButton = findChild(attachedHeaderId, "replaceButton")
            verify(replaceButton !== null, "replaceButton must exist")
            verify(!replaceButton.visible, "an unattached header shows no Replace")

            const fixture = attachFixtureTrip("panel-header-actions")
            rootId.trip = fixture.trip
            tryVerify(() => replaceButton.visible,
                      5000, "attaching shows the file action")
            // A just-shown item has no layout until the next frame, so a
            // click before it lands on the header's old geometry.
            waitForRendering(attachedHeaderId)

            mouseClick(replaceButton)
            compare(replaceRequestedSpyId.count, 1, "Replace raises replaceRequested")

            attachedHeaderId.actionsEnabled = false
            tryVerify(() => !replaceButton.enabled,
                      5000, "a busy owner disables the action")
        }

        // §16 B2e: right-clicking the file name reaches the file on disk.
        // The menu only has to offer both verbs — running them would hand
        // the file to the desktop.
        function test_attachedFileRightClickOffersRevealAndOpen() {
            const fixture = attachFixtureTrip("panel-header-context-menu")
            rootId.trip = fixture.trip

            // The panel resolves this for the real header; here the
            // resolution itself is the thing being handed over.
            stageEntryFilePath(fixture)
            verify(attachedHeaderId.entryFilePath.indexOf("survex_simple.svx") > 0,
                   "the in-project copy resolves to an absolute path; got: "
                   + attachedHeaderId.entryFilePath)
            verify(RootData.pathExists(attachedHeaderId.entryFilePath),
                   "the resolved path names a file that is there")

            const contextMenu = findChild(attachedHeaderId, "attachedFileContextMenu")
            verify(contextMenu !== null, "attachedFileContextMenu must exist")

            const fileLabel = findChild(attachedHeaderId, "attachedFileLabel")
            verify(fileLabel !== null, "attachedFileLabel must exist")
            waitForRendering(attachedHeaderId)
            mouseClick(fileLabel, fileLabel.width / 2, fileLabel.height / 2,
                       Qt.RightButton)
            tryVerify(() => contextMenu.menu.opened, 5000,
                      "right-clicking the file name opens the menu")

            const revealItem = findChild(contextMenu, "showInFileManagerAction")
            verify(revealItem !== null, "showInFileManagerAction must exist")
            verify(revealItem.enabled, "a resolved path can be revealed")
            verify(revealItem.text.indexOf("Show in") === 0,
                   "the reveal item names the platform's file manager; got: "
                   + revealItem.text)

            const openItem = findChild(contextMenu, "openFileAction")
            verify(openItem !== null, "openFileAction must exist")
            verify(openItem.enabled, "a resolved path can be opened")
            compare(openItem.text, "Open")

            contextMenu.menu.dismiss()
            tryVerify(() => !contextMenu.menu.opened, 5000, "the menu closes")

            // A path that names a file which is gone (the missing-copy
            // case) leaves Open unavailable — the desktop would do nothing
            // with it — while reveal stays available and lands on the
            // containing folder.
            attachedHeaderId.entryFilePath = attachedHeaderId.entryFilePath + ".gone"
            mouseClick(fileLabel, fileLabel.width / 2, fileLabel.height / 2,
                       Qt.RightButton)
            tryVerify(() => contextMenu.menu.opened, 5000, "the menu opens again")
            verify(revealItem.enabled, "a deleted file still reveals its folder")
            verify(!openItem.enabled, "a deleted file offers nothing to open")

            contextMenu.menu.dismiss()
            tryVerify(() => !contextMenu.menu.opened, 5000, "the menu closes")

            // Nothing resolved leaves both verbs unavailable rather than
            // handing the desktop an empty path.
            attachedHeaderId.entryFilePath = ""
            mouseClick(fileLabel, fileLabel.width / 2, fileLabel.height / 2,
                       Qt.RightButton)
            tryVerify(() => contextMenu.menu.opened, 5000, "the menu opens again")
            tryVerify(() => !revealItem.enabled && !openItem.enabled, 5000,
                      "an unresolved path disables both actions")

            contextMenu.menu.dismiss()
            tryVerify(() => !contextMenu.menu.opened, 5000, "the menu closes")
        }

        // §16 B9c: right-clicking the provenance line reaches the source
        // file — the one path on this panel that no other verb touches.
        // Reload only raises a signal (the panel runs the manager's verb),
        // and reveal/open would hand the file to the desktop, so the menu
        // is checked for what it offers rather than run.
        function test_sourceLineRightClickOffersReloadRevealAndOpen() {
            const fixture = attachFixtureTrip("panel-source-context-menu")
            rootId.trip = fixture.trip

            const sourceLabel = findChild(attachedHeaderId, "sourceModeLabel")
            verify(sourceLabel !== null, "sourceModeLabel must exist")
            tryVerify(() => attachedHeaderId.rememberedSourcePath.length > 0,
                      5000, "the attach remembered where the copy came from")

            const contextMenu = findChild(attachedHeaderId, "sourceContextMenu")
            verify(contextMenu !== null, "sourceContextMenu must exist")
            verify(contextMenu.visible, "a known origin offers its menu")

            waitForRendering(attachedHeaderId)
            mouseClick(sourceLabel, sourceLabel.width / 2,
                       sourceLabel.height / 2, Qt.RightButton)
            tryVerify(() => contextMenu.menu.opened, 5000,
                      "right-clicking the source line opens the menu")

            const reloadItem = findChild(contextMenu, "reloadFromSourceAction")
            verify(reloadItem !== null, "reloadFromSourceAction must exist")
            compare(reloadItem.text, "Reload")
            verify(reloadItem.enabled,
                   "a source sitting on this machine can be copied back in")

            const revealItem = findChild(contextMenu, "showSourceInFileManagerAction")
            verify(revealItem !== null, "showSourceInFileManagerAction must exist")
            verify(revealItem.enabled, "a remembered source can be revealed")
            verify(revealItem.text.indexOf("Show in") === 0,
                   "the reveal item names the platform's file manager; got: "
                   + revealItem.text)

            const openItem = findChild(contextMenu, "openSourceAction")
            verify(openItem !== null, "openSourceAction must exist")
            compare(openItem.text, "Open")
            verify(openItem.enabled, "a source that is there can be opened")

            reloadItem.triggered()
            compare(reloadFromSourceSpyId.count, 1,
                    "Reload raises reloadFromSourceRequested for the panel")

            contextMenu.menu.dismiss()
            tryVerify(() => !contextMenu.menu.opened, 5000, "the menu closes")

            // A breadcrumb naming a file that is gone from this machine —
            // a moved source, or a project that arrived through Git. Both
            // verbs that touch the file go quiet; reveal stays available
            // and lands on the containing folder.
            attachedHeaderId.rememberedSourcePath = fixture.source + ".gone"
            mouseClick(sourceLabel, sourceLabel.width / 2,
                       sourceLabel.height / 2, Qt.RightButton)
            tryVerify(() => contextMenu.menu.opened, 5000, "the menu opens again")
            tryVerify(() => !reloadItem.enabled, 5000,
                      "a source that is gone offers nothing to copy back in")
            verify(!openItem.enabled, "a source that is gone offers nothing to open")
            verify(revealItem.enabled, "a missing source still reveals its folder")

            contextMenu.menu.dismiss()
            tryVerify(() => !contextMenu.menu.opened, 5000, "the menu closes")

            // An unknown origin names no path at all, so the line carries
            // no menu rather than a menu of dead actions.
            RootData.externalSourceSettings.clearBreadcrumb(fixture.trip.id)
            tryVerify(() => attachedHeaderId.rememberedSourcePath.length === 0,
                      5000, "clearing forgets the remembered source")
            verify(!contextMenu.visible, "an unknown origin offers no menu")

            mouseClick(sourceLabel, sourceLabel.width / 2,
                       sourceLabel.height / 2, Qt.RightButton)
            wait(100)
            verify(!contextMenu.menu.opened,
                   "right-clicking an unknown origin opens nothing")
        }

        // §16 B9d: both labels elide, and the two paths they name are
        // different — the copy inside the project and where it came from.
        // Hovering spells each one out in full. The attached text is the
        // gate; the hover timing itself is Qt's.
        function test_bothPathsCarryFullPathTooltips() {
            const fixture = attachFixtureTrip("panel-path-tooltips")
            rootId.trip = fixture.trip

            const entryFilePath = stageEntryFilePath(fixture)
            verify(entryFilePath.length > 0,
                   "the in-project copy resolves to a real path to name")

            const fileLabel = findChild(attachedHeaderId, "attachedFileLabel")
            verify(fileLabel !== null, "attachedFileLabel must exist")
            verify(fileLabel.text.indexOf("/") < 0,
                   "the row shows a bare file name; got: " + fileLabel.text)
            compare(fileLabel.QC.ToolTip.text, entryFilePath,
                    "the file name names the whole project-copy path on hover")

            const sourceLabel = findChild(attachedHeaderId, "sourceModeLabel")
            verify(sourceLabel !== null, "sourceModeLabel must exist")
            tryVerify(() => attachedHeaderId.rememberedSourcePath.length > 0,
                      5000, "the attach remembered where the copy came from")
            compare(sourceLabel.QC.ToolTip.text,
                    attachedHeaderId.rememberedSourcePath,
                    "the provenance line names the whole source path on hover")
            verify(sourceLabel.QC.ToolTip.text !== fileLabel.QC.ToolTip.text,
                   "the two tooltips name two different files")

            // An unknown origin names no path, so it offers no tooltip.
            RootData.externalSourceSettings.clearBreadcrumb(fixture.trip.id)
            tryVerify(() => attachedHeaderId.rememberedSourcePath.length === 0,
                      5000, "clearing forgets the remembered source")
            compare(sourceLabel.QC.ToolTip.text, "",
                    "an unknown origin carries no tooltip")
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

        // plans/EXTERNAL_FILE_LIVE_LINK_RETIREMENT.html §7 question 2: the
        // dialog states plainly what Replace does to the in-project copies,
        // because no baseline exists to warn about local edits specifically.
        function test_replaceDialogStatesWhatItReplaces() {
            const fixture = attachFixtureTrip("panel-replace-statement")
            rootId.trip = fixture.trip

            replaceDialogId.open()

            const explainer = findChild(replaceDialogId, "replaceExplainerText")
            verify(explainer !== null, "replaceExplainerText must exist")
            tryVerify(() => explainer.visible, 5000, "the statement is on screen")
            verify(explainer.text.length > 0, "the statement carries copy")
            verify(explainer.text.indexOf("including any edits made to them") >= 0,
                   "the statement says the in-project copies are replaced; got: "
                   + explainer.text)

            const cancelButton = findChild(replaceDialogId, "replaceCancelButton")
            verify(cancelButton !== null, "replaceCancelButton must exist")
            cancelButton.clicked()
            tryVerify(() => !explainer.visible, 5000, "the dialog closes on cancel")
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
            const listView = stationsListViewWithRows("panel-stations")

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

        function test_stationsListRowsAreCompact() {
            const listView = stationsListViewWithRows("panel-station-rows")

            //The bound is the component's own row height: one body-font
            //line plus a delegate padding band above and below.
            const bound = Math.ceil(fontMetricsId.height) + 2 * Theme.delegatePadding
            compare(stationsListId.rowHeight, bound,
                    "rowHeight is a body-text line plus two padding bands")
            verify(listView.itemAtIndex(0).height <= bound,
                   "a station row stays within the tightened bound; got: "
                   + listView.itemAtIndex(0).height + " > " + bound)
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
            const dateItem = findChild(tripMetadataId, "tripMetadataDate")
            verify(dateItem !== null, "tripMetadataDate must exist")
            compare(dateItem.text, "2024-06-01", "the date renders as the file has it")
            trip.date = new Date(2025, 2, 4)
            tryVerify(() => dateSpyId.count === 1, 1000,
                      "the trip's date change reaches the panel")
            tryCompare(dateItem, "text", "2025-03-04")

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

        // §16 B2b: the file owns an externally-backed trip's team, so the
        // panel's team table drops the "+". A native TeamTable keeps it.
        function test_externalTeamTableOffersNoAddButton() {
            const externalTeam = findChild(tripMetadataId, "tripMetadataTeam")
            verify(externalTeam !== null, "tripMetadataTeam must exist")

            const externalAdd = findChild(externalTeam, "sectionHeaderAddButton")
            verify(externalAdd !== null, "the Team header's add button must exist")
            tryVerify(() => !externalAdd.visible, 1000,
                      "an externally-backed team offers no +")

            const nativeAdd = findChild(nativeTeamTableId, "sectionHeaderAddButton")
            verify(nativeAdd !== null, "the native Team header's add button must exist")
            tryVerify(() => nativeAdd.visible, 1000,
                      "a native team keeps its +")
        }

        // Every descendant of item carrying objectName, in declaration order.
        function collectByObjectName(item, objectNameToFind) {
            let found = []
            for(let i = 0; i < item.children.length; i++) {
                const child = item.children[i]
                if(child.objectName === objectNameToFind) {
                    found.push(child)
                }
                found = found.concat(collectByObjectName(child, objectNameToFind))
            }
            return found
        }

        // The components column is taller than the window, and a click only
        // lands on an item the window actually shows.
        function scrollIntoView(item) {
            const position = item.mapToItem(rootId, 0, 0)
            componentColumnId.y -= position.y - rootId.height * 0.5
            waitForRendering(rootId)
        }

        // Fills a team with one member who has one role.
        function seedTeam(team, memberName) {
            while(team.rowCount() > 0) {
                team.removeTeamMember(0)
            }
            team.addTeamMember()
            team.setData(0, Team.NameRole, memberName)
            team.setData(0, Team.JobsRole, ["Sketch"])
        }

        // Selects a team table's first row and returns the parts that editing
        // turns on or off — selecting is what brings them out at all.
        function selectedFirstTeamRowParts(teamTable) {
            const teamList = findChild(teamTable, "teamList")
            verify(teamList !== null, "teamList must exist")
            tryVerify(() => teamList.itemAtIndex(0) !== null, 5000,
                      "the first team row must materialize")

            const row = teamList.itemAtIndex(0)
            scrollIntoView(row)
            mouseClick(row)
            tryVerify(() => teamList.currentIndex === 0, 1000,
                      "clicking a row selects it")

            const deleteButton = findChild(teamTable, "deletePersonButton.0")
            verify(deleteButton !== null, "deletePersonButton.0 must exist")

            const addJobButton = findChild(teamTable, "addJobButton.0")
            verify(addJobButton !== null, "addJobButton.0 must exist")

            const fields = collectByObjectName(row, "coreTextInput")
            compare(fields.length, 2, "the row shows a name and one role")

            return {
                "row": row,
                "deleteButton": deleteButton,
                "addJobButton": addJobButton,
                "nameField": fields[0],
                "roleField": fields[1],
                "roleChip": fields[1].parent
            }
        }

        // §16 B10: the survey file owns an externally-backed trip's date and
        // team, so the panel presents both and edits neither.
        function test_externalTripMetadataPresentsWithoutEditing() {
            RootData.region.addCave()
            const cave = RootData.region.cave(0)
            cave.addTrip()
            const trip = cave.trip(0)
            trip.date = new Date(2024, 5, 1)
            seedTeam(trip.team, "Alice")
            rootId.trip = trip

            const dateItem = findChild(tripMetadataId, "tripMetadataDate")
            verify(dateItem !== null, "tripMetadataDate must exist")
            compare(dateItem.text, "2024-06-01", "the date reads as the file has it")
            verify(findChild(dateItem.parent, "coreTextInput") === null,
                   "the date row holds a label, not a text field")
            scrollIntoView(dateItem)
            mouseDoubleClickSequence(dateItem)
            verify(rootId.shadowEditor.coreClickInput === null,
                   "double-clicking the date opens no editor")

            const externalTeam = findChild(tripMetadataId, "tripMetadataTeam")
            verify(externalTeam !== null, "tripMetadataTeam must exist")
            const parts = selectedFirstTeamRowParts(externalTeam)
            verify(!parts.deleteButton.visible, "a selected row offers no delete")
            verify(!parts.addJobButton.visible, "a selected row offers no role to add")

            mouseDoubleClickSequence(parts.row)
            verify(rootId.shadowEditor.coreClickInput === null,
                   "double-clicking a team name opens no editor")

            scrollIntoView(parts.roleChip)
            mouseDoubleClickSequence(parts.roleChip)
            verify(rootId.shadowEditor.coreClickInput === null,
                   "double-clicking a role opens no editor")

            // A role chip still takes focus, so the keyboard is a second way
            // in and Delete has to be refused as well.
            tryVerify(() => parts.roleChip.activeFocus, 1000,
                      "clicking a role focuses its chip")
            keyClick(Qt.Key_Delete)
            compare(trip.team.data(trip.team.index(0, 0), Team.JobsRole).length, 1,
                    "pressing Delete on a role leaves the file's team alone")

            compare(trip.team.data(trip.team.index(0, 0), Team.NameRole), "Alice",
                    "the team survives every double-click intact")
        }

        // The same table at its defaults still edits, which is what keeps the
        // read-only switch a property of the panel and not of TeamTable.
        function test_nativeTeamTableStillEdits() {
            seedTeam(nativeTeamModelId, "Bob")

            const parts = selectedFirstTeamRowParts(nativeTeamTableId)
            tryVerify(() => parts.deleteButton.visible, 1000,
                      "a selected native row offers delete")
            verify(parts.addJobButton.visible, "a selected native row offers a role to add")

            mouseDoubleClickSequence(parts.row)
            compare(rootId.shadowEditor.coreClickInput, parts.nameField,
                    "double-clicking a native team name opens its editor")
            rootId.closeAnyOpenEditor()

            scrollIntoView(parts.roleChip)
            mouseDoubleClickSequence(parts.roleChip)
            compare(rootId.shadowEditor.coreClickInput, parts.roleField,
                    "double-clicking a native role opens its editor")

            rootId.closeAnyOpenEditor()

            mouseClick(parts.roleChip)
            tryVerify(() => parts.roleChip.activeFocus, 1000,
                      "clicking a native role focuses its chip")
            keyClick(Qt.Key_Delete)
            tryVerify(() => nativeTeamModelId.data(
                          nativeTeamModelId.index(0, 0), Team.JobsRole).length === 0,
                      1000, "Delete drops a native role")
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

            // The card the notice banners share: bold title at the top of the
            // section margin, body under it, and a height that holds both
            // margins.
            const title = findChild(floatingBannerId, "floatingSurveyTitle")
            verify(title !== null, "floatingSurveyTitle must render")
            verify(title.font.bold, "the title is the card's bold line")
            tryVerify(() => title.mapToItem(floatingBannerId, 0, 0).y === Theme.sectionSpacing,
                      1000, "the title sits at the card's section margin")
            verify(detail.mapToItem(floatingBannerId, 0, 0).y
                   > title.mapToItem(floatingBannerId, 0, 0).y,
                   "the body lands under the title")
            verify(floatingBannerId.implicitHeight
                   >= stations.mapToItem(floatingBannerId, 0, 0).y + stations.height
                      + Theme.sectionSpacing,
                   "the card holds its content plus the bottom margin")

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
            // panel's way out of that state — Replace — sits below this
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
