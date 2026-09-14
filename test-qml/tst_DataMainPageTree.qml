import QtQuick as QQ
import QtQuick.Controls as QC
import QtQml.Models
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    // The smoke gate's bare TreeView. It lives in a Loader so it only exists
    // while the smoke test runs — the page tests below click through
    // mainWindow, which this would otherwise cover.
    QQ.Loader {
        id: smokeLoaderId
        active: false
        width: 400
        height: 300
        sourceComponent: smokeComponentId
    }

    QQ.Component {
        id: smokeComponentId

        QQ.TreeView {
            id: smokeTreeId
            objectName: "smokeTree"
            anchors.fill: parent
            clip: true

            model: SurveyTreeFilterModel {
                sourceModel: SurveyTreeModel {
                    region: RootData.region
                }
            }

            selectionModel: ItemSelectionModel {
                model: smokeTreeId.model
            }

            delegate: QQ.Rectangle {
                id: smokeDelegateId

                required property string name
                required property int depth

                implicitWidth: 200
                implicitHeight: 24
                color: Theme.surface

                QC.Label {
                    x: smokeDelegateId.depth * 10
                    text: smokeDelegateId.name
                }
            }
        }
    }

    TestCase {
        name: "DataMainPageTree"
        when: windowShown

        function init() {
            RootData.futureManagerModel.waitForFinished()
            RootData.newProject()
            RootData.futureManagerModel.waitForFinished()
        }

        function cleanup() {
            smokeLoaderId.active = false
            RootData.pageSelectionModel.currentPageAddress = "View"
            RootData.newProject()
        }

        function addCave(name, tripCount) {
            RootData.region.addCave()
            const cave = RootData.region.cave(RootData.region.rowCount() - 1)
            cave.name = name
            for(let i = 0; i < tripCount; i++) {
                cave.addTrip()
            }
            return cave
        }

        // Smoke gate: a stock TreeView over the new models draws rows offscreen
        // and its expand/collapse change the view's row count.
        function test_treeViewShowsAndExpandsRowsOffscreen() {
            addCave("Alpha Cave", 2)
            addCave("Beta Cave", 1)

            smokeLoaderId.active = true
            const treeView = smokeLoaderId.item
            verify(treeView !== null, "the smoke TreeView must load")
            waitForRendering(treeView)

            tryCompare(treeView, "rows", 2, 5000)

            treeView.expand(0)
            tryCompare(treeView, "rows", 4, 5000)

            treeView.collapse(0)
            tryCompare(treeView, "rows", 2, 5000)
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

        // The TreeView carries no objectName — a name between the page and a
        // row would break the chains the other Data page tests match — so the
        // view is reached the way its own delegates see it.
        function surveyTree(page) {
            let row = null
            tryVerify(() => {
                          row = findChild(page, "caveDelegate0")
                          return row !== null
                      }, 5000, "the Data page must show the first cave row")
            return row.treeView
        }

        // Every column title comes from the model's headerData through the
        // stock HorizontalHeaderView, which #502 broke over a list-shaped
        // model — this is the tree-shaped model's proof.
        function test_headerShowsEveryColumnTitle() {
            addCave("Alpha Cave", 1)

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 1, 5000)

            const titles = ["Name", "Kind", "Trips", "Length", "Depth", "Last survey", ""]
            for(let column = 0; column < titles.length; column++) {
                let cell = null
                tryVerify(() => {
                              cell = findChild(page, "surveyTreeHeaderCell" + column)
                              return cell !== null
                          }, 5000, "header cell " + column + " must exist")
                compare(cell.display, titles[column],
                        "header cell " + column + " must carry its own title")
            }
        }

        function test_caveRowsAreCollapsedAndReadCave() {
            addCave("Alpha Cave", 2)
            addCave("Beta Cave", 1)

            const page = gotoDataMainPage()
            const tree = surveyTree(page)

            //Two caves, no trip rows: a native cave starts collapsed.
            tryCompare(tree, "rows", 2, 5000)

            for(let row = 0; row < 2; row++) {
                let kindCell = null
                tryVerify(() => {
                              kindCell = findChild(page, "caveKind" + row)
                              return kindCell !== null
                          }, 5000, "row " + row + " must show a Kind cell")
                const chip = findChild(kindCell, "kindChip")
                verify(chip !== null, "row " + row + " must show a kind chip")
                compare(chip.text, "Cave")
            }

            verify(findChild(page, "caveDelegate0") !== null,
                   "the first cave row keeps the caveDelegate0 name")
            verify(findChild(page, "tripDelegate1") === null,
                   "a collapsed cave shows no trip rows")
        }

        function test_expandingACaveShowsItsTrips() {
            const cave = addCave("Alpha Cave", 2)
            cave.trip(0).name = "Trip A"
            cave.trip(1).name = "Trip B"
            addCave("Beta Cave", 1)

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 2, 5000)

            tree.expand(0)
            tryCompare(tree, "rows", 4, 5000)

            let tripRow = null
            tryVerify(() => {
                          tripRow = findChild(page, "tripDelegate1")
                          return tripRow !== null
                      }, 5000, "the first trip must take the row under its cave")
            const tripLink = findChild(tripRow, "tripLink")
            verify(tripLink !== null, "a trip row shows its name as a link")
            compare(tripLink.text, "Trip A")

            tree.collapse(0)
            tryCompare(tree, "rows", 2, 5000)
        }

        function test_clickingATripRowOpensTheTripPageAndBackKeepsExpansion() {
            const cave = addCave("Alpha Cave", 2)
            cave.trip(0).name = "Trip A"
            cave.trip(1).name = "Trip B"

            const page = gotoDataMainPage()
            const tree = surveyTree(page)
            tryCompare(tree, "rows", 1, 5000)

            tree.expand(0)
            tryCompare(tree, "rows", 3, 5000)

            let tripLink = null
            tryVerify(() => {
                          const tripRow = findChild(page, "tripDelegate2")
                          tripLink = tripRow === null ? null : findChild(tripRow, "tripLink")
                          return tripLink !== null
                      }, 5000, "the second trip row must show its link")
            compare(tripLink.text, "Trip B")

            mouseClick(tripLink)
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "tripPage",
                      5000, "clicking a trip row must land on the trip page")

            RootData.pageSelectionModel.back()
            tryVerify(() => RootData.pageView.currentPageItem !== null
                            && RootData.pageView.currentPageItem.objectName === "dataMainPage",
                      5000, "back must land on the Data page again")

            //The Data page is cached, so its tree keeps the rows it had open.
            const treeAgain = surveyTree(RootData.pageView.currentPageItem)
            tryCompare(treeAgain, "rows", 3, 5000)
            verify(treeAgain.isExpanded(0), "the cave must still be expanded")
        }
    }
}
