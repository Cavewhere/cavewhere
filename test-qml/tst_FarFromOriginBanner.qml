import QtQuick as QQ
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

// MP.4 of plans/SURVEY_TREE_PLAN.html: a project with no geographic
// coordinate system whose survey solves far from the origin raises a banner
// in the window's banner strip. "Show it" frames the survey in the 3D view,
// "Fix Stations…" opens the page where a system is chosen, and the banner
// clears once one is.
MainWindowTest {
    id: rootId

    ExternalCenterlineTestCase {
        name: "FarFromOriginBanner"
        when: windowShown

        // bare_fix.svx fixes A at 500000 3000000 and runs 10 m east, then
        // 10 m north, so the framed survey's center is 5 m in from A on each
        // axis. Solved positions are float32 (a quarter meter per step out
        // there), and the camera's center lands on the framed box's center.
        readonly property real surveyCenterX: 500005
        readonly property real surveyCenterY: 3000005
        readonly property real centerMarginMeters: 50
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

        function attachBareFix(projectBaseName) {
            const sourceRoot = makeSavedCaveAttach(
                                 projectBaseName,
                                 "external-centerlines/survex_bare_fix/bare_fix.svx")
            verify(!RootData.region.geoReference.hasCoordinateSystem,
                   "the project has no geographic coordinate system")
            return sourceRoot
        }

        function chooseFixSystem(owner, cs) {
            const centerline = owner.externalCenterline
            centerline.fixCoordinateSystem = cs
            owner.externalCenterline = centerline
        }

        function turnTableInteraction() {
            const interaction = ObjectFinder.findObjectByChain(
                                  rootId.mainWindow,
                                  "rootId->viewPage->SplitView->renderer->turnTableInteraction")
            verify(interaction !== null, "the 3D view's turntable must exist")
            return interaction
        }

        function test_bannerShowsForAFarSurveyWithNoSystem() {
            attachBareFix("far-from-origin-shows")

            tryVerify(() => banner().visible, attachWaitMs,
                      "the banner shows for a survey 3,000 km out with no system")
            const message = findInBanner("farFromOriginMessage")
            verify(message.text.indexOf("No geographic coordinate system") >= 0,
                   "the message names the problem; got: " + message.text)
            verify(/3\D?041\s*km/.test(message.text),
                   "the message says how far out the survey sits; got: " + message.text)
        }

        function test_showItFramesTheSurvey() {
            attachBareFix("far-from-origin-show-it")
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
            attachBareFix("far-from-origin-fix-stations")
            tryVerify(() => banner().visible, attachWaitMs, "the banner is up")
            waitForRendering(rootId)

            mouseClick(findInBanner("farFromOriginFixStationsButton"))

            tryCompare(RootData.pageSelectionModel, "currentPageAddress",
                       linkGeneratorId.fixStationsLink(RootData.region.rootNode))
        }

        function test_choosingASystemClearsTheBanner() {
            const sourceRoot = attachBareFix("far-from-origin-choose")
            tryVerify(() => banner().visible, attachWaitMs, "the banner is up")

            chooseFixSystem(sourceRoot, "UTM48N")

            tryVerify(() => RootData.region.geoReference.hasCoordinateSystem, attachWaitMs,
                      "choosing a system gives the project a frame")
            tryVerify(() => !banner().visible, attachWaitMs,
                      "the banner clears once a system is chosen")
        }

        function test_dismissHidesTheBannerUntilTheSurveyMovesAgain() {
            attachBareFix("far-from-origin-dismiss")
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

            attachBareFix("far-from-origin-dismiss-again")
            tryVerify(() => banner().visible, attachWaitMs,
                      "the banner comes back when the survey lands far out again")
        }
    }
}
