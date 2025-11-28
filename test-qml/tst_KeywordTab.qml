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


    }
}
