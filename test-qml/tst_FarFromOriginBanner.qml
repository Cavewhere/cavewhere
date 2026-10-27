import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// MP.4 of plans/SURVEY_TREE_PLAN.html: a project with no geographic
// coordinate system whose survey solves far from the origin raises a banner
// in the window's banner strip. "Show it" frames the survey in the 3D view,
// and "Fix Stations…" opens the page where a system is chosen. A file of
// bare fixes is placed at the origin (FX.24), so it raises none.
MainWindowTest {
    id: rootId

    ExternalCenterlineTestCase {
        name: "FarFromOriginBanner"
        when: windowShown

        // far_traverse.svx fixes nothing and runs one 120 km leg east from
        // the origin, so the framed survey's center is 60 km east of it. The
        // camera's center lands on the framed box's center.
        readonly property real surveyCenterX: 60000
        readonly property real surveyCenterY: 0
        readonly property real centerMarginMeters: 50
        // A survey whose framed center sits this close to the origin is in
        // plain view.
        readonly property real nearOriginMeters: 100
        readonly property int attachWaitMs: 15000

        LinkGenerator {
            id: linkGeneratorId
            pageSelectionModel: RootData.pageSelectionModel
        }

        function init() {
            RootData.futureManagerModel.waitForFinished()
            RootData.newProject()
            RootData.futureManagerModel.waitForFinished()
            RootData.pageSelectionModel.currentPageAddress = "View"
        }

        function cleanup() {
            RootData.newProject()
        }

        function bannerArea() {
            verify(rootId.windowOverlay !== null, "the window has an overlay")
            return rootId.windowOverlay.bannerArea
        }

        // The banner is found where its registration put it: in the strip.
        function banner() {
            const item = findChild(bannerArea(), "farFromOriginBanner")
            verify(item !== null, "the far-from-origin banner registered into the banner strip")
            return item
        }

        function findInBanner(objectName) {
            const item = findChild(banner(), objectName)
            verify(item !== null, objectName + " must exist")
            return item
        }

        function attachFarTraverse(projectBaseName) {
            const sourceRoot = makeSavedCaveAttach(
                                 projectBaseName,
                                 "external-centerlines/survex_bare_fix_origin/far_traverse.svx")
            verify(!RootData.region.geoReference.hasCoordinateSystem,
                   "the project has no geographic coordinate system")
            return sourceRoot
        }

        function turnTableInteraction() {
            const interaction = ObjectFinder.findObjectByChain(
                                  rootId.mainWindow,
                                  "rootId->viewPage->SplitView->renderer->turnTableInteraction")
            verify(interaction !== null, "the 3D view's turntable must exist")
            return interaction
        }

        function test_bannerShowsForAFarSurveyWithNoSystem() {
            attachFarTraverse("far-from-origin-shows")

            tryVerify(() => banner().visible, attachWaitMs,
                      "the banner shows for a survey 60 km out with no system")
            const message = findInBanner("farFromOriginMessage")
            verify(message.text.indexOf("No geographic coordinate system") >= 0,
                   "the message names the problem; got: " + message.text)
            verify(/60\s*km/.test(message.text),
                   "the message says how far out the survey sits; got: " + message.text)
        }

        function test_showItFramesTheSurvey() {
            attachFarTraverse("far-from-origin-show-it")
            tryVerify(() => banner().visible, attachWaitMs, "the banner is up")

            RootData.pageSelectionModel.currentPageAddress = "Source/Data"
            waitForRendering(rootId)
            mouseClick(findInBanner("farFromOriginShowButton"))

            tryCompare(RootData.pageSelectionModel, "currentPageAddress", "View")
            const interaction = turnTableInteraction()
            tryVerify(() => Math.abs(interaction.center.x - surveyCenterX) < centerMarginMeters
                      && Math.abs(interaction.center.y - surveyCenterY) < centerMarginMeters,
                      attachWaitMs,
                      "the view centers on the survey; center is "
                      + interaction.center.x + ", " + interaction.center.y)
        }

        function test_fixStationsOpensTheProjectsFixStations() {
            attachFarTraverse("far-from-origin-fix-stations")
            tryVerify(() => banner().visible, attachWaitMs, "the banner is up")
            waitForRendering(rootId)

            mouseClick(findInBanner("farFromOriginFixStationsButton"))

            tryCompare(RootData.pageSelectionModel, "currentPageAddress",
                       linkGeneratorId.fixStationsLink(RootData.region.rootNode))
        }

        function test_aBareFixFileSitsAtTheOriginWithNoBanner() {
            makeSavedCaveAttach("far-from-origin-bare-fix",
                                "external-centerlines/survex_bare_fix/bare_fix.svx")

            // bare_fix.svx fixes A at 500000 3000000 with no system: the
            // project goes on the local grid, with A at the origin.
            tryCompare(RootData.region.geoReference, "state", GeoReference.LocalGrid, attachWaitMs)
            RootData.futureManagerModel.waitForFinished()
            verify(!RootData.region.geoReference.hasCoordinateSystem,
                   "the local grid has no geographic coordinate system")
            verify(RootData.region.farFromOriginNotice.distanceFromOrigin < nearOriginMeters,
                   "the survey sits at the origin; it is "
                   + RootData.region.farFromOriginNotice.distanceFromOrigin + " m out")
            verify(!RootData.region.farFromOriginNotice.shown, "nothing sits far out")
            verify(!banner().visible, "no banner for a survey at the origin")
        }

        function test_dismissHidesTheBannerUntilTheSurveyMovesAgain() {
            attachFarTraverse("far-from-origin-dismiss")
            tryVerify(() => banner().visible, attachWaitMs, "the banner is up")
            waitForRendering(rootId)

            mouseClick(findInBanner("farFromOriginDismissButton"))
            tryVerify(() => !banner().visible, attachWaitMs, "✕ hides the banner")
            verify(RootData.region.farFromOriginNotice.shown,
                   "the survey still sits far out; only the banner is hidden")

            RootData.newProject()
            RootData.futureManagerModel.waitForFinished()
            tryVerify(() => !RootData.region.farFromOriginNotice.shown, attachWaitMs,
                      "a new project has nothing far out")

            attachFarTraverse("far-from-origin-dismiss-again")
            tryVerify(() => banner().visible, attachWaitMs,
                      "the banner comes back when the survey lands far out again")
        }
    }
}
