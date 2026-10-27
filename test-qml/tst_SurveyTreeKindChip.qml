import QtQuick as QQ
import QtQuick.Controls as QC
import QtTest
import cavewherelib
import cw.TestLib

// The tree sits in a bare window on purpose: MainWindowTest's dialogs leave
// the process laying the lock emoji out at the caption's own line height, which
// hides the taller line box the lock gets in the running application.
QQ.Item {
    id: rootId

    width: 1000
    height: 400

    SurveyTreeView {
        id: treeId

        anchors.fill: parent
    }

    TestCase {
        name: "SurveyTreeKindChip"
        when: windowShown

        function init() {
            RootData.futureManagerModel.waitForFinished()
            RootData.newProject()
            RootData.futureManagerModel.waitForFinished()
        }

        function chipLabelWithText(item: QQ.Item, text: string) : QQ.Item {
            for (let i = 0; i < item.children.length; ++i) {
                const child = item.children[i]
                if (child instanceof QC.Label && child.text === text) {
                    return child
                }
                const found = chipLabelWithText(child, text)
                if (found !== null) {
                    return found
                }
            }
            return null
        }

        function centerYInRoot(item: QQ.Item) : real {
            return item.mapToItem(rootId, 0, item.height / 2).y
        }

        // A sourced chip carries the lock ahead of its text. The lock's
        // emoji line box is taller than the caption's, and the text must
        // still sit on the row name's centerline.
        function test_sourcedKindChipTextCentersOnRowName() {
            RootData.account.name = "Chip Test"
            RootData.account.email = "chip.test@example.com"
            const tmpPath = RootData.urlToLocal(TestHelper.tempDirectoryUrl())
            verify(RootData.project.saveAs(tmpPath + "/chipAlignTest.cwproj"), "saveAs should succeed")
            TestHelper.waitForProjectSaveToFinish(RootData.project)

            const model = RootData.externalCenterlineManager.attachedCenterlinesModel
            const rowsBefore = model.rowCount()
            // mixed.svx's block cave nests inner, so cave stays a node row.
            RootData.attachSource(RootData.region.rootNode,
                                  TestHelper.testcasesDatasetPath("external-centerlines/survex_block_rows/mixed.svx"))
            tryVerify(() => model.rowCount() > rowsBefore, 10000, "the attach must land")
            RootData.futureManagerModel.waitForFinished()
            const cave = RootData.region.cave(0)
            tryVerify(() => cave.childNodeCount > 0, 10000, "the attached file holds its block")

            let firstRow = null
            tryVerify(() => {
                          firstRow = findChild(treeId, "caveDelegate0")
                          return firstRow !== null
                      }, 5000, "the tree shows the attached file's row")
            treeId.expandAll()

            const blockRow = 1
            let chip = null
            tryVerify(() => {
                          const kindCell = findChild(treeId, "caveKind" + blockRow)
                          chip = kindCell === null ? null : findChild(kindCell, "kindChip")
                          return chip !== null && chip.text === "Survex block"
                      }, 5000, "the block row shows a Survex block chip")
            verify(chip.sourced, "an attached block's chip is sourced")
            const name = findChild(findChild(treeId, "caveDelegate" + blockRow), "caveLink")
            verify(name !== null, "the block row shows its name")
            waitForRendering(treeId)

            const chipText = chipLabelWithText(chip, "Survex block")
            const lock = chipLabelWithText(chip, "🔒")
            verify(chipText !== null, "the chip draws its text in a label")
            verify(lock !== null && lock.visible, "a sourced chip draws its lock")

            const textCenter = centerYInRoot(chipText)
            const nameCenter = centerYInRoot(name)
            verify(Math.abs(textCenter - nameCenter) <= 1,
                   "the chip text center " + textCenter + " sits on the name center " + nameCenter)
            const lockCenter = centerYInRoot(lock)
            verify(Math.abs(lockCenter - textCenter) <= 1,
                   "the lock center " + lockCenter + " sits on the chip text center " + textCenter)
        }
    }
}
