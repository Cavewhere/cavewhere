import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId

    TestCase {
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

            let addButton0_0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->delegate_0->addButton")
            mouseClick(addButton0_0_obj1)

            console.log("------ Added -------")

            wait(100);

            console.log("------ About to remove -------")

            let removeButton0_0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->delegate_0->removeButton")
            mouseClick(removeButton0_0_obj1)

            console.log("------ Removed -------")

            wait(100)

            console.log("------ About to add 2 -------")

            //Add it back again
            addButton0_0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->delegate_0->addButton")
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

                    let scrollbar = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->groupScrollBar")
                    let scrollbarAtEnd = (scrollbar) => {
                        return Math.abs(scrollbar.position - (1.0 - scrollbar.size)) < 0.0001
                    }

            let addButton0_0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->delegate_0->addButton")
            mouseClick(addButton0_0_obj1)

            tryVerify( ()=> { return scrollbarAtEnd(scrollbar); });

            let removeButton0_0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->delegate_1->removeButton")
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

            let addButton0_0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->delegate_0->addButton")
            mouseClick(addButton0_0_obj1)

            console.log("------ Added -------")

            wait(100);

            console.log("------ About to remove -------")

            let removeButton0_0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->delegate_1->removeButton")
            mouseClick(removeButton0_0_obj1)

            console.log("------ Removed -------")

            wait(100)

            console.log("------ About to add 2 -------")

            //Add it back again
            addButton0_0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->delegate_0->addButton")
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

                    let scrollbar = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->groupScrollBar")
                    let scrollbarAtEnd = (scrollbar) => {
                        return Math.abs(scrollbar.position - (1.0 - scrollbar.size)) < 0.0001
                    }

                    let addButton0_0_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->delegate_0->addButton")
                    mouseClick(addButton0_0_obj1)

                    tryVerify( ()=> { return scrollbarAtEnd(scrollbar); });

                    let addButton0_1_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->delegate_1->addButton")
                    mouseClick(addButton0_1_obj1)

                    tryVerify( ()=> { return scrollbarAtEnd(scrollbar); });

                    let removeButton0_2_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->delegate_2->removeButton")
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
            let removeButtonLast = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_2->delegate_0->removeButton");
            verify(removeButtonLast);
            mouseClick(removeButtonLast);

            tryVerify( ()=> { return scrollbarAtEnd(scrollbar); });
            wait(300);

            // Remove last row again
            removeButtonLast = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_1->delegate_0->removeButton");
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
            let addButton0_0 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->delegate_0->addButton");
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
            let removeMiddle = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_1->delegate_0->removeButton");
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

        function test_objectCount() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwProject/Phake Cave 3000 2024.2.cw");

            RootData.pageSelectionModel.gotoPageByName(null, "View");
            let layersTab = findChild(rootId.mainWindow, "layersTabButton");
            verify(layersTab !== null);
            mouseClick(layersTab);

            wait(150)

            // Set key combo to "orientation" on the first AND row
            let keyCombo = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->delegate_0->keyCombo");
            verify(keyCombo);
            keyCombo.currentIndex = keyCombo.model.indexOf("Orientation");
            verify(keyCombo.currentIndex > 0)

            let delegate = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->delegate_0");
            verify(delegate);
            delegate.filterModelObjectRole.key = keyCombo.currentText

            // Add another AND row
            let addButton0_0 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->delegate_0->addButton");
            verify(addButton0_0);
            mouseClick(addButton0_0);

                    let scrollbar = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->groupScrollBar")
                    verify(scrollbar);
                    let scrollbarAtEnd = (scrollbar) => {
                        return Math.abs(scrollbar.position - (1.0 - scrollbar.size)) < 0.0001
                    }

                    tryVerify(() => { return scrollbarAtEnd(scrollbar); } )

            // Grab the first value row in the first list view
            let andListView0 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0");
            verify(andListView0);
            tryVerify(() => andListView0.count > 0);

                    // let scrollbar = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->groupScrollBar")
                    let scrollbarAtBeginning = (scrollbar) => {
                        return scrollbar.position < 0.01
                    }

            andListView0.positionViewAtBeginning();

                    tryVerify(() => { return scrollbarAtBeginning(scrollbar); } )


            let firstCheckbox = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->delegate_0->row1->checkbox");
            verify(firstCheckbox);

            // Capture objectCountRole from the second list view (row0)
            let row0 = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->delegate_1->row0");
            verify(row0);

            for(let i = 0; i < 5; i++) {
                        let originalText = row0.objectCountRole;
                        verify(originalText >= 1)

                        // Toggle off then on in the first list view
                        mouseClick(firstCheckbox);
                        verify(firstCheckbox.checked === false);

                        wait(10)

                        mouseClick(firstCheckbox);
                        verify(firstCheckbox.checked === true);


                        wait(100)

                        // The objectCountRole should be unchanged
                        let row0SecondAfter = ObjectFinder.findObjectByChain(mainWindow, "rootId->viewPage->RenderingView->renderingSidePanel->keyword->groupListView->andListView_0->delegate_1->row0");
                        verify(row0SecondAfter);
                        verify(row0SecondAfter === row0);
                        console.log("objectCountRole:" + row0SecondAfter.objectCountRole + " " + originalText)
                        compare(row0SecondAfter.objectCountRole, originalText);
            }
        }
    }
}
