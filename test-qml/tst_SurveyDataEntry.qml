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

            let addSuveyButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->addSurveyData")
            mouseClick(addSuveyButton)

            waitForRendering(rootId)

            // wait(10000);
        }

        function enterSurveyData() {
            waitForRendering(rootId);

            // wait(100);

            //Start adding survey data
            keyClick("b")
            keyClick(49, 0) //1
            keyClick(16777220, 0) //Return

            waitForRendering(rootId);

            //Make sure that we go to the next station entry
            keyClick(16777217, 0) //Tab got to the next cell
            waitForRendering(rootId);

            // wait(1000000);

            keyClick(16777217, 0) //Tab add the next station B2
            waitForRendering(rootId);

            //Check that we have two stations
            let trip = RootData.pageView.currentPageItem.currentTrip as Trip;
            verify(trip !== null);

            // wait(100000);


            let firstChunk = trip.chunk(0);
            verify(firstChunk.stationCount() === 3)
            verify(firstChunk.data(SurveyChunk.StationNameRole, 0) === "b1")
            verify(firstChunk.data(SurveyChunk.StationNameRole, 1) === "b2")
            verify(firstChunk.data(SurveyChunk.StationNameRole, 2) === "")

            //Enter distance of 10
            verify(firstChunk.data(SurveyChunk.ShotCompassRole, 0).value === "")
            keyClick(49, 0) //1
            keyClick(48, 0) //0
            keyClick(16777217, 0) //Tab

            verify(firstChunk.data(SurveyChunk.ShotDistanceRole, 0).value === "10")
            waitForRendering(rootId);


            //Compass of 0
            verify(firstChunk.data(SurveyChunk.ShotCompassRole, 0).value === "")
            keyClick(48, 0) //0
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.ShotCompassRole, 0).value === "0")
            waitForRendering(rootId)

            //12 back compass
            verify(firstChunk.data(SurveyChunk.ShotBackCompassRole, 0).value === "")
            keyClick(49, 0) //1
            keyClick(50, 0) //2
            keyClick(16777220, 0) //Return
            verify(firstChunk.data(SurveyChunk.ShotBackCompassRole, 0).value === "12")
            keyClick(16777217, 0) //Tab


            //Enter 11 for clino
            verify(firstChunk.data(SurveyChunk.ShotClinoRole, 0).value === "")
            keyClick(49, 0) //1
            keyClick(49, 0) //1
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.ShotClinoRole, 0).value === "11")
            waitForRendering(rootId)

            //13 back clino
            verify(firstChunk.data(SurveyChunk.ShotBackClinoRole, 0).value === "")
            keyClick(49, 0) //1
            keyClick(51, 0) //3
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.ShotBackClinoRole, 0).value === "13")


            //Left
            verify(firstChunk.data(SurveyChunk.StationLeftRole, 0).value === "")
            keyClick(48, 0) //0
            keyClick(46, 0) //.
            keyClick(50, 0) //2
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.StationLeftRole, 0).value === "0.2")


            //Right
            verify(firstChunk.data(SurveyChunk.StationRightRole, 0).value === "")
            keyClick(49, 0) //1
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.StationRightRole, 0).value === "1")

            //Up
            verify(firstChunk.data(SurveyChunk.StationUpRole, 0).value === "")
            keyClick(50, 0) //2
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.StationUpRole, 0).value === "2")

            //Down
            verify(firstChunk.data(SurveyChunk.StationDownRole, 0).value === "")
            keyClick(51, 0) //3
            keyClick(16777220, 0) //Return
            verify(firstChunk.data(SurveyChunk.StationDownRole, 0).value === "3")
            keyClick(16777217, 0) //Tab

            //Skip the LRUD
            keyClick(16777217, 0) //Tab
            keyClick(16777217, 0) //Tab
            keyClick(16777217, 0) //Tab
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.StationLeftRole, 1).value === "")
            verify(firstChunk.data(SurveyChunk.StationRightRole, 1).value === "")
            verify(firstChunk.data(SurveyChunk.StationUpRole, 1).value === "")
            verify(firstChunk.data(SurveyChunk.StationDownRole, 1).value === "")


            //Connect to a1
            keyClick("a")
            keyClick(49, 0) //1
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.StationNameRole, 2) === "a1")
            waitForRendering(rootId)


            //Distance of 1
            keyClick(49, 0) //1
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.ShotDistanceRole, 1).value === "1")
            waitForRendering(rootId)

            //Compasss of 10
            keyClick(49, 0) //1
            keyClick(48, 0) //0
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.ShotCompassRole, 1).value === "10")
            waitForRendering(rootId)

            //Skip backcompass
            keyClick(16777217, 0) //Tab

            //Clino of 0
            keyClick(48, 0) //0
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.ShotClinoRole, 1).value === "0")
            waitForRendering(rootId)

            //make sure the secondChunk chunk doesn't exist before we create it
            verify(trip.chunkCount === 1);

            //Make a new scrap
            keyClick(32, 0) //Space
            verify(firstChunk)

            //Make sure focus is on the secondChunk
            let stationBox = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->view->dataBox.7.0")
            console.log("Focus:" + rootId.Window.window.activeFocusItem)
            verify(stationBox.focus === true);

            // verify

            //Make sure the first chunk gets trimmed
            verify(firstChunk.stationCount() === 3)
            verify(firstChunk.shotCount() === 2)

            //Make sure the new scrap exists
            verify(trip.chunkCount === 2);
            let secondChunk = trip.chunk(1);
            verify(secondChunk !== null);


            //Add data
            keyClick("a")
            keyClick(49, 0) //1
            keyClick(16777217, 0) //Tab

            console.log("Name:" + secondChunk.data(SurveyChunk.StationNameRole, 0))
            verify(secondChunk.data(SurveyChunk.StationNameRole, 0) === "a1")

            //Auto guess next row
            keyClick(16777217, 0) //Tab
            verify(secondChunk.data(SurveyChunk.StationNameRole, 1) === "a2")

            waitForRendering(rootId);

            keyClick(52, 0) //4
            keyClick(16777217, 0) //Tab
            verify(secondChunk.data(SurveyChunk.ShotDistanceRole, 0).value === "4")

            keyClick(51, 0) //3
            keyClick(48, 0) //0
            keyClick(16777217, 0) //Tab
            verify(secondChunk.data(SurveyChunk.ShotCompassRole, 0).value === "30")

            //Skip backsight
            keyClick(16777217, 0) //Tab
            verify(secondChunk.data(SurveyChunk.ShotBackCompassRole, 0).value === "")

            keyClick(50, 0) //2
            keyClick(48, 0) //0
            keyClick(16777217, 0) //Tab
            verify(secondChunk.data(SurveyChunk.ShotClinoRole, 0).value === "20")
            verify(secondChunk.data(SurveyChunk.ShotBackClinoRole, 0).value === "")
            verify(secondChunk.data(SurveyChunk.StationLeftRole, 0).value === "")
            verify(secondChunk.data(SurveyChunk.StationRightRole, 0).value === "")
            verify(secondChunk.data(SurveyChunk.StationUpRole, 0).value === "")
            verify(secondChunk.data(SurveyChunk.StationDownRole, 0).value === "")

            waitForRendering(rootId);
        }

        function navHelper(currentItem,
                           index,
                           nextRole,
                           key,
                           modifier)
        {
            if(currentItem !== null) {
                if(currentItem === undefined) {
                    console.log("Undefined!")
                }

                if(currentItem.focus !== true) {
                    console.log("Focus failed!, set breakpoint here" + currentItem);
                }

                verify(currentItem.focus === true)
            }

            keyClick(key, modifier) //Tab

            let itemName = "rootId->tripPage->view->dataBox." + index + "." + nextRole
            let item = ObjectFinder.findObjectByChain(mainWindow, itemName)
            if(item === null) {
                console.log("Searching for item:" + itemName + " current focused:" + mainWindow.Window.window.activeFocusItem)
                console.log("Testcase will fail, Failed to find item!!!");
            }

            verify(item !== null);

            if(currentItem !== null) {
                if(currentItem.focus !== false) {
                    console.log("Testcase will fail, bad currentItem focus!!! place breakpoint here" + currentItem);
                }

                verify(currentItem.focus === false);
            }

            //Uncomment to help debug
            if(item.focus !== true) {
                console.log("Testcase will fail, bad focus!!! current focused on:" + rootId.Window.window.activeFocusItem + "but should be focused on" + item);
            }

            verify(item.focus === true )

            //Enable for debugging this, this will slow down the testcase
            // waitForRendering(mainWindow)
            return item;
        }

        function nextTab(currentItem, index, nextRole) {
            let tab = 16777217; //Tab arrow keey
            let modifier = 0;
            return navHelper(currentItem, index, nextRole, tab, modifier);
        }

        function downArrow(currentItem, index, nextRole) {
            let tab = 16777237; //Down arrow key
            let modifier = 0;
            return navHelper(currentItem, index, nextRole, tab, modifier);
        }

        function rightArrow(currentItem, index, nextRole) {
            let right = 16777236; //Right arrow key
            let modifier = 0;
            return navHelper(currentItem, index, nextRole, right, modifier);
        }

        function previousTab(currentItem, index, nextRole) {
            let tab = 16777218;
            let modifier = 33554432;
            return navHelper(currentItem, index, nextRole, tab, modifier);
        }

        function test_enterSurveyData() {
            addSurvey();

            enterSurveyData();
        }

        function test_errorButtonsShouldWork() {
            addSurvey();
            // wait(100000);

            //Start adding survey data
            keyClick("a")
            keyClick(49, 0) //1
            keyClick(16777220, 0) //Return
            keyClick(16777217, 0) //Tab
            keyClick(16777217, 0) //Tab

            //Check that we can supress the a warning
            let databox = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->view->dataBox.1.1")
            verify(databox !== null)
            verify(databox.errorModel !== null);
            let errors = databox.errorModel.errors;

            verify(errors.count == 1);

            let firstErrorIndex = errors.index(0, 0);
            verify(errors.data(firstErrorIndex, ErrorListModel.ErrorTypeRole) === CwError.Warning)
            verify(errors.data(firstErrorIndex, ErrorListModel.SuppressedRole) === false)


            let errorIcon_obj1 = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->view->dataBox.1.1->coreTextInput->errorIcon")
            mouseClick(errorIcon_obj1)

            waitForRendering(rootId);

            let checkbox = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->errorBoxdataBox.1.1->suppress")
            mouseClick(checkbox)

            waitForRendering(rootId);

            //Make sure the warning is supressed
            verify(errors.count == 1);
            verify(errors.data(firstErrorIndex, ErrorListModel.ErrorTypeRole) === CwError.Warning)
            tryVerify(()=>{ return errors.data(firstErrorIndex, ErrorListModel.SuppressedRole) === true; })

            //Make sure error message works, click on it
            let distanceError = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->view->dataBox.2.5->coreTextInput->errorIcon")
            mouseClick(distanceError)
            waitForRendering(rootId);

            //Make sure the warning message that was shown before is now hidden
            verify(errorIcon_obj1.visible === false)

            //Make sure the error message is correct
            let errorText = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->errorBoxdataBox.2.5->errorText")
            verify(errorText.text === "Missing \"distance\" from shot \"a1\" ➔ \"a2\"")

            //Deselect the current message
            mouseClick(distanceError)
            waitForRendering(rootId);

            //Make sure the error quote box is hidden
            let quoteBox = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->errorBoxdataBox.2.5")
            tryVerify(()=>{ return quoteBox === null });
            verify(distanceError.checked === false);
        }

        function test_dateChangeShouldWork() {
            addSurvey();

            let date = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->tripDate")
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

            let coreTextInput_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox.2.5->coreTextInput")
            mouseClick(coreTextInput_obj1)

            let excludeMenuButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox.2.5->excludeMenuButton")
            let excludeMenu = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox.2.5->excludeMenuButton").menu
            tryVerify(() => { return excludeMenu.visible === false})

            mouseClick(excludeMenuButton)

            tryVerify(() => { return excludeMenu.visible })

            let menuItem = findChild(excludeMenu, "excludeDistanceMenuItem")
            mouseClick(menuItem);

            tryVerify(() => { return excludeMenu.visible === false })

            let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view");
            view.positionViewAtEnd()

            //Make sure the distance has gone down
            let totalLengthText_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->totalLengthText")
            tryCompare(totalLengthText_obj1, "text", "Total Length: 5 m");

            //Include it again
            mouseClick(excludeMenuButton)

            tryVerify(() => { return excludeMenu.visible })

            menuItem = findChild(excludeMenu, "excludeDistanceMenuItem")
            mouseClick(menuItem);

            tryVerify(() => { return excludeMenu.visible === false })

            //Make sure the distance has gone down
            totalLengthText_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->totalLengthText")
            tryCompare(totalLengthText_obj1, "text", "Total Length: 15 m");
        }

        /**
          Test arrow keys for navigation
          */
        function test_arrowNavigation() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/tst_SurveyDataEntry/navTest.cw");

            RootData.pageSelectionModel.currentPageAddress = "Data/Cave=Cave 1/Trip=Trip 1"

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

            waitForRendering(mainWindow)


            // wait(1000000)

            function rightArrowOnFullRow(currentItem, rowIndex) {
                let shotOffset = rowIndex === 1 ? 1 : -1
                let lrudOffset = rowIndex === 1 ? 2 : 0

                currentItem = rightArrow(currentItem, rowIndex+shotOffset, SurveyChunk.ShotDistanceRole);

                if(frontsight.checked && backsight.checked) {
                    currentItem = rightArrow(currentItem, rowIndex+shotOffset, SurveyChunk.ShotBackCompassRole);
                    currentItem = rightArrow(currentItem, rowIndex+shotOffset, SurveyChunk.ShotBackClinoRole);
                } else if(frontsight.checked) {
                    currentItem = rightArrow(currentItem, rowIndex+shotOffset, SurveyChunk.ShotCompassRole);
                    currentItem = rightArrow(currentItem, rowIndex+shotOffset, SurveyChunk.ShotClinoRole);
                } else if (backsight.checked) {
                    currentItem = rightArrow(currentItem, rowIndex+shotOffset, SurveyChunk.ShotBackCompassRole);
                    currentItem = rightArrow(currentItem, rowIndex+shotOffset, SurveyChunk.ShotBackClinoRole);
                }

                currentItem = rightArrow(currentItem, rowIndex+lrudOffset, SurveyChunk.StationLeftRole);
                currentItem = rightArrow(currentItem, rowIndex+lrudOffset, SurveyChunk.StationRightRole);
                currentItem = rightArrow(currentItem, rowIndex+lrudOffset, SurveyChunk.StationUpRole);
                currentItem = rightArrow(currentItem, rowIndex+lrudOffset, SurveyChunk.StationDownRole);


                //Make sure we can't continue going
                currentItem = rightArrow(null, rowIndex+lrudOffset, SurveyChunk.StationDownRole);
            }

            // function rightArrowOnFullRow(currentItem, rowIndex) {
            //     let shotOffset = rowIndex === 1 ? 1 : -1
            //     let lrudOffset = rowIndex === 1 ? 2 : 0

            //     currentItem = rightArrow(currentItem, rowIndex+shotOffset, SurveyChunk.ShotDistanceRole);

            //     if(frontsight.checked && backsight.checked) {
            //         currentItem = rightArrow(currentItem, rowIndex+shotOffset, SurveyChunk.ShotBackCompassRole);
            //         currentItem = rightArrow(currentItem, rowIndex+shotOffset, SurveyChunk.ShotBackClinoRole);
            //     } else if(frontsight.checked) {
            //         currentItem = rightArrow(currentItem, rowIndex+shotOffset, SurveyChunk.ShotCompassRole);
            //         currentItem = rightArrow(currentItem, rowIndex+shotOffset, SurveyChunk.ShotClinoRole);
            //     } else if (backsight.checked) {
            //         currentItem = rightArrow(currentItem, rowIndex+shotOffset, SurveyChunk.ShotBackCompassRole);
            //         currentItem = rightArrow(currentItem, rowIndex+shotOffset, SurveyChunk.ShotBackClinoRole);
            //     }

            //     currentItem = rightArrow(currentItem, rowIndex+lrudOffset, SurveyChunk.StationLeftRole);
            //     currentItem = rightArrow(currentItem, rowIndex+lrudOffset, SurveyChunk.StationRightRole);
            //     currentItem = rightArrow(currentItem, rowIndex+lrudOffset, SurveyChunk.StationUpRole);
            //     currentItem = rightArrow(currentItem, rowIndex+lrudOffset, SurveyChunk.StationDownRole);


            //     //Make sure we can't continue going
            //     currentItem = rightArrow(null, rowIndex+lrudOffset, SurveyChunk.StationDownRole);
            // }





            let frontsight = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->frontSightCalibrationEditor->checkBox")
            let backsight = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->backSightCalibrationEditor->checkBox")

            function testRow(row, direction) {
                let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view");
                view.positionViewAtIndex(row, ListView.Contain)

                let station = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + ".0")
                mouseClick(station)

                verify(frontsight.checked === true)
                verify(backsight.checked === false)
                verify(station.focus === true);

                direction(station, row);

                // ----- Enable the backsights and frontsights ----
                mouseClick(backsight);

                verify(frontsight.checked === true)
                verify(backsight.checked === true)

                mouseClick(station)
                verify(station.focus === true);

                direction(station, row);

                // ----- Enable the backsights only
                mouseClick(frontsight);

                verify(frontsight.checked === false)
                verify(backsight.checked === true)

                mouseClick(station)
                verify(station.focus === true);

                direction(station, row);

                mouseClick(frontsight);
                mouseClick(backsight);
            }

            //Test the first row
            testRow(1, rightArrowOnFullRow)

            //Test a middle row
            // wait(100000);
            testRow(5, rightArrowOnFullRow)

            //Test last row
            testRow(9, rightArrowOnFullRow)
        }

        /**
          Test the tab interaction going forward and backwards with frontsights on, backsights on, and both
          frontsights and backsights on.
          */
        function test_tabNavigationTabWorks() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/tst_SurveyDataEntry/navTest.cw");

            RootData.pageSelectionModel.currentPageAddress = "Data/Cave=Cave 1/Trip=Trip 1"

            tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

            waitForRendering(mainWindow)

            let station1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox.1.0")
            mouseClick(station1)

            waitForRendering(mainWindow)




            let surveyView = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view")
            let currentItem = station1;

            function runTabTest(frontsights, backsights) {
                //Go forward
                for(let i = 0; i < surveyView.count; i++) {
                    // console.log("tab i:" + i)
                    let item = surveyView.itemAtIndex(i) as DrySurveyComponent;
                    if(item.rowIndex.rowType === SurveyEditorRowIndex.TitleRow) {
                        //Skip the title
                        continue;
                    }
                    if(item.rowIndex.rowType !== SurveyEditorRowIndex.ShotRow) {
                        continue;
                    }

                    if(item.rowIndex.indexInChunk === 0) {
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

                    if(item.rowIndex.indexInChunk === 0) {
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

                        if(currentItem.dataValue.reading.value === "") {
                            // console.log("Last station:" + (i + 4) + " " + surveyView.count)
                            if(i + 4 >= surveyView.count) {
                                //Done
                                break;
                            }

                            //probably at the last row, skip
                            //We need to use null instead of currentItem, because the currentItem gets delete
                            //The delete currentItem will cause a type error in the testcase
                            currentItem = downArrow(null, i + 3, SurveyChunk.StationNameRole)
                        }
                    }
                }

                console.log("Reverse!")

                //Uncomment to debug tabbing from the end
                // surveyView.currentIndex = surveyView.count - 1;
                // waitForRendering(RootData.pageView.currentPageItem)
                // let itemName = "rootId->tripPage->view->dataBox." + surveyView.currentIndex + "." + SurveyChunk.StationDownRole;
                // let lastItem = ObjectFinder.findObjectByChain(mainWindow, itemName)
                // currentItem = lastItem;
                // currentItem.forceActiveFocus();
                // surveyView.positionViewAtEnd();
                // waitForRendering(surveyView)

                //Go to the previous down
                currentItem = previousTab(currentItem, surveyView.count - 3, SurveyChunk.StationDownRole);

                //Go backwards
                for(let i = surveyView.count - 3; i > 0; i--) {
                    // console.log("---- Backwords i:" + i + " " + currentItem + "-----")

                    let item = surveyView.itemAtIndex(i) as DrySurveyComponent;
                    if(item.rowIndex.rowType === SurveyEditorRowIndex.TitleRowp) {
                        //Skip the title
                        continue;
                    }

                    currentItem = previousTab(currentItem, i, SurveyChunk.StationUpRole);
                    currentItem = previousTab(currentItem, i, SurveyChunk.StationRightRole);
                    currentItem = previousTab(currentItem, i, SurveyChunk.StationLeftRole);

                    if(item.rowIndex.indexInChunk === 1) {
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


                    let prevStationOffset = 2;
                    if(item.rowIndex.indexInChunk === 1) {
                        currentItem = previousTab(currentItem, i-2, SurveyChunk.StationNameRole);
                        prevStationOffset = 4;
                    }

                    //Loop up to the previous row
                    if(i-prevStationOffset >= 0) {
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

            verify(lastItem.dataValue.reading.value === "9", `"${lastItem.dataValue.reading.value}" === "9"`);
        }

        // function test_editorTest() {
        //     TestHelper.loadProjectFromFile(RootData.project, "://datasets/tst_SurveyDataEntry/navTest.cw");

        //     RootData.pageSelectionModel.currentPageAddress = "Data/Cave=Cave 1/Trip=Trip 1"

        //     tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

        //     waitForRendering(RootData.pageView.currentPageItem)
        //     wait(1000000);
        // }
    }
}
