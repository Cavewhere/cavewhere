import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// N5 of plans/EXTERNAL_SOURCE_CHANGE_NOTIFY.html: the app-scope banner in
// the window's AppOverlay, its [Update all] (one shared overwrite per
// Changed owner, missing sources skipped), the [Show] review list, and
// per-fingerprint dismissal.
MainWindowTest {
    id: rootId

    ExternalCenterlineTestCase {
        name: "ExternalSourceChangeBanner"
        when: windowShown

        SignalSpy {
            id: attachCompletedSpyId
            target: RootData.externalCenterlineManager
            signalName: "attachCompleted"
        }

        // A banner that says nothing at all, for asserting that the overlay
        // hosts whatever registers rather than this one feature.
        QQ.Component {
            id: dummyBannerComponent

            QQ.Item {
                height: 10
            }
        }

        function init() {
            RootData.futureManagerModel.waitForFinished()
            RootData.newProject()
            RootData.futureManagerModel.waitForFinished()
            RootData.pageSelectionModel.currentPageAddress = "View"
            attachCompletedSpyId.clear()
        }

        function cleanup() {
            RootData.newProject()
        }

        function statusModel() {
            return RootData.externalCenterlineManager.sourceStatusModel
        }

        // A private copy of the fixture, so editing it disturbs no other
        // test and no file in the repository.
        function tempSource() {
            return TestHelper.copyToTempDir(
                TestHelper.testcasesDatasetPath(
                    "external-centerlines/survex_simple.svx"))
        }

        // Rewrites `source` with different contents, which is what the
        // fingerprint check needs to reach its hash verdict.
        function rewriteSource(source, datasetName) {
            verify(TestHelper.copyFile(
                       TestHelper.testcasesDatasetPath(
                           "external-centerlines/" + datasetName),
                       source),
                   "the source is rewritten with " + datasetName)
        }

        function addTripToFirstCave() {
            const cave = RootData.region.cave(0)
            cave.addTrip()
            return cave.trip(cave.rowCount() - 1)
        }

        // `count` attached trips in one cave, each copied from a source of
        // its own. Returns parallel arrays of trips and source paths.
        function attachTrips(projectBaseName, count) {
            const trips = []
            const sources = []
            for (let i = 0; i < count; ++i) {
                const trip = i === 0 ? makeSavedTrip(projectBaseName)
                                     : addTripToFirstCave()
                const source = tempSource()
                attachSourceToTrip(trip, source)
                trips.push(trip)
                sources.push(source)
            }

            attachCompletedSpyId.clear()
            return { trips: trips, sources: sources }
        }

        // The banner strip the window's overlay offers every app-scope
        // banner. Nothing here names the source-change feature.
        function bannerArea() {
            verify(rootId.windowOverlay !== null, "the window has an overlay")
            return rootId.windowOverlay.bannerArea
        }

        // The source-change host is found where its registration put it —
        // the same way anything else would find it, with no back door.
        function sourceChangeHost() {
            const host = findChild(bannerArea(), "externalSourceChangeHost")
            verify(host !== null,
                   "the source-change host registered into the banner strip")
            return host
        }

        function findInHost(objectName) {
            const item = findChild(sourceChangeHost(), objectName)
            verify(item !== null, objectName + " must exist")
            return item
        }

        // §6 "fu-overlay-banner-abstraction": the overlay hosts banners
        // generically, and the source-change banner is simply the first
        // feature to register one.
        function test_bannersAreHostedByTheOverlayStrip() {
            const strip = bannerArea()

            verify(sourceChangeHost().parent === strip,
                   "the source-change host registered into the banner strip")

            const first = createTemporaryObject(dummyBannerComponent, rootId)
            const second = createTemporaryObject(dummyBannerComponent, rootId)
            rootId.windowOverlay.addBanner(first)
            rootId.windowOverlay.addBanner(second)
            waitForRendering(rootId)

            verify(first.parent === strip, "any banner may register")
            verify(second.parent === strip, "and so may a second one")
            compare(first.width, strip.width,
                    "the strip spans its banners across the window")
            verify(second.y > first.y,
                   "banners stack in registration order, the first on top")
        }

        function test_bannerAppearsWithTheChangedCount() {
            const host = sourceChangeHost()
            const trip = makeSavedTrip("banner-count")
            const source = tempSource()
            attachSourceToTrip(trip, source)
            verify(!host.hasUndismissedChange,
                   "a fresh copy matches its source, so nothing is said")

            rewriteSource(source, "survex_no_metadata.svx")

            tryVerify(() => host.hasUndismissedChange, 15000,
                      "the edited source raises the banner")
            compare(host.changedCount, 1, "one source changed")
            waitForRendering(rootId)

            const banner = findInHost("externalSourceChangeBanner")
            verify(banner.visible, "the banner shows itself")
            const message = findInHost("externalSourceChangeMessage")
            verify(message.text.indexOf("1 external source has changed") === 0,
                   "the count is in the text; got: " + message.text)
        }

        function test_updateAllUpdatesChangedOwnersAndSkipsAMissingSource() {
            const host = sourceChangeHost()
            const attached = attachTrips("banner-update-all", 3)
            const changedTrips = [attached.trips[0], attached.trips[1]]
            const missingTrip = attached.trips[2]

            rewriteSource(attached.sources[0], "survex_with_metadata.svx")
            rewriteSource(attached.sources[1], "survex_no_metadata.svx")
            TestHelper.removeFile(TestHelper.toLocalUrl(attached.sources[2]))

            for (const trip of changedTrips) {
                tryVerify(() => statusModel().statusFor(trip.id)
                                === ExternalSourceStatusModel.Changed, 15000,
                          "each edited source reads Changed")
            }
            tryVerify(() => statusModel().statusFor(missingTrip.id)
                            === ExternalSourceStatusModel.SourceMissing, 15000,
                      "the deleted source reads SourceMissing")
            tryCompare(host, "changedCount", 2, 15000,
                       "only the edited sources count")

            tryVerify(() => host.hasUndismissedChange, 15000, "the banner is up")
            waitForRendering(rootId)
            mouseClick(findInHost("externalSourceUpdateAllButton"))

            tryVerify(() => attachCompletedSpyId.count === 2, 30000,
                      "the shared overwrite path runs once per changed owner")

            const updatedOwners = []
            for (let i = 0; i < attachCompletedSpyId.count; ++i) {
                const report = attachCompletedSpyId.signalArguments[i][0]
                verify(report.success,
                       "each update succeeds; got: " + report.errorMessage)
                updatedOwners.push(report.ownerId.toString())
            }
            for (const trip of changedTrips) {
                verify(updatedOwners.indexOf(trip.id.toString()) >= 0,
                       "every changed owner was updated")
            }

            for (const trip of changedTrips) {
                tryVerify(() => statusModel().statusFor(trip.id)
                                === ExternalSourceStatusModel.UpToDate, 15000,
                          "each updated copy matches its source again")
            }
            compare(statusModel().statusFor(missingTrip.id),
                    ExternalSourceStatusModel.SourceMissing,
                    "the gone source is left exactly as it was")
            compare(attachCompletedSpyId.count, 2,
                    "the missing source starts no further operation")
            tryVerify(() => !host.hasUndismissedChange, 15000,
                      "with nothing changed the banner goes away")
        }

        function test_dismissHidesUntilTheSourceChangesAgain() {
            const host = sourceChangeHost()
            const trip = makeSavedTrip("banner-dismiss")
            const source = tempSource()
            attachSourceToTrip(trip, source)

            rewriteSource(source, "survex_no_metadata.svx")
            tryVerify(() => host.hasUndismissedChange, 15000, "the banner is up")
            waitForRendering(rootId)

            mouseClick(findInHost("externalSourceDismissButton"))
            verify(!host.hasUndismissedChange, "dismissing hides the banner")

            // The source is still changed, and re-asking the manager keeps
            // giving the same answer — the banner stays away for it.
            compare(statusModel().statusFor(trip.id),
                    ExternalSourceStatusModel.Changed,
                    "dismissing changes nothing about the source")
            host.refresh()
            verify(!host.hasUndismissedChange,
                   "a later sweep of the same edit says nothing again")

            rewriteSource(source, "survex_with_metadata.svx")
            tryVerify(() => host.hasUndismissedChange, 15000,
                      "a second edit brings the banner back")
            compare(host.changedCount, 1, "still one changed source")
        }

        function test_showListsEveryOwnerNeedingAttention() {
            const host = sourceChangeHost()
            const attached = attachTrips("banner-show-list", 2)

            rewriteSource(attached.sources[0], "survex_no_metadata.svx")
            TestHelper.removeFile(TestHelper.toLocalUrl(attached.sources[1]))

            tryVerify(() => host.hasUndismissedChange, 15000, "the banner is up")
            waitForRendering(rootId)
            mouseClick(findInHost("externalSourceShowButton"))
            verify(host.listShown, "Show opens the review list")
            waitForRendering(rootId)

            const rows = findInHost("externalSourceChangeRows")
            tryCompare(rows, "count", 2, 15000,
                       "one row per owner needing attention")

            let changedRows = 0
            let missingRows = 0
            for (let i = 0; i < rows.count; ++i) {
                const row = rows.itemAtIndex(i)
                verify(row !== null, "row " + i + " must be realized")
                const status = findChild(row, "externalSourceChangeRowStatus")
                const updateButton = findChild(row, "externalSourceChangeRowUpdateButton")
                const owner = findChild(row, "externalSourceChangeRowOwner")
                verify(owner.text.length > 0, "the row names its owner")
                if (status.text === "Changed") {
                    changedRows += 1
                    verify(updateButton.enabled, "a changed source can be updated")
                } else {
                    missingRows += 1
                    verify(!updateButton.enabled,
                           "a gone source has nothing to copy from")
                }
            }
            compare(changedRows, 1, "one changed row")
            compare(missingRows, 1, "one missing-source row")
        }

        // The owner a status row is about. A cave is reached by id like
        // every other owner, and the status model is where QML reads it.
        function statusRowOwnerId(rowIndex) {
            const model = statusModel()
            return model.data(model.index(rowIndex, 0),
                              ExternalSourceStatusModel.OwnerIdRole)
        }

        function test_caveRowUpdatesTheCave() {
            const host = sourceChangeHost()
            makeSavedTrip("banner-cave-row")

            const source = tempSource()
            makeAttachedCave("BannerCave", source)
            attachCompletedSpyId.clear()

            tryVerify(() => statusModel().rowCount() === 1, 15000,
                      "the cave is the only attached owner")
            const caveId = statusRowOwnerId(0)

            rewriteSource(source, "survex_no_metadata.svx")
            tryVerify(() => statusModel().statusFor(caveId)
                            === ExternalSourceStatusModel.Changed, 15000,
                      "the edited cave source reads Changed")
            tryVerify(() => host.hasUndismissedChange, 15000, "the banner is up")
            waitForRendering(rootId)

            mouseClick(findInHost("externalSourceShowButton"))
            waitForRendering(rootId)

            const rows = findInHost("externalSourceChangeRows")
            tryCompare(rows, "count", 1, 15000, "the cave is the only owner listed")
            const caveRow = rows.itemAtIndex(0)
            verify(caveRow !== null, "the cave row must be realized")
            compare(findChild(caveRow, "externalSourceChangeRowOwner").text,
                    "BannerCave", "a cave row is named by the cave alone")
            const updateButton = findChild(caveRow, "externalSourceChangeRowUpdateButton")
            verify(updateButton.enabled, "a changed cave source can be updated")

            mouseClick(updateButton)
            tryVerify(() => attachCompletedSpyId.count === 1, 30000,
                      "the row's Update runs the cave's copy")
            const report = attachCompletedSpyId.signalArguments[0][0]
            verify(report.success, "the cave update succeeds; got: " + report.errorMessage)
            compare(report.ownerId.toString(), caveId.toString(),
                    "the cave is the owner that was updated")

            tryVerify(() => statusModel().statusFor(caveId)
                            === ExternalSourceStatusModel.UpToDate, 15000,
                      "the cave's copy matches its source again")
        }

        function test_rowUpdateUpdatesJustThatOwner() {
            const host = sourceChangeHost()
            const attached = attachTrips("banner-row-update", 2)

            rewriteSource(attached.sources[0], "survex_no_metadata.svx")
            rewriteSource(attached.sources[1], "survex_with_metadata.svx")

            tryVerify(() => host.changedCount === 2, 15000,
                      "both sources read Changed")
            waitForRendering(rootId)
            mouseClick(findInHost("externalSourceShowButton"))
            waitForRendering(rootId)

            const rows = findInHost("externalSourceChangeRows")
            tryCompare(rows, "count", 2, 15000, "both owners are listed")
            const firstRow = rows.itemAtIndex(0)
            verify(firstRow !== null, "the first row must be realized")
            mouseClick(findChild(firstRow, "externalSourceChangeRowUpdateButton"))

            tryVerify(() => attachCompletedSpyId.count === 1, 20000,
                      "exactly the row's owner is updated")
            tryVerify(() => statusModel().changedCount === 1, 15000,
                      "the other source is left changed")
        }
    }
}
