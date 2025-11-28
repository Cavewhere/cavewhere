import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    CWTestCase {
        name: "KeywordTabLoadProject"
        when: windowShown

        function test_addremovefirst() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwProject/Phake Cave 3000.cw");

            // Ensure we are on the View page
            RootData.pageSelectionModel.gotoPageByName(null, "View");

            let layersTab = findChild(rootId.mainWindow, "layersTabButton");
            verify(layersTab !== null);

            mouseClick(layersTab);

            wait(100);

            console.log("------ About to Add -------")

            let addButton0_0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->addButton0_0")
            mouseClick(addButton0_0_obj1)

            console.log("------ Added -------")

            wait(100);

            console.log("------ About to remove -------")

            let removeButton0_0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->removeButton0_0")
            mouseClick(removeButton0_0_obj1)

            console.log("------ Removed -------")

            wait(100)

            console.log("------ About to add 2 -------")

            //Add it back again
            addButton0_0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->addButton0_0")
            mouseClick(addButton0_0_obj1)

            console.log("------ Added 2 -------")

        }

        function test_addRemoveSimple() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwProject/Phake Cave 3000.cw");

            // Ensure we are on the View page
            RootData.pageSelectionModel.gotoPageByName(null, "View");

            let layersTab = findChild(rootId.mainWindow, "layersTabButton");
            verify(layersTab !== null);

            mouseClick(layersTab);

            wait(150);

                    let scrollbar = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->groupScrollBar_0")
                    let scrollbarAtEnd = (scrollbar) => {
                        return Math.abs(scrollbar.position - (1.0 - scrollbar.size)) < 0.0001
                    }

            let addButton0_0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->addButton0_0")
            mouseClick(addButton0_0_obj1)

            tryVerify( ()=> { return scrollbarAtEnd(scrollbar); });

            let removeButton0_0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->removeButton0_1")
            mouseClick(removeButton0_0_obj1)

                    let listViewRow0 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0")
                    let groupListView = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView")
                    tryVerify( () => { return listViewRow0.count === 1; })
                    tryVerify( () => { return groupListView.count === 1; })
        }

        function test_addremovelast() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwProject/Phake Cave 3000.cw");

            // Ensure we are on the View page
            RootData.pageSelectionModel.gotoPageByName(null, "View");

            let layersTab = findChild(rootId.mainWindow, "layersTabButton");
            verify(layersTab !== null);

            mouseClick(layersTab);

            wait(100);

            console.log("------ About to Add -------")

            let addButton0_0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->addButton0_0")
            mouseClick(addButton0_0_obj1)

            console.log("------ Added -------")

            wait(100);

            console.log("------ About to remove -------")

            let removeButton0_0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->removeButton0_1")
            mouseClick(removeButton0_0_obj1)

            console.log("------ Removed -------")

            wait(100)

            console.log("------ About to add 2 -------")

            //Add it back again
            addButton0_0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->addButton0_0")
            mouseClick(addButton0_0_obj1)

            console.log("------ Added 2 -------")
        }

        function test_add3removelast() {
                    TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwProject/Phake Cave 3000.cw");

                    // Ensure we are on the View page
                    RootData.pageSelectionModel.gotoPageByName(null, "View");

                    let layersTab = findChild(rootId.mainWindow, "layersTabButton");
                    verify(layersTab !== null);

                    mouseClick(layersTab);

                    wait(100);

                    let scrollbar = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->groupScrollBar_0")
                    let scrollbarAtEnd = (scrollbar) => {
                        return Math.abs(scrollbar.position - (1.0 - scrollbar.size)) < 0.0001
                    }

                    let addButton0_0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->addButton0_0")
                    mouseClick(addButton0_0_obj1)

                    tryVerify( ()=> { return scrollbarAtEnd(scrollbar); });

                    let addButton0_1_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->addButton0_1")
                    mouseClick(addButton0_1_obj1)

                    tryVerify( ()=> { return scrollbarAtEnd(scrollbar); });

                    let removeButton0_2_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->removeButton0_2")
                    mouseClick(removeButton0_2_obj1)
        }

        function test_orBoundariesRemoveThenAdd() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwProject/Phake Cave 3000.cw");

            // Ensure we are on the View page
            RootData.pageSelectionModel.gotoPageByName(null, "View");

            let layersTab = findChild(rootId.mainWindow, "layersTabButton");
            verify(layersTab !== null);

            mouseClick(layersTab);
            wait(150);

                    let scrollbar = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->vScrollBar")
                    let scrollbarAtEnd = (scrollbar) => {
                        return Math.abs(scrollbar.position - (1.0 - scrollbar.size)) < 0.0001
                    }

            // Click "also" twice to create two OR boundaries
            let alsoButton0 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->alsoButton");
            verify(alsoButton0);
            mouseClick(alsoButton0);
            mouseClick(alsoButton0);

            tryVerify( ()=> { return scrollbarAtEnd(scrollbar); });

            // Remove last row
            let removeButtonLast = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_2->removeButton2_0");
            verify(removeButtonLast);
            mouseClick(removeButtonLast);

            tryVerify( ()=> { return scrollbarAtEnd(scrollbar); });
            wait(300);

            // Remove last row again
            removeButtonLast = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_1->removeButton1_0");
            verify(removeButtonLast);
            mouseClick(removeButtonLast);

            // tryVerify( ()=> { return scrollbarAtEnd(scrollbar); });

            let groupListView = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView");
            verify(groupListView);
            tryVerify(() => groupListView.count === 1);

            let andListView0 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0");
            verify(andListView0);
            tryVerify(() => andListView0.count === 1);

            wait(300)

            // Press add button to append another row
            let addButton0_0 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->addButton0_0");
            verify(addButton0_0);
            mouseClick(addButton0_0);

                    tryVerify(() => groupListView.count === 1);
                    tryVerify(() => andListView0.count === 2);
        }

        function test_removeMiddleOrBoundary() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwProject/Phake Cave 3000.cw");

            RootData.pageSelectionModel.gotoPageByName(null, "View");
            let layersTab = findChild(rootId.mainWindow, "layersTabButton");
            verify(layersTab !== null);
            mouseClick(layersTab);
            wait(150);

                    let scrollbar = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->vScrollBar")
                    let scrollbarAtEnd = (scrollbar) => {
                        return Math.abs(scrollbar.position - (1.0 - scrollbar.size)) < 0.0001
                    }

            // Click "also" twice to create two OR boundaries
            let alsoButton0 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->alsoButton");
            verify(alsoButton0);
            mouseClick(alsoButton0);
            mouseClick(alsoButton0);

                    tryVerify( ()=> { return scrollbarAtEnd(scrollbar); });

                    let groupListView = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView");
                    groupListView.positionViewAtIndex(1, ListView.Beginning) //Jump to the first index


            // Remove the middle OR row via its remove button
            let removeMiddle = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_1->removeButton1_0");
            verify(removeMiddle);
            mouseClick(removeMiddle);

            groupListView = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView");
            verify(groupListView);
            tryVerify(() => groupListView.count === 2);

            let andListView0 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0");
            let andListView1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_1");
            verify(andListView0);
            verify(andListView1);
            tryVerify(() => andListView0.count === 1);
            tryVerify(() => andListView1.count === 1);
        }
    }
}
