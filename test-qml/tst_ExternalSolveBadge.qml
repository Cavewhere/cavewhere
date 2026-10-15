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

        readonly property int treeRowLimit: 32

        // The badge on `cave`'s row, looked up under tryVerify because the
        // delegate may still be laying out on the first frame. Rows are
        // searched by object: an attached cave's trip rows sit between it and
        // the next cave, so a cave's row index is not its region index.
        function badgeForCave(page, cave) {
            let badge = null
            tryVerify(() => {
                          for (let row = 0; row < treeRowLimit; row++) {
                              const delegate = findChild(page, "caveDelegate" + row)
                              if (delegate !== null && delegate.object === cave) {
                                  badge = findChild(delegate, "externalSolveBadge")
                                  return badge !== null
                              }
                          }
                          return false
                      }, 5000, cave.name + "'s row must carry an external solve badge")
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
            const badge = badgeForCave(page, cave)

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

        // The file solves cleanly, but its scan warns about the *declination
        // units: the row's one warning glyph is ErrorIconBar's, and the
        // badge stays for errors only.
        function test_readWarningsShowNoBadgeAndCountOnErrorIconBar() {
            const cave = makeSavedCaveAttach(
                "solve-badge-warning",
                "external-centerlines/survex_read_warning/read_warning.svx")
            const attachedModel = RootData.externalCenterlineManager.attachedCenterlinesModel
            tryVerify(() => attachedModel.rowCount() > 0, 10000,
                      "the attach must land a row in the attached-centerlines model")

            const page = gotoDataMainPage()
            const badge = badgeForCave(page, cave)
            verify(!badge.hasError, "the file solves cleanly; reason: " + badge.reason)
            verify(!badge.visible, "a file read with warnings shows no solve badge")

            tryVerify(() => cave.errorModel.warningCount === 1, 10000,
                      "the cave's error model counts the read warnings once; got: "
                      + cave.errorModel.warningCount)

            const row = badge.parent
            const iconBar = findChild(row, "errorIconBar")
            verify(iconBar !== null, "the cave's row must carry an ErrorIconBar")
            compare(iconBar.warningCount, 1, "ErrorIconBar counts the read warnings")
            const warningBadge = findChild(iconBar, "warningBadge")
            verify(warningBadge !== null && warningBadge.visible,
                   "ErrorIconBar shows its warning glyph")
        }

        function test_cleanOwnerShowsNoBadge() {
            const attached = makeSavedCaveAttach("solve-badge-clean",
                                                 "external-centerlines/survex_simple.svx")
            RootData.region.addCave()
            const native = RootData.region.cave(1)
            native.name = "NativeCave"

            const page = gotoDataMainPage()

            const attachedBadge = badgeForCave(page, attached)
            verify(!attachedBadge.visible,
                   "a cleanly attached cave shows no badge; reason: "
                   + attachedBadge.reason)

            const nativeBadge = badgeForCave(page, native)
            verify(!nativeBadge.visible, "a native cave shows no badge")
        }
    }
}
