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
            verify(firstChunk.stationCount === 3)
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
            verify(stationBox.focus === true);

            // verify

            //Make sure the first chunk gets trimmed
            verify(firstChunk.stationCount === 3)
            verify(firstChunk.shotCount === 2)

            //Make sure the new scrap exists
            verify(trip.chunkCount === 2);
            let secondChunk = trip.chunk(1);
            verify(secondChunk !== null);

            wait(50)

            //Add data
            keyClick("a")
            keyClick(49, 0) //1
            keyClick(16777217, 0) //Tab

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
            // wait(25);
            // waitForRendering(rootId)

            let itemName = "rootId->tripPage->view->dataBox." + index + "." + nextRole
            let item = ObjectFinder.findObjectByChain(mainWindow, itemName)
            if(item === null) {
                console.log("Searching for item:" + itemName + " current focused:" + mainWindow.Window.window.activeFocusItem)
                console.log("Testcase will fail, Failed to find item!!!");
                // wait(100000);
            }

            verify(item !== null);

            if(currentItem !== null) {
                if(currentItem.focus !== false) {
                    console.log("Testcase will fail, bad currentItem focus!!! place breakpoint here" + currentItem);
                    wait(100000)
                }

                verify(currentItem.focus === false);
            }

            //Uncomment to help debug
            if(item.focus !== true) {
                console.log("Testcase will fail, bad focus!!! current focused on:" + rootId.Window.window.activeFocusItem + "but should be focused on" + item);
                // wait(100000)
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
            let downArrow = 16777237; //Down arrow key
            let modifier = 0;
            return navHelper(currentItem, index, nextRole, downArrow, modifier);
        }

        function upArrow(currentItem, index, nextRole) {
            let upArrow = 16777235; //up arrow key
            let modifier = 0;
            return navHelper(currentItem, index, nextRole, upArrow, modifier);
        }

        function rightArrow(currentItem, index, nextRole) {
            let right = 16777236; //Right arrow key
            let modifier = 0;
            return navHelper(currentItem, index, nextRole, right, modifier);
        }

        function leftArrow(currentItem, index, nextRole) {
            let left = 16777234; //Left arrow key
            let modifier = 0;
            return navHelper(currentItem, index, nextRole, left, modifier);
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

        function test_spaceBarVisible() {
            addSurvey();
            let spaceBar = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->view->spaceAddBar")
            verify(spaceBar.visible === true)

            enterSurveyData();
            verify(spaceBar.visible === true)
        }

        /**
          Test to see if an error is shown when the chunk isn't connected
          */
        function test_notConnected() {
            addSurvey();
            enterSurveyData();

            let a1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox.7.0->coreTextInput")
            mouseClick(a1, 38.9219, 10.4531)

            let chunkError = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->chunkErrorDelegate.6")
            let errorText = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->chunkErrorDelegate.6->errorText")
            verify(chunkError.visible === false);
            verify(errorText.text === "")

            //Change to a1 to c1, this will disconnect the chunk
            keyClick("c")
            keyClick(49, 0) //1
            keyClick(16777220, 0) //Return

            let a2 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox.9.0->coreTextInput")
            mouseClick(a2)

            keyClick("d")
            keyClick(50, 0) //2
            keyClick(16777220, 0) //Return

            verify(chunkError.visible === true);
            verify(errorText.text === "Survey leg isn't connect to the cave")
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
            verify(errors.count === 1);
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

            let frontsight = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->frontSightCalibrationEditor->checkBox")
            let backsight = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->backSightCalibrationEditor->checkBox")

            function rightArrowOnFullRow(currentItem, rowIndex, columnIndex) {
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

            function leftArrowOnFullRow(currentItem, rowIndex, columnIndex) {
                let shotOffset = rowIndex === 1 ? 1 : -1
                let lrudOffset = 0; //rowIndex === 1 ? 2 : 0
                let stationOffset = rowIndex === 1 ? 2 : 0

                currentItem = leftArrow(currentItem, rowIndex+lrudOffset, SurveyChunk.StationUpRole);
                currentItem = leftArrow(currentItem, rowIndex+lrudOffset, SurveyChunk.StationRightRole);
                currentItem = leftArrow(currentItem, rowIndex+lrudOffset, SurveyChunk.StationLeftRole);

                if(frontsight.checked && backsight.checked) {
                    currentItem = leftArrow(currentItem, rowIndex+shotOffset, SurveyChunk.ShotBackClinoRole);
                    currentItem = leftArrow(currentItem, rowIndex+shotOffset, SurveyChunk.ShotBackCompassRole);
                } else if(frontsight.checked) {
                    currentItem = leftArrow(currentItem, rowIndex+shotOffset, SurveyChunk.ShotClinoRole);
                    currentItem = leftArrow(currentItem, rowIndex+shotOffset, SurveyChunk.ShotCompassRole);
                } else if (backsight.checked) {
                    currentItem = leftArrow(currentItem, rowIndex+shotOffset, SurveyChunk.ShotBackClinoRole);
                    currentItem = leftArrow(currentItem, rowIndex+shotOffset, SurveyChunk.ShotBackCompassRole);
                }
                currentItem = leftArrow(currentItem, rowIndex+shotOffset, SurveyChunk.ShotDistanceRole);

                //Make sure we can't continue going
                currentItem = leftArrow(null, rowIndex+stationOffset, SurveyChunk.StationNameRole);
            }

            function downArrowOnFullColumn(currentItem, rowIndex, columnIndex) {
                let surveyView = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view")

                for(let i = rowIndex + 2; i < surveyView.count; i += 2) {
                    let chunk = currentItem.dataValue.chunk
                    let stationCount = chunk.stationCount;
                    let shotCount = chunk.shotCount;
                    let lastStationIndex = stationCount - 1
                    let lastShotIndex = shotCount - 1;
                    //console.log("i:" + i + " " + surveyView.count + " " + currentItem.dataValue.indexInChunk + " " + lastStationIndex + " " + (currentItem.dataValue.indexInChunk === lastStationIndex))
                    if(currentItem.dataValue.rowType === SurveyEditorRowIndex.StationRow
                            && currentItem.dataValue.indexInChunk === lastStationIndex)
                    {
                        if(i + 1 === surveyView.count) {
                            break;
                        }

                        currentItem = null
                        i -= 2;
                    } else if(currentItem.dataValue.rowType === SurveyEditorRowIndex.ShotRow) {
                        if(columnIndex === SurveyChunk.ShotCompassRole && backsight.checked && frontsight.checked) {
                            currentItem = downArrow(currentItem, i-2, SurveyChunk.ShotBackCompassRole)
                        } else if(columnIndex === SurveyChunk.ShotClinoRole && backsight.checked && frontsight.checked) {
                            currentItem = downArrow(currentItem, i-2, SurveyChunk.ShotBackClinoRole)
                        }

                        if(currentItem.dataValue.indexInChunk === lastShotIndex) {
                            if(i === surveyView.count) {
                                break;
                            }
                            currentItem = null;
                        }
                    }

                    currentItem = downArrow(currentItem, i, columnIndex);
                }

                //Scroll to the top
                surveyView.positionViewAtBeginning()
                waitForRendering(rootId)
            }

            function upArrowOnFullColumn(currentItem, rowIndex, columnIndex) {
                let surveyView = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view")

                //Scroll to the bottom
                surveyView.positionViewAtEnd()
                waitForRendering(rootId)

                for(let i = rowIndex; i >= 1; i -= 2) {
                    let chunk = currentItem.dataValue.chunk
                    let stationCount = chunk.stationCount;
                    let shotCount = chunk.shotCount;
                    let firstStationIndex = 0
                    let firstShotIndex = 0;
                    // console.log("i:" + i + " " + surveyView.count + " " + currentItem.dataValue.indexInChunk + " " + firstStationIndex + " " + (currentItem.dataValue.indexInChunk === firstStationIndex))
                    if(currentItem.dataValue.rowType === SurveyEditorRowIndex.StationRow
                            && currentItem.dataValue.indexInChunk === firstStationIndex)
                    {
                        if(i - 1 === 0) {
                            break;
                        }

                        currentItem = null
                    } else if(currentItem.dataValue.rowType === SurveyEditorRowIndex.ShotRow) {
                        let offset = 0;

                        if(columnIndex === SurveyChunk.ShotBackCompassRole && backsight.checked && frontsight.checked) {
                            currentItem = upArrow(currentItem, i, SurveyChunk.ShotCompassRole)
                        } else if(columnIndex === SurveyChunk.ShotBackClinoRole && backsight.checked && frontsight.checked) {
                            currentItem = upArrow(currentItem, i, SurveyChunk.ShotClinoRole)
                        }

                        if(currentItem.dataValue.indexInChunk === firstShotIndex) {
                            if(i === 2) {
                                break;
                            }
                            let nextIndex = currentItem.listViewIndex - 4;
                            currentItem = null;
                            currentItem = upArrow(currentItem, nextIndex, columnIndex);
                            i -= 2;
                            continue;
                        }
                        currentItem = upArrow(currentItem, i - 2, columnIndex);
                        continue;
                    }
                    currentItem = upArrow(currentItem, i - 2, columnIndex);
                }
            }

            function testRow(row, column, direction, testBacksights = true, startWithBacksights = false) {
                let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view");
                view.positionViewAtIndex(row, ListView.Contain)


                let cell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + "." + column)
                mouseClick(cell)

                verify(frontsight.checked === true)
                verify(backsight.checked === false)
                verify(cell.focus === true);

                direction(cell, row, column);

                if(testBacksights) {
                    // ----- Enable the backsights and frontsights ----
                    view.positionViewAtBeginning()
                    wait(100);
                    mouseClick(backsight);

                    verify(frontsight.checked === true)
                    verify(backsight.checked === true)

                    let startColumn = column
                    if(startWithBacksights) {
                        if(column === SurveyChunk.ShotCompassRole) {
                            startColumn = SurveyChunk.ShotBackCompassRole
                        } else if(column === SurveyChunk.ShotClinoRole) {
                            startColumn = SurveyChunk.ShotBackClinoRole
                        }
                    }

                    view.positionViewAtIndex(row - 1, ListView.Contain)
                    view.positionViewAtIndex(row, ListView.Contain)
                    view.positionViewAtIndex(row + 1, ListView.Contain)
                    wait(50);

                    //We need to click twice because, this could change the number of the cells
                    cell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + "." + startColumn)
                    mouseClick(cell)
                    cell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + "." + startColumn)
                    mouseClick(cell)

                    verify(cell.focus === true);

                    direction(cell, row, startColumn);

                    // ----- Enable the backsights only
                    view.positionViewAtBeginning()
                    wait(50);
                    mouseClick(frontsight);

                    verify(frontsight.checked === false)
                    verify(backsight.checked === true)

                    if(column === SurveyChunk.ShotCompassRole) {
                        column = SurveyChunk.ShotBackCompassRole
                    } else if(column === SurveyChunk.ShotClinoRole) {
                        column = SurveyChunk.ShotBackClinoRole
                    }

                    view.positionViewAtIndex(row + 1, ListView.Contain)
                    wait(50);

                    cell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + "." + column)
                    mouseClick(cell)
                    cell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + "." + column)
                    mouseClick(cell)
                    verify(cell.focus === true);

                    direction(cell, row, column);

                    view.positionViewAtBeginning()
                    wait(50);
                    mouseClick(frontsight);
                    mouseClick(backsight);
                }
            }

            //--- Test the right arrow
            //Test the first row
            testRow(1, SurveyChunk.StationNameRole, rightArrowOnFullRow)

            //Test a middle row
            testRow(5, SurveyChunk.StationNameRole, rightArrowOnFullRow)

            //Test last row
            testRow(9, SurveyChunk.StationNameRole, rightArrowOnFullRow)

            //--- Test the left arrow
            //Test the first row
            testRow(1, SurveyChunk.StationDownRole, leftArrowOnFullRow)

            //Test a middle row
            testRow(5, SurveyChunk.StationDownRole, leftArrowOnFullRow)

            //Test last row
            testRow(9, SurveyChunk.StationDownRole, leftArrowOnFullRow)

            // //Test Down arrow
            let testBacksights = false;
            testRow(1, SurveyChunk.StationNameRole, downArrowOnFullColumn, testBacksights)
            testRow(2, SurveyChunk.ShotDistanceRole, downArrowOnFullColumn, testBacksights)
            testRow(2, SurveyChunk.ShotCompassRole, downArrowOnFullColumn, true)
            testRow(2, SurveyChunk.ShotClinoRole, downArrowOnFullColumn, true)

            testRow(1, SurveyChunk.StationLeftRole, downArrowOnFullColumn, testBacksights)
            testRow(1, SurveyChunk.StationRightRole, downArrowOnFullColumn, testBacksights)
            testRow(1, SurveyChunk.StationUpRole, downArrowOnFullColumn, testBacksights)
            testRow(1, SurveyChunk.StationDownRole, downArrowOnFullColumn, testBacksights)

            //Test Up downArrow

            function scrollToBottom(row) {
                //Scroll to the bottom
                let surveyView = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view")
                surveyView.positionViewAtEnd()
                wait(100)
                // waitForRendering(rootId)

                let cell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + "." + SurveyChunk.StationNameRole)
                mouseClick(cell)
            }

            let lastStation = 21;
            let startWithBacksights = true
            scrollToBottom(lastStation);
            testRow(lastStation, SurveyChunk.StationNameRole, upArrowOnFullColumn, testBacksights)
            scrollToBottom(lastStation);
            testRow(lastStation - 1, SurveyChunk.ShotDistanceRole, upArrowOnFullColumn, testBacksights)
            scrollToBottom(lastStation);
            testRow(lastStation - 1, SurveyChunk.ShotCompassRole, upArrowOnFullColumn, true, startWithBacksights)
            scrollToBottom(lastStation);
            testRow(lastStation - 1, SurveyChunk.ShotClinoRole, upArrowOnFullColumn, true, startWithBacksights)

            scrollToBottom(lastStation);
            testRow(lastStation, SurveyChunk.StationLeftRole, upArrowOnFullColumn, testBacksights)
            scrollToBottom(lastStation);
            testRow(lastStation, SurveyChunk.StationRightRole, upArrowOnFullColumn, testBacksights)
            scrollToBottom(lastStation);
            testRow(lastStation, SurveyChunk.StationUpRole, upArrowOnFullColumn, testBacksights)
            scrollToBottom(lastStation);
            testRow(lastStation, SurveyChunk.StationDownRole, upArrowOnFullColumn, testBacksights)

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

            function tabTestSingleSight(frontsight) {

                let compassRole = frontsight ? SurveyChunk.ShotCompassRole : SurveyChunk.ShotBackCompassRole
                let clinoRole = frontsight ? SurveyChunk.ShotClinoRole : SurveyChunk.ShotBackClinoRole

                currentItem = nextTab(currentItem, 3, SurveyChunk.StationNameRole);
                currentItem = nextTab(currentItem, 2, SurveyChunk.ShotDistanceRole);
                currentItem = nextTab(currentItem, 2, compassRole);
                currentItem = nextTab(currentItem, 2, clinoRole);
                currentItem = nextTab(currentItem, 1, SurveyChunk.StationLeftRole);
                currentItem = nextTab(currentItem, 1, SurveyChunk.StationRightRole);
                currentItem = nextTab(currentItem, 1, SurveyChunk.StationUpRole);
                currentItem = nextTab(currentItem, 1, SurveyChunk.StationDownRole);
                currentItem = nextTab(currentItem, 3, SurveyChunk.StationLeftRole);
                currentItem = nextTab(currentItem, 3, SurveyChunk.StationRightRole);
                currentItem = nextTab(currentItem, 3, SurveyChunk.StationUpRole);
                currentItem = nextTab(currentItem, 3, SurveyChunk.StationDownRole);

                currentItem = nextTab(currentItem, 5, SurveyChunk.StationNameRole);
                currentItem = nextTab(currentItem, 4, SurveyChunk.ShotDistanceRole);
                currentItem = nextTab(currentItem, 4, compassRole);
                currentItem = nextTab(currentItem, 4, clinoRole);
                currentItem = nextTab(currentItem, 5, SurveyChunk.StationLeftRole);
                currentItem = nextTab(currentItem, 5, SurveyChunk.StationRightRole);
                currentItem = nextTab(currentItem, 5, SurveyChunk.StationUpRole);
                currentItem = nextTab(currentItem, 5, SurveyChunk.StationDownRole);

                currentItem = nextTab(currentItem, 7, SurveyChunk.StationNameRole);
                currentItem = nextTab(currentItem, 6, SurveyChunk.ShotDistanceRole);
                currentItem = nextTab(currentItem, 6, compassRole);
                currentItem = nextTab(currentItem, 6, clinoRole);
                currentItem = nextTab(currentItem, 7, SurveyChunk.StationLeftRole);
                currentItem = nextTab(currentItem, 7, SurveyChunk.StationRightRole);
                currentItem = nextTab(currentItem, 7, SurveyChunk.StationUpRole);
                currentItem = nextTab(currentItem, 7, SurveyChunk.StationDownRole);

                currentItem = nextTab(currentItem, 9, SurveyChunk.StationNameRole);
                currentItem = downArrow(null, 9, SurveyChunk.StationNameRole)

                currentItem = nextTab(null, 11, SurveyChunk.StationNameRole);
                currentItem = nextTab(currentItem, 10, SurveyChunk.ShotDistanceRole);
                currentItem = nextTab(currentItem, 10, compassRole);
                currentItem = nextTab(currentItem, 10, clinoRole);
                currentItem = nextTab(currentItem, 9, SurveyChunk.StationLeftRole);
                currentItem = nextTab(currentItem, 9, SurveyChunk.StationRightRole);
                currentItem = nextTab(currentItem, 9, SurveyChunk.StationUpRole);
                currentItem = nextTab(currentItem, 9, SurveyChunk.StationDownRole);
                currentItem = nextTab(currentItem, 11, SurveyChunk.StationLeftRole);
                currentItem = nextTab(currentItem, 11, SurveyChunk.StationRightRole);
                currentItem = nextTab(currentItem, 11, SurveyChunk.StationUpRole);
                currentItem = nextTab(currentItem, 11, SurveyChunk.StationDownRole);

                currentItem = nextTab(currentItem, 13, SurveyChunk.StationNameRole);
                currentItem = downArrow(null, 13, SurveyChunk.StationNameRole)

                currentItem = nextTab(null, 15, SurveyChunk.StationNameRole);
                currentItem = nextTab(currentItem, 14, SurveyChunk.ShotDistanceRole);
                currentItem = nextTab(currentItem, 14, compassRole);
                currentItem = nextTab(currentItem, 14, clinoRole);
                currentItem = nextTab(currentItem, 13, SurveyChunk.StationLeftRole);
                currentItem = nextTab(currentItem, 13, SurveyChunk.StationRightRole);
                currentItem = nextTab(currentItem, 13, SurveyChunk.StationUpRole);
                currentItem = nextTab(currentItem, 13, SurveyChunk.StationDownRole);
                currentItem = nextTab(currentItem, 15, SurveyChunk.StationLeftRole);
                currentItem = nextTab(currentItem, 15, SurveyChunk.StationRightRole);
                currentItem = nextTab(currentItem, 15, SurveyChunk.StationUpRole);
                currentItem = nextTab(currentItem, 15, SurveyChunk.StationDownRole);

                currentItem = nextTab(currentItem, 17, SurveyChunk.StationNameRole);
                currentItem = nextTab(currentItem, 16, SurveyChunk.ShotDistanceRole);
                currentItem = nextTab(currentItem, 16, compassRole);
                currentItem = nextTab(currentItem, 16, clinoRole);
                currentItem = nextTab(currentItem, 17, SurveyChunk.StationLeftRole);
                currentItem = nextTab(currentItem, 17, SurveyChunk.StationRightRole);
                currentItem = nextTab(currentItem, 17, SurveyChunk.StationUpRole);
                currentItem = nextTab(currentItem, 17, SurveyChunk.StationDownRole);

                currentItem = nextTab(currentItem, 19, SurveyChunk.StationNameRole);
                currentItem = nextTab(currentItem, 18, SurveyChunk.ShotDistanceRole);
                currentItem = nextTab(currentItem, 18, compassRole);
                currentItem = nextTab(currentItem, 18, clinoRole);
                currentItem = nextTab(currentItem, 19, SurveyChunk.StationLeftRole);
                currentItem = nextTab(currentItem, 19, SurveyChunk.StationRightRole);
                currentItem = nextTab(currentItem, 19, SurveyChunk.StationUpRole);
                currentItem = nextTab(currentItem, 19, SurveyChunk.StationDownRole);

                currentItem = nextTab(currentItem, 21, SurveyChunk.StationNameRole);

                //Reverse
                currentItem = previousTab(currentItem, 19, SurveyChunk.StationDownRole);
                currentItem = previousTab(currentItem, 19, SurveyChunk.StationUpRole);
                currentItem = previousTab(currentItem, 19, SurveyChunk.StationRightRole);
                currentItem = previousTab(currentItem, 19, SurveyChunk.StationLeftRole);
                currentItem = previousTab(currentItem, 18, clinoRole);
                currentItem = previousTab(currentItem, 18, compassRole);
                currentItem = previousTab(currentItem, 18, SurveyChunk.ShotDistanceRole);

                currentItem = previousTab(currentItem, 19, SurveyChunk.StationNameRole);
                currentItem = previousTab(currentItem, 17, SurveyChunk.StationDownRole);
                currentItem = previousTab(currentItem, 17, SurveyChunk.StationUpRole);
                currentItem = previousTab(currentItem, 17, SurveyChunk.StationRightRole);
                currentItem = previousTab(currentItem, 17, SurveyChunk.StationLeftRole);
                currentItem = previousTab(currentItem, 16, clinoRole);
                currentItem = previousTab(currentItem, 16, compassRole);
                currentItem = previousTab(currentItem, 16, SurveyChunk.ShotDistanceRole);

                currentItem = previousTab(currentItem, 17, SurveyChunk.StationNameRole);
                currentItem = previousTab(currentItem, 15, SurveyChunk.StationDownRole);
                currentItem = previousTab(currentItem, 15, SurveyChunk.StationUpRole);
                currentItem = previousTab(currentItem, 15, SurveyChunk.StationRightRole);
                currentItem = previousTab(currentItem, 15, SurveyChunk.StationLeftRole);
                currentItem = previousTab(currentItem, 13, SurveyChunk.StationDownRole);
                currentItem = previousTab(currentItem, 13, SurveyChunk.StationUpRole);
                currentItem = previousTab(currentItem, 13, SurveyChunk.StationRightRole);
                currentItem = previousTab(currentItem, 13, SurveyChunk.StationLeftRole);
                currentItem = previousTab(currentItem, 14, clinoRole);
                currentItem = previousTab(currentItem, 14, compassRole);
                currentItem = previousTab(currentItem, 14, SurveyChunk.ShotDistanceRole);
                currentItem = previousTab(currentItem, 15, SurveyChunk.StationNameRole);

                currentItem = upArrow(null, 13, SurveyChunk.StationNameRole);

                currentItem = previousTab(currentItem, 11, SurveyChunk.StationDownRole);
                currentItem = previousTab(currentItem, 11, SurveyChunk.StationUpRole);
                currentItem = previousTab(currentItem, 11, SurveyChunk.StationRightRole);
                currentItem = previousTab(currentItem, 11, SurveyChunk.StationLeftRole);
                currentItem = previousTab(currentItem, 9, SurveyChunk.StationDownRole);
                currentItem = previousTab(currentItem, 9, SurveyChunk.StationUpRole);
                currentItem = previousTab(currentItem, 9, SurveyChunk.StationRightRole);
                currentItem = previousTab(currentItem, 9, SurveyChunk.StationLeftRole);
                currentItem = previousTab(currentItem, 10, clinoRole);
                currentItem = previousTab(currentItem, 10, compassRole);
                currentItem = previousTab(currentItem, 10, SurveyChunk.ShotDistanceRole);
                currentItem = previousTab(currentItem, 11, SurveyChunk.StationNameRole);

                currentItem = upArrow(null, 9, SurveyChunk.StationNameRole);

                currentItem = previousTab(currentItem, 7, SurveyChunk.StationDownRole);
                currentItem = previousTab(currentItem, 7, SurveyChunk.StationUpRole);
                currentItem = previousTab(currentItem, 7, SurveyChunk.StationRightRole);
                currentItem = previousTab(currentItem, 7, SurveyChunk.StationLeftRole);
                currentItem = previousTab(currentItem, 6, clinoRole);
                currentItem = previousTab(currentItem, 6, compassRole);
                currentItem = previousTab(currentItem, 6, SurveyChunk.ShotDistanceRole);
                currentItem = previousTab(currentItem, 7, SurveyChunk.StationNameRole);

                currentItem = previousTab(currentItem, 5, SurveyChunk.StationDownRole);
                currentItem = previousTab(currentItem, 5, SurveyChunk.StationUpRole);
                currentItem = previousTab(currentItem, 5, SurveyChunk.StationRightRole);
                currentItem = previousTab(currentItem, 5, SurveyChunk.StationLeftRole);
                currentItem = previousTab(currentItem, 4, clinoRole);
                currentItem = previousTab(currentItem, 4, compassRole);
                currentItem = previousTab(currentItem, 4, SurveyChunk.ShotDistanceRole);
                currentItem = previousTab(currentItem, 5, SurveyChunk.StationNameRole);

                currentItem = previousTab(currentItem, 3, SurveyChunk.StationDownRole);
                currentItem = previousTab(currentItem, 3, SurveyChunk.StationUpRole);
                currentItem = previousTab(currentItem, 3, SurveyChunk.StationRightRole);
                currentItem = previousTab(currentItem, 3, SurveyChunk.StationLeftRole);
                currentItem = previousTab(currentItem, 1, SurveyChunk.StationDownRole);
                currentItem = previousTab(currentItem, 1, SurveyChunk.StationUpRole);
                currentItem = previousTab(currentItem, 1, SurveyChunk.StationRightRole);
                currentItem = previousTab(currentItem, 1, SurveyChunk.StationLeftRole);
                currentItem = previousTab(currentItem, 2, clinoRole);
                currentItem = previousTab(currentItem, 2, compassRole);
                currentItem = previousTab(currentItem, 2, SurveyChunk.ShotDistanceRole);
                currentItem = previousTab(currentItem, 3, SurveyChunk.StationNameRole);
                currentItem = previousTab(currentItem, 1, SurveyChunk.StationNameRole);
            }

            function tabTestFrontAndBackSight() {
                currentItem = nextTab(currentItem, 3, SurveyChunk.StationNameRole);
                currentItem = nextTab(currentItem, 2, SurveyChunk.ShotDistanceRole);
                currentItem = nextTab(currentItem, 2, SurveyChunk.ShotCompassRole);
                currentItem = nextTab(currentItem, 2, SurveyChunk.ShotBackCompassRole);
                currentItem = nextTab(currentItem, 2, SurveyChunk.ShotClinoRole);
                currentItem = nextTab(currentItem, 2, SurveyChunk.ShotBackClinoRole);
                currentItem = nextTab(currentItem, 1, SurveyChunk.StationLeftRole);
                currentItem = nextTab(currentItem, 1, SurveyChunk.StationRightRole);
                currentItem = nextTab(currentItem, 1, SurveyChunk.StationUpRole);
                currentItem = nextTab(currentItem, 1, SurveyChunk.StationDownRole);
                currentItem = nextTab(currentItem, 3, SurveyChunk.StationLeftRole);
                currentItem = nextTab(currentItem, 3, SurveyChunk.StationRightRole);
                currentItem = nextTab(currentItem, 3, SurveyChunk.StationUpRole);
                currentItem = nextTab(currentItem, 3, SurveyChunk.StationDownRole);

                currentItem = nextTab(currentItem, 5, SurveyChunk.StationNameRole);
                currentItem = nextTab(currentItem, 4, SurveyChunk.ShotDistanceRole);
                currentItem = nextTab(currentItem, 4, SurveyChunk.ShotCompassRole);
                currentItem = nextTab(currentItem, 4, SurveyChunk.ShotBackCompassRole);
                currentItem = nextTab(currentItem, 4, SurveyChunk.ShotClinoRole);
                currentItem = nextTab(currentItem, 4, SurveyChunk.ShotBackClinoRole);
                currentItem = nextTab(currentItem, 5, SurveyChunk.StationLeftRole);
                currentItem = nextTab(currentItem, 5, SurveyChunk.StationRightRole);
                currentItem = nextTab(currentItem, 5, SurveyChunk.StationUpRole);
                currentItem = nextTab(currentItem, 5, SurveyChunk.StationDownRole);

                currentItem = nextTab(currentItem, 7, SurveyChunk.StationNameRole);
                currentItem = nextTab(currentItem, 6, SurveyChunk.ShotDistanceRole);
                currentItem = nextTab(currentItem, 6, SurveyChunk.ShotCompassRole);
                currentItem = nextTab(currentItem, 6, SurveyChunk.ShotBackCompassRole);
                currentItem = nextTab(currentItem, 6, SurveyChunk.ShotClinoRole);
                currentItem = nextTab(currentItem, 6, SurveyChunk.ShotBackClinoRole);
                currentItem = nextTab(currentItem, 7, SurveyChunk.StationLeftRole);
                currentItem = nextTab(currentItem, 7, SurveyChunk.StationRightRole);
                currentItem = nextTab(currentItem, 7, SurveyChunk.StationUpRole);
                currentItem = nextTab(currentItem, 7, SurveyChunk.StationDownRole);

                currentItem = nextTab(currentItem, 9, SurveyChunk.StationNameRole);
                currentItem = downArrow(null, 9, SurveyChunk.StationNameRole)

                currentItem = nextTab(null, 11, SurveyChunk.StationNameRole);
                currentItem = nextTab(currentItem, 10, SurveyChunk.ShotDistanceRole);
                currentItem = nextTab(currentItem, 10, SurveyChunk.ShotCompassRole);
                currentItem = nextTab(currentItem, 10, SurveyChunk.ShotBackCompassRole);
                currentItem = nextTab(currentItem, 10, SurveyChunk.ShotClinoRole);
                currentItem = nextTab(currentItem, 10, SurveyChunk.ShotBackClinoRole);
                currentItem = nextTab(currentItem, 9, SurveyChunk.StationLeftRole);
                currentItem = nextTab(currentItem, 9, SurveyChunk.StationRightRole);
                currentItem = nextTab(currentItem, 9, SurveyChunk.StationUpRole);
                currentItem = nextTab(currentItem, 9, SurveyChunk.StationDownRole);
                currentItem = nextTab(currentItem, 11, SurveyChunk.StationLeftRole);
                currentItem = nextTab(currentItem, 11, SurveyChunk.StationRightRole);
                currentItem = nextTab(currentItem, 11, SurveyChunk.StationUpRole);
                currentItem = nextTab(currentItem, 11, SurveyChunk.StationDownRole);

                currentItem = nextTab(currentItem, 13, SurveyChunk.StationNameRole);
                currentItem = downArrow(null, 13, SurveyChunk.StationNameRole)

                currentItem = nextTab(null, 15, SurveyChunk.StationNameRole);
                currentItem = nextTab(currentItem, 14, SurveyChunk.ShotDistanceRole);
                currentItem = nextTab(currentItem, 14, SurveyChunk.ShotCompassRole);
                currentItem = nextTab(currentItem, 14, SurveyChunk.ShotBackCompassRole);
                currentItem = nextTab(currentItem, 14, SurveyChunk.ShotClinoRole);
                currentItem = nextTab(currentItem, 14, SurveyChunk.ShotBackClinoRole);
                currentItem = nextTab(currentItem, 13, SurveyChunk.StationLeftRole);
                currentItem = nextTab(currentItem, 13, SurveyChunk.StationRightRole);
                currentItem = nextTab(currentItem, 13, SurveyChunk.StationUpRole);
                currentItem = nextTab(currentItem, 13, SurveyChunk.StationDownRole);
                currentItem = nextTab(currentItem, 15, SurveyChunk.StationLeftRole);
                currentItem = nextTab(currentItem, 15, SurveyChunk.StationRightRole);
                currentItem = nextTab(currentItem, 15, SurveyChunk.StationUpRole);
                currentItem = nextTab(currentItem, 15, SurveyChunk.StationDownRole);

                currentItem = nextTab(currentItem, 17, SurveyChunk.StationNameRole);
                currentItem = nextTab(currentItem, 16, SurveyChunk.ShotDistanceRole);
                currentItem = nextTab(currentItem, 16, SurveyChunk.ShotCompassRole);
                currentItem = nextTab(currentItem, 16, SurveyChunk.ShotBackCompassRole);
                currentItem = nextTab(currentItem, 16, SurveyChunk.ShotClinoRole);
                currentItem = nextTab(currentItem, 16, SurveyChunk.ShotBackClinoRole);
                currentItem = nextTab(currentItem, 17, SurveyChunk.StationLeftRole);
                currentItem = nextTab(currentItem, 17, SurveyChunk.StationRightRole);
                currentItem = nextTab(currentItem, 17, SurveyChunk.StationUpRole);
                currentItem = nextTab(currentItem, 17, SurveyChunk.StationDownRole);

                currentItem = nextTab(currentItem, 19, SurveyChunk.StationNameRole);
                currentItem = nextTab(currentItem, 18, SurveyChunk.ShotDistanceRole);
                currentItem = nextTab(currentItem, 18, SurveyChunk.ShotCompassRole);
                currentItem = nextTab(currentItem, 18, SurveyChunk.ShotBackCompassRole);
                currentItem = nextTab(currentItem, 18, SurveyChunk.ShotClinoRole);
                currentItem = nextTab(currentItem, 18, SurveyChunk.ShotBackClinoRole);
                currentItem = nextTab(currentItem, 19, SurveyChunk.StationLeftRole);
                currentItem = nextTab(currentItem, 19, SurveyChunk.StationRightRole);
                currentItem = nextTab(currentItem, 19, SurveyChunk.StationUpRole);
                currentItem = nextTab(currentItem, 19, SurveyChunk.StationDownRole);

                currentItem = nextTab(currentItem, 21, SurveyChunk.StationNameRole);

                //Reverse
                currentItem = previousTab(currentItem, 19, SurveyChunk.StationDownRole);
                currentItem = previousTab(currentItem, 19, SurveyChunk.StationUpRole);
                currentItem = previousTab(currentItem, 19, SurveyChunk.StationRightRole);
                currentItem = previousTab(currentItem, 19, SurveyChunk.StationLeftRole);
                currentItem = previousTab(currentItem, 18, SurveyChunk.ShotBackClinoRole);
                currentItem = previousTab(currentItem, 18, SurveyChunk.ShotClinoRole);
                currentItem = previousTab(currentItem, 18, SurveyChunk.ShotBackCompassRole);
                currentItem = previousTab(currentItem, 18, SurveyChunk.ShotCompassRole);
                currentItem = previousTab(currentItem, 18, SurveyChunk.ShotDistanceRole);

                currentItem = previousTab(currentItem, 19, SurveyChunk.StationNameRole);
                currentItem = previousTab(currentItem, 17, SurveyChunk.StationDownRole);
                currentItem = previousTab(currentItem, 17, SurveyChunk.StationUpRole);
                currentItem = previousTab(currentItem, 17, SurveyChunk.StationRightRole);
                currentItem = previousTab(currentItem, 17, SurveyChunk.StationLeftRole);
                currentItem = previousTab(currentItem, 16, SurveyChunk.ShotBackClinoRole);
                currentItem = previousTab(currentItem, 16, SurveyChunk.ShotClinoRole);
                currentItem = previousTab(currentItem, 16, SurveyChunk.ShotBackCompassRole);
                currentItem = previousTab(currentItem, 16, SurveyChunk.ShotCompassRole);
                currentItem = previousTab(currentItem, 16, SurveyChunk.ShotDistanceRole);

                currentItem = previousTab(currentItem, 17, SurveyChunk.StationNameRole);
                currentItem = previousTab(currentItem, 15, SurveyChunk.StationDownRole);
                currentItem = previousTab(currentItem, 15, SurveyChunk.StationUpRole);
                currentItem = previousTab(currentItem, 15, SurveyChunk.StationRightRole);
                currentItem = previousTab(currentItem, 15, SurveyChunk.StationLeftRole);
                currentItem = previousTab(currentItem, 13, SurveyChunk.StationDownRole);
                currentItem = previousTab(currentItem, 13, SurveyChunk.StationUpRole);
                currentItem = previousTab(currentItem, 13, SurveyChunk.StationRightRole);
                currentItem = previousTab(currentItem, 13, SurveyChunk.StationLeftRole);
                currentItem = previousTab(currentItem, 14, SurveyChunk.ShotBackClinoRole);
                currentItem = previousTab(currentItem, 14, SurveyChunk.ShotClinoRole);
                currentItem = previousTab(currentItem, 14, SurveyChunk.ShotBackCompassRole);
                currentItem = previousTab(currentItem, 14, SurveyChunk.ShotCompassRole);
                currentItem = previousTab(currentItem, 14, SurveyChunk.ShotDistanceRole);
                currentItem = previousTab(currentItem, 15, SurveyChunk.StationNameRole);

                currentItem = upArrow(null, 13, SurveyChunk.StationNameRole);

                currentItem = previousTab(currentItem, 11, SurveyChunk.StationDownRole);
                currentItem = previousTab(currentItem, 11, SurveyChunk.StationUpRole);
                currentItem = previousTab(currentItem, 11, SurveyChunk.StationRightRole);
                currentItem = previousTab(currentItem, 11, SurveyChunk.StationLeftRole);
                currentItem = previousTab(currentItem, 9, SurveyChunk.StationDownRole);
                currentItem = previousTab(currentItem, 9, SurveyChunk.StationUpRole);
                currentItem = previousTab(currentItem, 9, SurveyChunk.StationRightRole);
                currentItem = previousTab(currentItem, 9, SurveyChunk.StationLeftRole);
                currentItem = previousTab(currentItem, 10, SurveyChunk.ShotBackClinoRole);
                currentItem = previousTab(currentItem, 10, SurveyChunk.ShotClinoRole);
                currentItem = previousTab(currentItem, 10, SurveyChunk.ShotBackCompassRole);
                currentItem = previousTab(currentItem, 10, SurveyChunk.ShotCompassRole);
                currentItem = previousTab(currentItem, 10, SurveyChunk.ShotDistanceRole);
                currentItem = previousTab(currentItem, 11, SurveyChunk.StationNameRole);

                currentItem = upArrow(null, 9, SurveyChunk.StationNameRole);

                currentItem = previousTab(currentItem, 7, SurveyChunk.StationDownRole);
                currentItem = previousTab(currentItem, 7, SurveyChunk.StationUpRole);
                currentItem = previousTab(currentItem, 7, SurveyChunk.StationRightRole);
                currentItem = previousTab(currentItem, 7, SurveyChunk.StationLeftRole);
                currentItem = previousTab(currentItem, 6, SurveyChunk.ShotBackClinoRole);
                currentItem = previousTab(currentItem, 6, SurveyChunk.ShotClinoRole);
                currentItem = previousTab(currentItem, 6, SurveyChunk.ShotBackCompassRole);
                currentItem = previousTab(currentItem, 6, SurveyChunk.ShotCompassRole);
                currentItem = previousTab(currentItem, 6, SurveyChunk.ShotDistanceRole);
                currentItem = previousTab(currentItem, 7, SurveyChunk.StationNameRole);

                currentItem = previousTab(currentItem, 5, SurveyChunk.StationDownRole);
                currentItem = previousTab(currentItem, 5, SurveyChunk.StationUpRole);
                currentItem = previousTab(currentItem, 5, SurveyChunk.StationRightRole);
                currentItem = previousTab(currentItem, 5, SurveyChunk.StationLeftRole);
                currentItem = previousTab(currentItem, 4, SurveyChunk.ShotBackClinoRole);
                currentItem = previousTab(currentItem, 4, SurveyChunk.ShotClinoRole);
                currentItem = previousTab(currentItem, 4, SurveyChunk.ShotBackCompassRole);
                currentItem = previousTab(currentItem, 4, SurveyChunk.ShotCompassRole);
                currentItem = previousTab(currentItem, 4, SurveyChunk.ShotDistanceRole);
                currentItem = previousTab(currentItem, 5, SurveyChunk.StationNameRole);

                currentItem = previousTab(currentItem, 3, SurveyChunk.StationDownRole);
                currentItem = previousTab(currentItem, 3, SurveyChunk.StationUpRole);
                currentItem = previousTab(currentItem, 3, SurveyChunk.StationRightRole);
                currentItem = previousTab(currentItem, 3, SurveyChunk.StationLeftRole);
                currentItem = previousTab(currentItem, 1, SurveyChunk.StationDownRole);
                currentItem = previousTab(currentItem, 1, SurveyChunk.StationUpRole);
                currentItem = previousTab(currentItem, 1, SurveyChunk.StationRightRole);
                currentItem = previousTab(currentItem, 1, SurveyChunk.StationLeftRole);
                currentItem = previousTab(currentItem, 2, SurveyChunk.ShotBackClinoRole);
                currentItem = previousTab(currentItem, 2, SurveyChunk.ShotClinoRole);
                currentItem = previousTab(currentItem, 2, SurveyChunk.ShotBackCompassRole);
                currentItem = previousTab(currentItem, 2, SurveyChunk.ShotCompassRole);
                currentItem = previousTab(currentItem, 2, SurveyChunk.ShotDistanceRole);
                currentItem = previousTab(currentItem, 3, SurveyChunk.StationNameRole);
                currentItem = previousTab(currentItem, 1, SurveyChunk.StationNameRole);
            }

            let frontsight = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->frontSightCalibrationEditor->checkBox")
            let backsight = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->backSightCalibrationEditor->checkBox")

            verify(frontsight.checked === true)
            verify(backsight.checked === false)

            tabTestSingleSight(true)

            //Scroll to the top
            surveyView.positionViewAtBeginning();
            waitForRendering(surveyView)

            // ----- Enable the backsights ----
            mouseClick(backsight);

            verify(frontsight.checked === true)
            verify(backsight.checked === true)

            tabTestFrontAndBackSight();

            // ----- Backsights only ----
            //Scroll to the top
            surveyView.positionViewAtBeginning();
            waitForRendering(surveyView)

            mouseClick(frontsight);

            verify(frontsight.checked === false)
            verify(backsight.checked === true)

            tabTestSingleSight(false);
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
