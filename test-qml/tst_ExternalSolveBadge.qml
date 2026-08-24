import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    ExternalCenterlineTestCase {
        name: "ExternalSolveBadge"
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

        function gotoDataMainPage() {
            RootData.pageSelectionModel.currentPageAddress = "Source/Data"
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "dataMainPage",
                      5000, "should land on dataMainPage")
            const page = RootData.pageView.currentPageItem
            waitForRendering(page)
            return page
        }

        // The badge on the cave row at `index`, looked up under tryVerify
        // because the delegate may still be laying out on the first frame.
        function badgeForCaveRow(page, index) {
            let badge = null
            tryVerify(() => {
                          const delegate = findChild(page, "caveDelegate" + index)
                          badge = delegate === null
                                  ? null : findChild(delegate, "externalSolveBadge")
                          return badge !== null
                      }, 5000, "cave row " + index + " must carry an external solve badge")
            return badge
        }

        // Rewrites the cave's in-project copy into an *include that reaches
        // outside the project's data root — the containment check drops the
        // owner from the solve and the model row says why.
        function breakCaveCopy(cave) {
            const copyPath = TestHelper.externalCenterlineCaveCopyPath(
                RootData.project, cave, "survex_simple.svx")
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
        }

        function test_caveContainmentErrorShowsBadgeAndNavigates() {
            const cave = makeSavedCaveAttach(
                "solve-badge-error", "external-centerlines/survex_simple.svx")
            breakCaveCopy(cave)

            const page = gotoDataMainPage()
            const badge = badgeForCaveRow(page, 0)

            tryVerify(() => badge.visible && badge.hasError, 10000,
                      "the broken cave's row must show the error badge; reason: "
                      + badge.reason)
            compare(badge.color, Theme.danger, "an error badge is danger colored")
            verify(badge.reason.indexOf("outside.svx") >= 0,
                   "the badge's reason names the escaping file; got: " + badge.reason)

            mouseClick(badge)
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "cavernOutputPage",
                      5000, "clicking the badge must land on cavernOutputPage")
        }

        function test_cleanOwnerShowsNoBadge() {
            makeSavedCaveAttach("solve-badge-clean",
                                "external-centerlines/survex_simple.svx")
            RootData.region.addCave()
            RootData.region.cave(1).name = "NativeCave"

            const page = gotoDataMainPage()

            const attachedBadge = badgeForCaveRow(page, 0)
            verify(!attachedBadge.visible,
                   "a cleanly attached cave shows no badge; reason: "
                   + attachedBadge.reason)

            const nativeBadge = badgeForCaveRow(page, 1)
            verify(!nativeBadge.visible, "a native cave shows no badge")
        }
    }
}
