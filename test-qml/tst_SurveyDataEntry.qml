import QtQuick
import QtTest
import cavewherelib
import cw.TestLib
import QmlTestRecorder

MainWindowTest {
    id: rootId


    TestCase {
        name: "SurveyDataEntry"
        when: windowShown

        function addSurvey() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwScrapManager/ProjectProfile-test-v3.cw");
            RootData.pageSelectionModel.currentPageAddress = "Data/Cave=Cave 1"

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "cavePage" });

            let addCaveButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->cavePage->addTrip->addButton")
            mouseClick(addCaveButton)

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

            let addSuveyButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->addSurveyData")
            mouseClick(addSuveyButton)

            wait(200);
        }

        function enterSurveyData() {
            //Start adding survey data
            keyClick("b")
            keyClick(49, 0) //1
            keyClick(16777220, 0) //Return

            //Make sure that we go to the next station entry
            keyClick(16777217, 0) //Tab got to the next cell
            keyClick(16777217, 0) //Tab add the next station B2

            //Check that we have two stations
            let trip = RootData.pageView.currentPageItem.currentTrip as Trip;
            verify(trip !== null);

            let firstChunk = trip.chunk(0);
            verify(firstChunk.stationCount() === 3)
            verify(firstChunk.data(SurveyChunk.StationNameRole, 0) === "b1")
            verify(firstChunk.data(SurveyChunk.StationNameRole, 1) === "b2")
            verify(firstChunk.data(SurveyChunk.StationNameRole, 2) === "")


            //Enter distance of 10
            // verify(firstChunk.data(SurveyChunk.ShotCompassRole, 0) === "")
            keyClick(49, 0) //1
            keyClick(48, 0) //0
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.ShotDistanceRole, 0) === "10")
            wait(50);

            //Compass of 0
            // verify(firstChunk.data(SurveyChunk.ShotCompassRole, 0) === "")
            keyClick(48, 0) //0
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.ShotCompassRole, 0) === "0")
            wait(50);

            //Skip back compass
            // verify(firstChunk.data(SurveyChunk.ShotBackCompassRole, 0) === "")
            keyClick(16777217, 0) //Tab
            // verify(firstChunk.data(SurveyChunk.ShotBackCompassRole, 0) === "")

            //Enter 11 for clino
            // verify(firstChunk.data(SurveyChunk.ShotClinoRole, 0) === "")
            keyClick(49, 0) //1
            keyClick(49, 0) //1
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.ShotClinoRole, 0) === "11")
            wait(50);

            //Skip the LRUD
            // keyClick(16777217, 0) //Tab
            keyClick(16777217, 0) //Tab
            keyClick(16777217, 0) //Tab
            keyClick(16777217, 0) //Tab
            keyClick(16777217, 0) //Tab
            keyClick(16777217, 0) //Tab
            keyClick(16777217, 0) //Tab
            keyClick(16777217, 0) //Tab
            keyClick(16777217, 0) //Tab
            keyClick(16777217, 0) //Tab

            //Connect to a1
            keyClick("a")
            keyClick(49, 0) //1
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.StationNameRole, 2) === "a1")
            wait(50);

            //Distance of 1
            keyClick(49, 0) //1
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.ShotDistanceRole, 1) === "1")
            wait(50);

            //Compasss of 10
            keyClick(49, 0) //1
            keyClick(48, 0) //0
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.ShotCompassRole, 1) === "10")
            wait(50);

            //Skip backcompass
            keyClick(16777217, 0) //Tab

            //Clino of 0
            keyClick(48, 0) //0
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.ShotClinoRole, 1) === "0")
            wait(50);
        }

        function test_enterSurveyData() {
            addSurvey();

            wait(10000);

            enterSurveyData();
        }

        function test_errorButtonsShouldWork() {
            addSurvey();

            //Start adding survey data
            keyClick("a")
            keyClick(49, 0) //1
            keyClick(16777220, 0) //Return
            keyClick(16777217, 0) //Tab
            keyClick(16777217, 0) //Tab

            //Check that we can supress the a warning
            let databox = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->dataBox.1.1")
            let errors = databox.errorModel.errors;

            verify(errors.count == 1);

            let firstErrorIndex = errors.index(0, 0);
            verify(errors.data(firstErrorIndex, ErrorListModel.ErrorTypeRole) === CwError.Warning)
            verify(errors.data(firstErrorIndex, ErrorListModel.SuppressedRole) === false)

            let errorIcon_obj1 = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->dataBox.1.1->errorIcon")
            mouseClick(errorIcon_obj1)

            // wait(1000000);

            let checkbox = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->errorBoxdataBox.1.1->suppress")
            mouseClick(checkbox)


            //Make sure the warning is supressed
            verify(errors.count == 1);
            verify(errors.data(firstErrorIndex, ErrorListModel.ErrorTypeRole) === CwError.Warning)
            tryVerify(()=>{ return errors.data(firstErrorIndex, ErrorListModel.SuppressedRole) === true; })

            //Make sure error message works, click on it
            let distanceError = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->dataBox.0.5->errorIcon")
            mouseClick(distanceError)

            //Make sure the warning message that was shown before is now hidden
            verify(errorIcon_obj1.visible === false)

            //Make sure the error message is correct
            let errorText = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->errorBoxdataBox.0.5->errorText")
            verify(errorText.text === "Missing \"distance\" from shot \"a1\" ➔ \"a2\"")

            //Deselect the current message
            mouseClick(distanceError)

            //Make sure the error quote box is hidden
            let quoteBox = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->errorBoxdataBox.0.5")
            tryVerify(()=>{ return quoteBox === null });
            verify(distanceError.checked === false);
        }

        function test_dateChangeShouldWork() {
            addSurvey();

            let date = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->tripDate")
            date.openEditor();
            // mouseDoubleClickSequence(date)

            keyClick(50, 0) //2
            keyClick(48, 0) //0
            keyClick(50, 0) //2
            keyClick(53, 0) //5
            keyClick(45, 0) //-
            keyClick(48, 0) //0
            keyClick(50, 0) //2
            keyClick(45, 0) //-
            keyClick(49, 0) //1
            keyClick(48, 0) //0
            keyClick(16777220, 0) //Return

            compare(date.text, "2025-02-10")
        }

        function test_excludeDistance() {
            addSurvey();

            enterSurveyData();

            let coreTextInput_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->dataBox.1.5->coreTextInput")
            mouseClick(coreTextInput_obj1)

            let excludeMenuButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->dataBox.1.5->excludeMenuButton")
            let excludeMenu = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->dataBox.1.5->excludeMenuButton").menu
            tryVerify(() => { return excludeMenu.visible === false})

            mouseClick(excludeMenuButton)

            tryVerify(() => { return excludeMenu.visible })

            let menuItem = findChild(excludeMenu, "excludeDistanceMenuItem")
            mouseClick(menuItem);

            tryVerify(() => { return excludeMenu.visible === false })

            //Make sure the distance has gone down
            let totalLengthText_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->totalLengthText")
            tryCompare(totalLengthText_obj1, "text", "Total Length: 10 m");

            //Include it again
            mouseClick(excludeMenuButton)

            tryVerify(() => { return excludeMenu.visible })

            menuItem = findChild(excludeMenu, "excludeDistanceMenuItem")
            mouseClick(menuItem);

            tryVerify(() => { return excludeMenu.visible === false })

            //Make sure the distance has gone down
            totalLengthText_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->totalLengthText")
            tryCompare(totalLengthText_obj1, "text", "Total Length: 11 m");
        }

        /**
          Test the tab interaction going forward and backwards with frontsights on, backsights on, and both
          frontsights and backsights on.
          */
        function test_surveyEditorNavigationTabWorks() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/tst_SurveyDataEntry/navTest.cw");

            RootData.pageSelectionModel.currentPageAddress = "Data/Cave=Cave 1/Trip=Trip 1"

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

            waitForRendering(RootData.pageView.currentPageItem)

            let station1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox.1.0")
            mouseClick(station1)

            waitForRendering(RootData.pageView.currentPageItem)

            function tabHelper(currentItem, index, nextRole) {
                let itemName = "rootId->tripPage->view->dataBox." + index + "." + nextRole
                let item = ObjectFinder.findObjectByChain(mainWindow, itemName)
                // console.log("Searching for item:" + itemName + " " + item)
                if(item === null) {
                    console.log("Testcase will fail, Failed to find item!!!");
                }

                verify(item !== null);

                console.log("CurrentItem:" + currentItem + " " + currentItem.focus + " index:" + index + " nextRole:" + nextRole + " nextItem:" + item + " " + item.focus)

                if(currentItem.focus === true) {
                    console.log("Testcase will fail, bad currentItem focus!!!");
                }

                verify(currentItem.focus === false);

                //Uncomment to help debug
                if(item.focus !== true) {
                    console.log("Testcase will fail, bad focus!!! current focused on:" + rootId.Window.window.activeFocusItem);
                }

                verify(item.focus === true )
                waitForRendering(RootData.pageView.currentPageItem)
                return item;
            }

            function nextTab(currentItem, index, nextRole) {
                verify(currentItem.focus === true)
                keyClick(16777217, 0) //Tab
                return tabHelper(currentItem, index, nextRole)
            }

            function previousTab(currentItem, index, nextRole) {
                verify(currentItem.focus === true)
                keyClick(16777218, 33554432) //Backtab, Shift+
                return tabHelper(currentItem, index, nextRole)
            }

            let surveyView = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view")
            let currentItem = station1;


            function runTabTest(frontsights, backsights) {
                //Go forward
                for(let i = 0; i < surveyView.count; i++) {
                    // console.log("i:" + i)
                    let item = surveyView.itemAtIndex(i) as DrySurveyComponent;
                    if(item.titleVisible) {
                        //Skip the title
                        continue;
                    }
                    if(!item.shotVisible) {
                        continue;
                    }

                    if(item.indexInChunk === 0) {
                        //First row in the chunk, go to the next station name
                        currentItem = nextTab(currentItem, i+1, SurveyChunk.StationNameRole);
                    }

                    if(frontsights && backsights) {
                        currentItem = nextTab(currentItem, i, SurveyChunk.ShotDistanceRole);
                        currentItem = nextTab(currentItem, i, SurveyChunk.ShotCompassRole);
                        currentItem = nextTab(currentItem, i, SurveyChunk.ShotBackCompassRole);
                        currentItem = nextTab(currentItem, i, SurveyChunk.ShotClinoRole);
                        currentItem = nextTab(currentItem, i, SurveyChunk.ShotBackClinoRole);
                    } else if(frontsights) {
                        currentItem = nextTab(currentItem, i, SurveyChunk.ShotDistanceRole);
                        currentItem = nextTab(currentItem, i, SurveyChunk.ShotCompassRole);
                        currentItem = nextTab(currentItem, i, SurveyChunk.ShotClinoRole);
                    } else if(backsights) {
                        currentItem = nextTab(currentItem, i, SurveyChunk.ShotDistanceRole);
                        currentItem = nextTab(currentItem, i, SurveyChunk.ShotBackCompassRole);
                        currentItem = nextTab(currentItem, i, SurveyChunk.ShotBackClinoRole);
                    }

                    if(item.indexInChunk === 0) {
                        currentItem = nextTab(currentItem, i-1, SurveyChunk.StationLeftRole);
                        currentItem = nextTab(currentItem, i-1, SurveyChunk.StationRightRole);
                        currentItem = nextTab(currentItem, i-1, SurveyChunk.StationUpRole);
                        currentItem = nextTab(currentItem, i-1, SurveyChunk.StationDownRole);
                    }

                    let nextStationOffset = function() {
                        let nextItem = surveyView.itemAtIndex(i+3);
                        if(nextItem) {
                            return 3;
                        }
                        return -1;
                    }();

                    currentItem = nextTab(currentItem, i+1, SurveyChunk.StationLeftRole);
                    currentItem = nextTab(currentItem, i+1, SurveyChunk.StationRightRole);
                    currentItem = nextTab(currentItem, i+1, SurveyChunk.StationUpRole);
                    currentItem = nextTab(currentItem, i+1, SurveyChunk.StationDownRole);

                    if(nextStationOffset > 0) {
                        currentItem = nextTab(currentItem, i + nextStationOffset, SurveyChunk.StationNameRole);
                    }
                }

                //Uncomment to debug tabbing from the end
                // surveyView.currentIndex = surveyView.count - 1;
                // waitForRendering(RootData.pageView.currentPageItem)
                // let itemName = "rootId->tripPage->view->dataBox." + surveyView.currentIndex + "." + SurveyChunk.StationDownRole;
                // let lastItem = ObjectFinder.findObjectByChain(mainWindow, itemName)
                // currentItem = lastItem;
                // currentItem.forceActiveFocus();
                // surveyView.positionViewAtEnd();
                // waitForRendering(surveyView)

                //Go backwards
                for(let i = surveyView.count - 1; i > 0; i--) {
                    // console.log("---- Backwords i:" + i + " " + currentItem + "-----")

                    let item = surveyView.itemAtIndex(i) as DrySurveyComponent;
                    if(item.titleVisible) {
                        //Skip the title
                        continue;
                    }

                    currentItem = previousTab(currentItem, i, SurveyChunk.StationUpRole);
                    currentItem = previousTab(currentItem, i, SurveyChunk.StationRightRole);
                    currentItem = previousTab(currentItem, i, SurveyChunk.StationLeftRole);

                    if(item.indexInChunk === 1) {
                        currentItem = previousTab(currentItem, i-2, SurveyChunk.StationDownRole);
                        currentItem = previousTab(currentItem, i-2, SurveyChunk.StationUpRole);
                        currentItem = previousTab(currentItem, i-2, SurveyChunk.StationRightRole);
                        currentItem = previousTab(currentItem, i-2, SurveyChunk.StationLeftRole);
                    }

                    if(frontsights && backsights) {
                        currentItem = previousTab(currentItem, i-1, SurveyChunk.ShotBackClinoRole);
                        currentItem = previousTab(currentItem, i-1, SurveyChunk.ShotClinoRole);
                        currentItem = previousTab(currentItem, i-1, SurveyChunk.ShotBackCompassRole);
                        currentItem = previousTab(currentItem, i-1, SurveyChunk.ShotCompassRole);
                        currentItem = previousTab(currentItem, i-1, SurveyChunk.ShotDistanceRole);
                    } else if(frontsights) {
                        currentItem = previousTab(currentItem, i-1, SurveyChunk.ShotClinoRole);
                        currentItem = previousTab(currentItem, i-1, SurveyChunk.ShotCompassRole);
                        currentItem = previousTab(currentItem, i-1, SurveyChunk.ShotDistanceRole);
                    } else if(backsights) {
                        currentItem = previousTab(currentItem, i-1, SurveyChunk.ShotBackClinoRole);
                        currentItem = previousTab(currentItem, i-1, SurveyChunk.ShotBackCompassRole);
                        currentItem = previousTab(currentItem, i-1, SurveyChunk.ShotDistanceRole);
                    }

                    currentItem = previousTab(currentItem, i, SurveyChunk.StationNameRole);

                    if(item.indexInChunk === 1) {
                        currentItem = previousTab(currentItem, i-2, SurveyChunk.StationNameRole);
                    }

                    let prevStationOffset = function() {
                        let prevItem = surveyView.itemAtIndex(i-3);
                        if(prevItem) {
                            console.log("prevItem titleVisible:" + prevItem.rowType === SurveyEditorModel.TitleRow + " " + prevItem.rowType + " i:" + i);
                            // return 2;
                            return prevItem.rowType === SurveyEditorModel.TitleRow ? 4 : 2
                        }
                        return 1;
                    }();

                    //Loop up to the previous row
                    if(i-prevStationOffset >= 0) {
                        // console.log("prevStationOffset:" + prevStationOffset);
                        currentItem = previousTab(currentItem, i-prevStationOffset, SurveyChunk.StationDownRole);

                        if(prevStationOffset > 1) {
                            i -= prevStationOffset - 1
                        }
                    } else {
                        //At the very beginning of the list
                        break;
                    }
                }
            }


            let frontsight = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->frontSightCalibrationEditor->checkBox")
            let backsight = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->backSightCalibrationEditor->checkBox")

            verify(frontsight.checked === true)
            verify(backsight.checked === false)

            runTabTest(frontsight.checked, backsight.checked)

            //Scroll to the top
            surveyView.positionViewAtBeginning();
            waitForRendering(surveyView)

            // ----- Enable the backsights ----
            mouseClick(backsight);

            verify(frontsight.checked === true)
            verify(backsight.checked === true)

            runTabTest(frontsight.checked, backsight.checked)

            // ----- Backsights only ----
            //Scroll to the top
            surveyView.positionViewAtBeginning();
            waitForRendering(surveyView)

            mouseClick(frontsight);

            verify(frontsight.checked === false)
            verify(backsight.checked === true)

            runTabTest(frontsight.checked, backsight.checked)

        }

        /**
          Tabing backwards from the first element should stay focus.
          Tabing forward from the last element should stay focus
          */
        function test_tabEndingShouldStayFocus() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/tst_SurveyDataEntry/navTest.cw");

            RootData.pageSelectionModel.currentPageAddress = "Data/Cave=Cave 1/Trip=Trip 1"

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

            waitForRendering(RootData.pageView.currentPageItem)

            let station1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox.1.0")
            mouseClick(station1)

            waitForRendering(RootData.pageView.currentPageItem)

            // wait(100000);

            //Go backwards
            verify(station1.focus === true)
            keyClick(16777218, 33554432) //Backtab, Shift+

            tryVerify(() => { return station1.focus === true; })


            //-- Go the last element and tab forward
            let surveyView = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view")
            surveyView.currentIndex = surveyView.count - 1;
            surveyView.positionViewAtEnd();
            waitForRendering(RootData.pageView.currentPageItem)

            let itemName = "rootId->tripPage->view->dataBox." + surveyView.currentIndex + "." + SurveyChunk.StationDownRole;
            let lastItem = ObjectFinder.findObjectByChain(mainWindow, itemName)
            mouseClick(lastItem);
            waitForRendering(RootData.pageView.currentPageItem)

            //Go forward
            verify(lastItem.focus === true)

            keyClick(16777217, 0) //Tab

            verify(lastItem.focus === true);
        }

        function test_tabGuessSurveyName() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/tst_SurveyDataEntry/navTest.cw");

            RootData.pageSelectionModel.currentPageAddress = "Data/Cave=Cave 1/Trip=Trip 1"

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

            waitForRendering(RootData.pageView.currentPageItem)

            //-- Go the last element and tab forward
            let surveyView = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view")
            surveyView.currentIndex = surveyView.count - 1;
            surveyView.positionViewAtEnd();
            waitForRendering(RootData.pageView.currentPageItem)

            let itemName = "rootId->tripPage->view->dataBox." + surveyView.currentIndex + "." + SurveyChunk.StationNameRole;
            let lastItem = ObjectFinder.findObjectByChain(mainWindow, itemName)
            mouseClick(lastItem);
            waitForRendering(RootData.pageView.currentPageItem)

            keyClick(16777217, 0) //Tab

            verify(lastItem.dataValue === "9", `"${lastItem.dataValue}" === "9"`);
        }

        function test_editorTest() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/tst_SurveyDataEntry/navTest.cw");

            RootData.pageSelectionModel.currentPageAddress = "Data/Cave=Cave 1/Trip=Trip 1"

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

            waitForRendering(RootData.pageView.currentPageItem)
            wait(1000000);
        }
    }
}
