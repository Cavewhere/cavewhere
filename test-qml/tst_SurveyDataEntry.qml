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

        function init() {
            RootData.project.newProject()
            RootData.pageSelectionModel.currentPageAddress = "View"
            wait(100)
        }

        function addSurvey() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwScrapManager/ProjectProfile-test-v3.cw");
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Cave 1"

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
            let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view")
            verify(view !== null)
            let editorModel = view.model
            verify(editorModel !== null)

            // wait(100000);


            let firstChunk = trip.chunk(0);
            verify(firstChunk.stationCount === 2)
            verify(firstChunk.data(SurveyChunk.StationNameRole, 0) === "b1")
            verify(firstChunk.data(SurveyChunk.StationNameRole, 1) === "b2")

            // Virtual trailing station is exposed by the model while this chunk is focused.
            let virtualStationRow = editorModel.toModelRow(
                        editorModel.rowIndex(firstChunk, 2, SurveyEditorRowIndex.StationRow))
            verify(virtualStationRow >= 0)
            compare(editorModel.toModelRow(
                        editorModel.rowIndex(firstChunk, 3, SurveyEditorRowIndex.StationRow)), -1)

            let virtualStationIndex = editorModel.index(virtualStationRow, 0)
            let virtualStationData = editorModel.data(virtualStationIndex, SurveyEditorModel.StationNameRole)
            verify(virtualStationData.reading.value === "")

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
            let firstChunkVirtualStationRowNow = editorModel.toModelRow(
                        editorModel.rowIndex(firstChunk, 2, SurveyEditorRowIndex.StationRow))
            verify(firstChunkVirtualStationRowNow >= 0)
            view.positionViewAtIndex(firstChunkVirtualStationRowNow, ListView.Contain)
            waitForRendering(rootId)
            let firstChunkVirtualStationBox = null
            tryVerify(() => {
                          firstChunkVirtualStationBox = ObjectFinder.findObjectByChain(
                                      rootId.mainWindow,
                                      "rootId->tripPage->view->dataBox." + firstChunkVirtualStationRowNow + ".0")
                          return firstChunkVirtualStationBox !== null
                      })
            let firstChunkVirtualStationInput = ObjectFinder.findObjectByChain(
                        rootId.mainWindow,
                        "rootId->tripPage->view->dataBox." + firstChunkVirtualStationRowNow + ".0->coreTextInput")
            if(firstChunkVirtualStationInput !== null) {
                mouseClick(firstChunkVirtualStationInput)
            } else {
                mouseClick(firstChunkVirtualStationBox)
            }
            firstChunkVirtualStationBox.forceActiveFocus()
            tryVerify(() => { return firstChunkVirtualStationBox.focus === true })
            verifySingleDataBoxSelection("a1 virtual station selected")
            keyClick("a")
            keyClick(49, 0) //1
            keyClick(16777217, 0) //Tab
            tryVerify(() => { return firstChunk.data(SurveyChunk.StationNameRole, 2) === "a1" })
            verifySingleDataBoxSelection("after entering a1")
            waitForRendering(rootId)


            //Distance of 1
            keyClick(49, 0) //1
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.ShotDistanceRole, 1).value === "1")
            verifySingleDataBoxSelection("after entering a1 distance")
            waitForRendering(rootId)

            //Compasss of 10
            keyClick(49, 0) //1
            keyClick(48, 0) //0
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.ShotCompassRole, 1).value === "10")
            verifySingleDataBoxSelection("after entering a1 compass")
            waitForRendering(rootId)

            //Skip backcompass
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.ShotBackCompassRole, 1).value === "")
            verifySingleDataBoxSelection("after a1 back compass skip")

            //Clino of 0
            keyClick(48, 0) //0
            keyClick(16777217, 0) //Tab
            verify(firstChunk.data(SurveyChunk.ShotClinoRole, 1).value === "0")
            verifySingleDataBoxSelection("after entering a1 clino")
            waitForRendering(rootId)

            //make sure the secondChunk chunk doesn't exist before we create it
            verify(trip.chunkCount === 1);

            // wait(1000000)

            //Make a new chunk
            keyClick(32, 0) //Space
            verify(firstChunk)
            tryVerify(() => { return trip.chunkCount === 2 });
            tryVerify(() => {
                          return editorModel.data(editorModel.index(6, 0), SurveyEditorModel.RowTypeRole) === SurveyEditorRowIndex.TitleRow
                      })
            tryVerify(() => {
                          return editorModel.data(editorModel.index(7, 0), SurveyEditorModel.RowTypeRole) === SurveyEditorRowIndex.StationRow
                      })
            tryVerify(() => { return editorModel.focusedRow === 7 })
            tryVerify(() => { return editorModel.focusedRole === SurveyChunk.StationNameRole })
            view.positionViewAtIndex(6, ListView.Contain)
            waitForRendering(rootId)
            tryVerify(() => { return view.itemAtIndex(6) !== null })
            compare(ObjectFinder.findObjectByChain(
                        rootId.mainWindow,
                        "rootId->tripPage->view->dataBox.6.0"), null)
            let secondChunkStationRowBox = null
            tryVerify(() => {
                          secondChunkStationRowBox = ObjectFinder.findObjectByChain(
                                      rootId.mainWindow,
                                      "rootId->tripPage->view->dataBox.7.0")
                          return secondChunkStationRowBox !== null
                      })
            tryVerify(() => { return secondChunkStationRowBox.focus === true })
            verifySingleDataBoxSelection("after creating second chunk")

            //Make sure focus is on the secondChunk
            let secondChunk = trip.chunk(1);
            verify(secondChunk !== null);
            let focusedSecondChunkStation0Row = editorModel.toModelRow(
                        editorModel.rowIndex(secondChunk, 0, SurveyEditorRowIndex.StationRow))
            compare(focusedSecondChunkStation0Row, 7)
            let stationBox = ObjectFinder.findObjectByChain(
                        rootId.mainWindow,
                        "rootId->tripPage->view->dataBox." + focusedSecondChunkStation0Row + ".0")
            verify(stationBox !== null)
            tryVerify(() => { return stationBox.focus === true; })
            verify(stationBox === mainWindow.Window.window.activeFocusItem)

            //Make sure the first chunk gets trimmed
            compare(firstChunk.stationCount, 3)
            compare(firstChunk.shotCount, 2)

            //Make sure the new scrap exists
            wait(50)

            //Add data
            let secondChunkStation0Row = editorModel.toModelRow(
                        editorModel.rowIndex(secondChunk, 0, SurveyEditorRowIndex.StationRow))
            verify(secondChunkStation0Row >= 0)
            view.positionViewAtIndex(secondChunkStation0Row, ListView.Contain)
            waitForRendering(rootId)
            let secondChunkStation0Box = null
            tryVerify(() => {
                          secondChunkStation0Box = ObjectFinder.findObjectByChain(
                                      rootId.mainWindow,
                                      "rootId->tripPage->view->dataBox." + secondChunkStation0Row + ".0")
                          return secondChunkStation0Box !== null
                      })
            mouseClick(secondChunkStation0Box)
            keyClick("a")
            keyClick(49, 0) //1
            keyClick(16777217, 0) //Tab
            tryVerify(() => {return secondChunk.data(SurveyChunk.StationNameRole, 0) === "a1"})

            //Auto guess next row
            keyClick(16777217, 0) //Tab
            verify(secondChunk.data(SurveyChunk.StationNameRole, 1) === "a2")
            let secondChunkVirtualShotRow = editorModel.toModelRow(
                        editorModel.rowIndex(secondChunk, secondChunk.shotCount, SurveyEditorRowIndex.ShotRow))
            verify(secondChunkVirtualShotRow >= 0)
            compare(editorModel.shotDistanceIncludedAt(
                        editorModel.cellIndex(secondChunkVirtualShotRow,
                                              SurveyChunk.ShotDistanceRole)), true)

            waitForRendering(rootId);

            // wait(1000000)

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

        function focusedDataBoxObjectName() {
            let activeItem = rootId.Window.window.activeFocusItem
            let depth = 0
            while(activeItem !== null && activeItem !== undefined && depth < 20) {
                if(activeItem.objectName !== undefined
                        && activeItem.objectName !== null
                        && activeItem.objectName.indexOf("dataBox.") === 0)
                {
                    return activeItem.objectName
                }
                activeItem = activeItem.parent
                depth += 1
            }
            return ""
        }

        function collectDataBoxes(item, boxes) {
            if(item === null || item === undefined) {
                return
            }

            if(item.objectName !== undefined
                    && item.objectName !== null
                    && item.objectName.indexOf("dataBox.") === 0)
            {
                boxes.push(item)
            }

            let children = item.children
            if(children === undefined || children === null) {
                return
            }
            for(let i = 0; i < children.length; ++i) {
                collectDataBoxes(children[i], boxes)
            }
        }

        function selectedDataBoxObjectNames() {
            let boxes = []
            collectDataBoxes(rootId.mainWindow, boxes)

            let selected = []
            for(let i = 0; i < boxes.length; ++i) {
                let box = boxes[i]
                let editorFocused = false
                if(box.shouldHaveFocus !== undefined) {
                    editorFocused = box.shouldHaveFocus()
                } else if(box.hasEditorFocus !== undefined) {
                    editorFocused = box.hasEditorFocus === true
                }
                if(editorFocused) {
                    selected.push(box.objectName)
                }
            }
            return selected
        }

        function verifySingleDataBoxSelection(stepName) {
            waitForRendering(rootId)
            let selected = selectedDataBoxObjectNames()
            compare(selected.length, 1, stepName + " selected boxes: " + selected.join(", "))
        }

        function navHelper(currentItem,
                               index,
                               nextRole,
                               key,
                               modifier)
            {
                if(currentItem !== null
                        && currentItem !== undefined
                        && currentItem.model !== undefined
                        && currentItem.dataValue !== undefined)
                {
                    currentItem.model.setFocusedCell(
                                currentItem.model.cellIndex(currentItem.listViewIndex,
                                                            currentItem.dataValue.chunkDataRole))
                }

                keyClick(key, modifier)

                let itemName = "rootId->tripPage->view->dataBox." + index + "." + nextRole
                let item = null
                let focusedName = ""
                tryVerify(() => {
                              item = ObjectFinder.findObjectByChain(mainWindow, itemName)
                              focusedName = focusedDataBoxObjectName()
                              return item !== null || focusedName.length > 0
                          })

                if(item === null && focusedName.length > 0) {
                    item = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->" + focusedName)
                }
                verify(item !== null)

                focusedName = focusedDataBoxObjectName()
                if(focusedName.length > 0) {
                    let focusedItem = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->" + focusedName)
                    if(focusedItem !== null) {
                        item = focusedItem
                    }
                }

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

            function navHelperByBoxIndex(currentItem, nextBoxIndex, key, modifier, model) {
                keyClick(key, modifier)

                let row = model.toModelRow(nextBoxIndex.rowIndex)
                let itemName = "rootId->tripPage->view->dataBox." + row + "." + nextBoxIndex.chunkDataRole
                let item = null
                let focusedName = ""
                tryVerify(() => {
                              item = ObjectFinder.findObjectByChain(mainWindow, itemName)
                              focusedName = focusedDataBoxObjectName()
                              return item !== null || focusedName.length > 0
                          })

                if(item === null && focusedName.length > 0) {
                    item = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->" + focusedName)
                }
                verify(item !== null)

                focusedName = focusedDataBoxObjectName()
                if(focusedName.length > 0) {
                    let focusedItem = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->" + focusedName)
                    if(focusedItem !== null) {
                        item = focusedItem
                    }
                }

                return item;
            }

            function downArrowByBoxIndex(currentItem, nextBoxIndex, model) {
                let downArrow = 16777237;
                return navHelperByBoxIndex(currentItem, nextBoxIndex, downArrow, 0, model);
            }

            function upArrowByBoxIndex(currentItem, nextBoxIndex, model) {
                let upArrow = 16777235;
                return navHelperByBoxIndex(currentItem, nextBoxIndex, upArrow, 0, model);
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

        function test_enterSurveyData_fiveStationsFourShots_cannotContinueEditing() {
            addSurvey();

            let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view")
            verify(view !== null)
            let editorModel = view.model
            verify(editorModel !== null)

            function logEditorSnapshot(tag, chunk) {
                let stationRows = []
                for(let i = 0; i <= chunk.stationCount; ++i) {
                    let stationRowIndex = editorModel.rowIndex(chunk, i, SurveyEditorRowIndex.StationRow)
                    stationRows.push("s" + i + "->" + editorModel.toModelRow(stationRowIndex))
                }

                let shotRows = []
                for(let i = 0; i <= chunk.shotCount; ++i) {
                    let shotRowIndex = editorModel.rowIndex(chunk, i, SurveyEditorRowIndex.ShotRow)
                    shotRows.push("sh" + i + "->" + editorModel.toModelRow(shotRowIndex))
                }

                console.log("[SurveyDataEntryDebug]", tag,
                            "stationCount=", chunk.stationCount,
                            "shotCount=", chunk.shotCount,
                            "stationRows=[" + stationRows.join(", ") + "]",
                            "shotRows=[" + shotRows.join(", ") + "]")
            }

            function logRowCells(row, roles) {
                let present = []
                for(let i = 0; i < roles.length; ++i) {
                    let role = roles[i]
                    let objectName = "rootId->tripPage->view->dataBox." + row + "." + role
                    let cell = ObjectFinder.findObjectByChain(mainWindow, objectName)
                    if(cell !== null) {
                        present.push(role)
                    }
                }
                console.log("[SurveyDataEntryDebug]",
                            "row=", row,
                            "presentRoles=[" + present.join(", ") + "]")
            }

            function dataBoxAt(row, column) {
                view.positionViewAtIndex(row, ListView.Contain)
                waitForRendering(rootId)
                let cell = null
                for(let attempt = 0; attempt < 50; ++attempt) {
                    cell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + "." + column)
                    if(cell !== null) {
                        break
                    }
                    wait(100)
                }
                if(cell === null) {
                    console.log("[SurveyDataEntryDebug]", "Missing dataBox", "row=", row, "column=", column)
                    let activeTrip = RootData.pageView.currentPageItem.currentTrip as Trip
                    if(activeTrip !== null && activeTrip.chunkCount > 0) {
                        let activeChunk = activeTrip.chunk(0)
                        if(activeChunk !== null) {
                            logEditorSnapshot("missing-cell-state", activeChunk)
                        }
                    }
                    logRowCells(row, [
                                    SurveyChunk.StationNameRole,
                                    SurveyChunk.ShotDistanceRole,
                                    SurveyChunk.ShotCompassRole,
                                    SurveyChunk.ShotBackCompassRole,
                                    SurveyChunk.ShotClinoRole,
                                    SurveyChunk.ShotBackClinoRole,
                                    SurveyChunk.StationLeftRole,
                                    SurveyChunk.StationRightRole,
                                    SurveyChunk.StationUpRole,
                                    SurveyChunk.StationDownRole
                                ])
                }
                verify(cell !== null, "Missing dataBox at row=" + row + " column=" + column)
                mouseClick(cell)
                return cell
            }

            function keyClickText(text) {
                for(let i = 0; i < text.length; i++) {
                    let ch = text[i]
                    if(ch >= "0" && ch <= "9") {
                        keyClick(ch.charCodeAt(0), 0)
                    } else if(ch === ".") {
                        keyClick(46, 0)
                    } else if(ch === "-") {
                        keyClick(45, 0)
                    } else {
                        keyClick(ch)
                    }
                }
            }

            function setCell(row, column, text) {
                console.log("[SurveyDataEntryDebug]", "setCell", "row=", row, "column=", column, "text=", text)
                dataBoxAt(row, column)
                keyClickText(text)
                keyClick(16777220, 0) //Return
                waitForRendering(rootId)
            }

            // Fill 5 stations and 4 shots (all data fields) in one chunk.
            for(let stationIndex = 0; stationIndex < 5; stationIndex++) {
                let stationRow = 1 + (stationIndex * 2)
                let stationNumber = stationIndex + 1

                setCell(stationRow, SurveyChunk.StationNameRole, "a" + stationNumber)
                setCell(stationRow, SurveyChunk.StationLeftRole, "" + stationNumber)
                setCell(stationRow, SurveyChunk.StationRightRole, "" + (stationNumber + 10))
                setCell(stationRow, SurveyChunk.StationUpRole, "" + (stationNumber + 20))
                setCell(stationRow, SurveyChunk.StationDownRole, "" + (stationNumber + 30))

                if(stationIndex < 4) {
                    let shotRow = stationRow + 1
                    let shotNumber = stationIndex + 1
                    setCell(shotRow, SurveyChunk.ShotDistanceRole, "" + (shotNumber * 10))
                    setCell(shotRow, SurveyChunk.ShotCompassRole, "" + (shotNumber * 20))
                    setCell(shotRow, SurveyChunk.ShotBackCompassRole, "" + (shotNumber * 20 + 180))
                    setCell(shotRow, SurveyChunk.ShotClinoRole, "" + shotNumber)
                    setCell(shotRow, SurveyChunk.ShotBackClinoRole, "" + (-shotNumber))
                }
            }

            let trip = RootData.pageView.currentPageItem.currentTrip as Trip
            verify(trip !== null)
            compare(trip.chunkCount, 1)
            let chunk = trip.chunk(0)
            verify(chunk !== null)
            compare(chunk.stationCount, 5)
            compare(chunk.shotCount, 4)
            logEditorSnapshot("after full fill", chunk)
            logRowCells(8, [
                            SurveyChunk.ShotDistanceRole,
                            SurveyChunk.ShotCompassRole,
                            SurveyChunk.ShotBackCompassRole,
                            SurveyChunk.ShotClinoRole,
                            SurveyChunk.ShotBackClinoRole
                        ])

            // Repro step: with 5 stations/4 shots already filled, editing should still work.
            // Current bug: entry stops working once this size is reached.
            setCell(8, SurveyChunk.ShotDistanceRole, "99")
            compare(chunk.data(SurveyChunk.ShotDistanceRole, 3).value, "99")

            setCell(9, SurveyChunk.StationDownRole, "55")
            compare(chunk.data(SurveyChunk.StationDownRole, 4).value, "55")

            // Keep model in view and validate trailing virtual rows still resolve.
            let virtualShotRow = editorModel.toModelRow(
                        editorModel.rowIndex(chunk, chunk.shotCount, SurveyEditorRowIndex.ShotRow))
            let virtualStationRow = editorModel.toModelRow(
                        editorModel.rowIndex(chunk, chunk.stationCount, SurveyEditorRowIndex.StationRow))
            verify(virtualShotRow >= 0)
            verify(virtualStationRow >= 0)
        }

        function test_tabFromFourthStation_shouldGoToThirdShot() {
            addSurvey();

            let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view")
            verify(view !== null)
            let editorModel = view.model
            verify(editorModel !== null)

            function dataBoxAt(row, column) {
                view.positionViewAtIndex(row, ListView.Contain)
                waitForRendering(rootId)
                let cell = ObjectFinder.findObjectByChain(
                            mainWindow,
                            "rootId->tripPage->view->dataBox." + row + "." + column)
                verify(cell !== null, "Missing dataBox at row=" + row + " column=" + column)
                return cell
            }

            function keyClickText(text) {
                for(let i = 0; i < text.length; i++) {
                    let ch = text[i]
                    if(ch >= "0" && ch <= "9") {
                        keyClick(ch.charCodeAt(0), 0)
                    } else {
                        keyClick(ch)
                    }
                }
            }

            function setCell(row, column, text) {
                let cell = dataBoxAt(row, column)
                mouseClick(cell)
                keyClickText(text)
                keyClick(16777220, 0) // Return
                waitForRendering(rootId)
            }

            let trip = RootData.pageView.currentPageItem.currentTrip as Trip
            verify(trip !== null)
            compare(trip.chunkCount, 1)
            let chunk = trip.chunk(0)
            verify(chunk !== null)

            // Seed 3 station names.
            for(let stationIndex = 0; stationIndex < 3; ++stationIndex) {
                let stationRow = editorModel.toModelRow(
                            editorModel.rowIndex(chunk, stationIndex, SurveyEditorRowIndex.StationRow))
                verify(stationRow >= 0)
                setCell(stationRow, SurveyChunk.StationNameRole, "a" + (stationIndex + 1))
            }

            // Fill only 2 shots to match repro.
            for(let shotIndex = 0; shotIndex < 2; ++shotIndex) {
                let shotRow = editorModel.toModelRow(
                            editorModel.rowIndex(chunk, shotIndex, SurveyEditorRowIndex.ShotRow))
                verify(shotRow >= 0)
                setCell(shotRow, SurveyChunk.ShotDistanceRole, "" + (shotIndex + 1))
            }

            compare(chunk.stationCount, 3)
            compare(chunk.shotCount, 2)
            compare(chunk.data(SurveyChunk.ShotDistanceRole, 0).value, "1")
            compare(chunk.data(SurveyChunk.ShotDistanceRole, 1).value, "2")

            // Station 4 starts as virtual.
            let station4Row = editorModel.toModelRow(
                        editorModel.rowIndex(chunk, chunk.stationCount, SurveyEditorRowIndex.StationRow))
            verify(station4Row >= 0)

            let station4Box = dataBoxAt(station4Row, SurveyChunk.StationNameRole)
            mouseClick(station4Box)
            tryVerify(() => { return focusedDataBoxObjectName() === "dataBox." + station4Row + "." + SurveyChunk.StationNameRole })

            // Repro path: type station 4 into the virtual station row then Tab.
            keyClick("a")
            keyClick("4")
            keyClick(16777217, 0) // Tab

            let shot3Row = editorModel.toModelRow(
                        editorModel.rowIndex(chunk, 2, SurveyEditorRowIndex.ShotRow))
            verify(shot3Row >= 0)
            let expectedFocus = "dataBox." + shot3Row + "." + SurveyChunk.ShotDistanceRole
            tryVerify(() => { return focusedDataBoxObjectName() === expectedFocus })
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

            tryVerify( () => {return chunkError.visible === true });
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


            // wait(1000000)
            //The error is in a async loader, so we need to wait for it to be created
            tryVerify(() => { return ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->view->dataBox.1.1->coreTextInput->errorIcon") !== null })

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

            //Since error icon is in a loader, the item is deleted
            errorIcon_obj1 = ObjectFinder.findObjectByChain(rootId.mainWindow, "rootId->tripPage->view->dataBox.1.1->coreTextInput->errorIcon")

            //Make sure the warning message that was shown before is now hidden
            verify(errorIcon_obj1 === null)

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

        function test_missingStationNameAfterClearingShotData() {
            addSurvey();

            let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view");
            let frontSight = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->frontSightCalibrationEditor->checkBox")
            let backSight = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->backSightCalibrationEditor->checkBox")

            verify(view !== null)
            verify(frontSight !== null)
            verify(backSight !== null)

            if(!frontSight.checked) {
                mouseClick(frontSight)
            }
            if(!backSight.checked) {
                mouseClick(backSight)
            }

            function dataBoxAt(row, column) {
                view.positionViewAtIndex(row, ListView.Contain)
                waitForRendering(rootId)
                let cell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + "." + column)
                verify(cell !== null)
                mouseClick(cell)
                return cell
            }

            function keyClickText(text) {
                for(let i = 0; i < text.length; i++) {
                    let ch = text[i]
                    if(ch >= "0" && ch <= "9") {
                        keyClick(ch.charCodeAt(0), 0)
                    } else if(ch === ".") {
                        keyClick(46, 0)
                    } else if(ch === "-") {
                        keyClick(45, 0)
                    } else {
                        keyClick(ch)
                    }
                }
            }

            function setCell(row, column, text) {
                dataBoxAt(row, column)
                keyClickText(text)
                keyClick(16777220, 0) //Return
                waitForRendering(rootId)
            }

            function clearCell(row, column) {
                dataBoxAt(row, column)
                keyClick(16777220, 0) //Return
                keyClick(16777223, 0) //Delete
                keyClick(16777220, 0) //Return
                waitForRendering(rootId)

                // wait(1000000)
            }

            // Stations: 0 -> 1 -> 2 -> 3
            setCell(1, SurveyChunk.StationNameRole, "A1")
            setCell(3, SurveyChunk.StationNameRole, "1")
            setCell(5, SurveyChunk.StationNameRole, "2")
            setCell(7, SurveyChunk.StationNameRole, "3")

            // LRUD for each station
            setCell(1, SurveyChunk.StationLeftRole, "1")
            setCell(1, SurveyChunk.StationRightRole, "2")
            setCell(1, SurveyChunk.StationUpRole, "3")
            setCell(1, SurveyChunk.StationDownRole, "4")

            setCell(3, SurveyChunk.StationLeftRole, "1.1")
            setCell(3, SurveyChunk.StationRightRole, "2.1")
            setCell(3, SurveyChunk.StationUpRole, "3.1")
            setCell(3, SurveyChunk.StationDownRole, "4.1")

            setCell(5, SurveyChunk.StationLeftRole, "1.2")
            setCell(5, SurveyChunk.StationRightRole, "2.2")
            setCell(5, SurveyChunk.StationUpRole, "3.2")
            setCell(5, SurveyChunk.StationDownRole, "4.2")

            setCell(7, SurveyChunk.StationLeftRole, "1.3")
            setCell(7, SurveyChunk.StationRightRole, "2.3")
            setCell(7, SurveyChunk.StationUpRole, "3.3")
            setCell(7, SurveyChunk.StationDownRole, "4.3")

            // Shot 0
            setCell(2, SurveyChunk.ShotDistanceRole, "10")
            setCell(2, SurveyChunk.ShotCompassRole, "10")
            setCell(2, SurveyChunk.ShotBackCompassRole, "190")
            setCell(2, SurveyChunk.ShotClinoRole, "5")
            setCell(2, SurveyChunk.ShotBackClinoRole, "-5")

            // Shot 1
            setCell(4, SurveyChunk.ShotDistanceRole, "11")
            setCell(4, SurveyChunk.ShotCompassRole, "20")
            setCell(4, SurveyChunk.ShotBackCompassRole, "200")
            setCell(4, SurveyChunk.ShotClinoRole, "6")
            setCell(4, SurveyChunk.ShotBackClinoRole, "-6")

            // Shot 2
            setCell(6, SurveyChunk.ShotDistanceRole, "12")
            setCell(6, SurveyChunk.ShotCompassRole, "30")
            setCell(6, SurveyChunk.ShotBackCompassRole, "210")
            setCell(6, SurveyChunk.ShotClinoRole, "7")
            setCell(6, SurveyChunk.ShotBackClinoRole, "-7")

            let trip = RootData.pageView.currentPageItem.currentTrip as Trip
            verify(trip !== null)
            verify(trip.errorModel !== null)
            let chunk = trip.chunk(0)
            verify(chunk !== null)
            verify(chunk.errorModel !== null)

            tryVerify(() => { return chunk.errorModel.fatalCount === 0 && chunk.errorModel.warningCount === 0 })
            tryVerify(() => { return trip.errorModel.fatalCount === 0 && trip.errorModel.warningCount === 0 })

            // wait(100000)

            // Clear first shot front readings and remove station 1 name.
            clearCell(2, SurveyChunk.ShotDistanceRole)
            clearCell(2, SurveyChunk.ShotCompassRole)
            clearCell(2, SurveyChunk.ShotClinoRole)
            clearCell(2, SurveyChunk.ShotBackCompassRole)
            clearCell(2, SurveyChunk.ShotBackClinoRole)
            clearCell(3, SurveyChunk.StationNameRole)

            let station1Cell = dataBoxAt(3, SurveyChunk.StationNameRole)
            verify(station1Cell.errorModel !== null)
            tryVerify(() => { return station1Cell.errorModel.fatalCount === 1 })
            verify(station1Cell.errorModel.warningCount === 0)
            tryVerify(() => { return chunk.errorModel.fatalCount === 1 && chunk.errorModel.warningCount === 0 })
            tryVerify(() => { return trip.errorModel.fatalCount === 1 && trip.errorModel.warningCount === 0 })

            let errors = station1Cell.errorModel.errors
            verify(errors.count === 1)
            let errorIndex = errors.index(0, 0)
            verify(errors.data(errorIndex, ErrorListModel.MessageRole) === "Missing station name")
            verify(errors.data(errorIndex, ErrorListModel.ErrorTypeRole) === CwError.Fatal)
        }

        function test_insertRemoveStationsAndShots() {
            addSurvey();

            let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view");
            verify(view !== null)
            let editorModel = view.model
            verify(editorModel !== null)

            function dataBoxAt(row, column) {
                view.positionViewAtIndex(row, ListView.Contain)
                waitForRendering(rootId)
                let cell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + "." + column)
                verify(cell !== null)
                mouseClick(cell)
                return cell
            }

            function keyClickText(text) {
                for(let i = 0; i < text.length; i++) {
                    let ch = text[i]
                    if(ch >= "0" && ch <= "9") {
                        keyClick(ch.charCodeAt(0), 0)
                    } else if(ch === ".") {
                        keyClick(46, 0)
                    } else if(ch === "-") {
                        keyClick(45, 0)
                    } else {
                        keyClick(ch)
                    }
                }
            }

            function setCell(row, column, text) {
                dataBoxAt(row, column)
                keyClickText(text)
                keyClick(16777220, 0) //Return
                waitForRendering(rootId)
            }

            function openContextMenu(row, column) {
                let cell = dataBoxAt(row, column)
                mouseClick(cell, cell.width / 2, cell.height / 2, Qt.RightButton)
                waitForRendering(rootId)
            }

            function triggerMenuItemFromLoader(row, column, loaderObjectName, itemObjectName) {
                let loaderPath = "rootId->tripPage->view->dataBox." + row + "." + column + "->" + loaderObjectName
                let loader = ObjectFinder.findObjectByChain(mainWindow, loaderPath)
                verify(loader !== null)
                verify(loader.item !== null)

                let item = findChild(loader.item, itemObjectName)
                verify(item !== null)
                item.triggered()
                waitForRendering(rootId)
            }

            let trip = RootData.pageView.currentPageItem.currentTrip as Trip
            verify(trip !== null)
            let chunk = trip.chunk(0)
            verify(chunk !== null)
            function verifyFocusedCounts(realStationCount, realShotCount) {
                compare(chunk.stationCount, realStationCount)
                compare(chunk.shotCount, realShotCount)
                verify(editorModel.toModelRow(
                           editorModel.rowIndex(chunk, realStationCount, SurveyEditorRowIndex.StationRow)) >= 0)
                verify(editorModel.toModelRow(
                           editorModel.rowIndex(chunk, realShotCount, SurveyEditorRowIndex.ShotRow)) >= 0)
            }

            // Base data: A0 -> A1 -> A2, two shots
            setCell(1, SurveyChunk.StationNameRole, "A0")
            setCell(3, SurveyChunk.StationNameRole, "A1")
            setCell(5, SurveyChunk.StationNameRole, "A2")
            setCell(2, SurveyChunk.ShotDistanceRole, "10")
            setCell(4, SurveyChunk.ShotDistanceRole, "20")


            verifyFocusedCounts(3, 2)
            verify(chunk.data(SurveyChunk.StationNameRole, 0) === "A0")
            verify(chunk.data(SurveyChunk.StationNameRole, 1) === "A1")
            verify(chunk.data(SurveyChunk.StationNameRole, 2) === "A2")

            // Insert station above A1
            openContextMenu(3, SurveyChunk.StationNameRole)
            wait(100);
            triggerMenuItemFromLoader(3, SurveyChunk.StationNameRole, "stationMenuLoader", "stationMenuInsertAbove")

            verifyFocusedCounts(4, 3)

            verify(chunk.data(SurveyChunk.StationNameRole, 0) === "A0")
            verify(chunk.data(SurveyChunk.StationNameRole, 1) === "")
            verify(chunk.data(SurveyChunk.StationNameRole, 2) === "A1")
            verify(chunk.data(SurveyChunk.StationNameRole, 3) === "A2")

            // Edit inserted station/shot
            setCell(3, SurveyChunk.StationNameRole, "B0")
            setCell(4, SurveyChunk.ShotDistanceRole, "15")

            // Remove inserted station and shot below
            openContextMenu(3, SurveyChunk.StationNameRole)
            triggerMenuItemFromLoader(3, SurveyChunk.StationNameRole, "stationMenuLoader", "stationMenuRemoveBelow")

            verifyFocusedCounts(3, 2)
            verify(chunk.data(SurveyChunk.StationNameRole, 0) === "A0")
            verify(chunk.data(SurveyChunk.StationNameRole, 1) === "A1")
            verify(chunk.data(SurveyChunk.StationNameRole, 2) === "A2")

            // Insert station below A1
            openContextMenu(3, SurveyChunk.StationNameRole)
            triggerMenuItemFromLoader(3, SurveyChunk.StationNameRole, "stationMenuLoader", "stationMenuInsertBelow")

            verifyFocusedCounts(4, 3)
            verify(chunk.data(SurveyChunk.StationNameRole, 0) === "A0")
            verify(chunk.data(SurveyChunk.StationNameRole, 1) === "A1")
            verify(chunk.data(SurveyChunk.StationNameRole, 2) === "")
            verify(chunk.data(SurveyChunk.StationNameRole, 3) === "A2")

            setCell(5, SurveyChunk.StationNameRole, "B1")
            setCell(6, SurveyChunk.ShotDistanceRole, "25")

            openContextMenu(5, SurveyChunk.StationNameRole)
            triggerMenuItemFromLoader(5, SurveyChunk.StationNameRole, "stationMenuLoader", "stationMenuRemoveAbove")

            verifyFocusedCounts(3, 2)
            verify(chunk.data(SurveyChunk.StationNameRole, 0) === "A0")
            verify(chunk.data(SurveyChunk.StationNameRole, 1) === "A1")
            verify(chunk.data(SurveyChunk.StationNameRole, 2) === "A2")

            // Insert shot below shot 0
            openContextMenu(2, SurveyChunk.ShotDistanceRole)
            triggerMenuItemFromLoader(2, SurveyChunk.ShotDistanceRole, "shotMenuLoader", "shotMenuInsertBelow")

            verifyFocusedCounts(4, 3)
            verify(chunk.data(SurveyChunk.StationNameRole, 1) === "")
            setCell(3, SurveyChunk.StationNameRole, "C0")
            setCell(4, SurveyChunk.ShotDistanceRole, "12")

            openContextMenu(4, SurveyChunk.ShotDistanceRole)
            triggerMenuItemFromLoader(4, SurveyChunk.ShotDistanceRole, "shotMenuLoader", "shotMenuRemoveAbove")

            verifyFocusedCounts(3, 2)
            verify(chunk.data(SurveyChunk.StationNameRole, 0) === "A0")
            verify(chunk.data(SurveyChunk.StationNameRole, 1) === "A1")
            verify(chunk.data(SurveyChunk.StationNameRole, 2) === "A2")

            // Insert shot above shot 1
            openContextMenu(4, SurveyChunk.ShotDistanceRole)
            triggerMenuItemFromLoader(4, SurveyChunk.ShotDistanceRole, "shotMenuLoader", "shotMenuInsertAbove")

            verifyFocusedCounts(4, 3)
            verify(chunk.data(SurveyChunk.StationNameRole, 2) === "")
            setCell(5, SurveyChunk.StationNameRole, "C1")
            setCell(4, SurveyChunk.ShotDistanceRole, "18")

            openContextMenu(4, SurveyChunk.ShotDistanceRole)
            triggerMenuItemFromLoader(4, SurveyChunk.ShotDistanceRole, "shotMenuLoader", "shotMenuRemoveBelow")

            verifyFocusedCounts(3, 2)
            verify(chunk.data(SurveyChunk.StationNameRole, 0) === "A0")
            verify(chunk.data(SurveyChunk.StationNameRole, 1) === "A1")
            verify(chunk.data(SurveyChunk.StationNameRole, 2) === "A2")
        }

        function test_insertAboveRowOneTwice() {
            addSurvey();

            let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view");
            verify(view !== null)

            function dataBoxAt(row, column) {
                view.positionViewAtIndex(row, ListView.Contain)
                waitForRendering(rootId)
                let cell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + "." + column)
                verify(cell !== null)
                mouseClick(cell)
                return cell
            }

            function keyClickText(text) {
                for(let i = 0; i < text.length; i++) {
                    let ch = text[i]
                    if(ch >= "0" && ch <= "9") {
                        keyClick(ch.charCodeAt(0), 0)
                    } else {
                        keyClick(ch)
                    }
                }
            }

            function setCell(row, column, text) {
                dataBoxAt(row, column)
                keyClickText(text)
                keyClick(16777220, 0) //Return
                waitForRendering(rootId)
            }

            function openContextMenu(row, column) {
                let cell = dataBoxAt(row, column)
                mouseClick(cell, cell.width / 2, cell.height / 2, Qt.RightButton)
                waitForRendering(rootId)
            }

            function triggerMenuItemFromLoader(row, column, loaderObjectName, itemObjectName) {
                let loaderPath = "rootId->tripPage->view->dataBox." + row + "." + column + "->" + loaderObjectName
                let loader = ObjectFinder.findObjectByChain(mainWindow, loaderPath)
                verify(loader !== null)
                verify(loader.item !== null)

                let item = findChild(loader.item, itemObjectName)
                verify(item !== null)
                item.triggered()
                waitForRendering(rootId)
            }

            let trip = RootData.pageView.currentPageItem.currentTrip as Trip
            verify(trip !== null)
            let chunk = trip.chunk(0)
            verify(chunk !== null)

            // Base data: A0 -> A1 -> A2
            setCell(1, SurveyChunk.StationNameRole, "A0")
            setCell(3, SurveyChunk.StationNameRole, "A1")
            setCell(5, SurveyChunk.StationNameRole, "A2")
            setCell(2, SurveyChunk.ShotDistanceRole, "10")
            setCell(4, SurveyChunk.ShotDistanceRole, "20")

            // Insert above row 1
            openContextMenu(1, SurveyChunk.StationNameRole)
            triggerMenuItemFromLoader(1, SurveyChunk.StationNameRole, "stationMenuLoader", "stationMenuInsertAbove")

            // Edit inserted station name
            setCell(1, SurveyChunk.StationNameRole, "B0")

            // Insert above row 2 again using same loader
            openContextMenu(3, SurveyChunk.StationNameRole)
            triggerMenuItemFromLoader(3, SurveyChunk.StationNameRole, "stationMenuLoader", "stationMenuInsertAbove")

            // Edit the newest inserted station name
            setCell(3, SurveyChunk.StationNameRole, "C0")

            // Verify station order
            verify(chunk.data(SurveyChunk.StationNameRole, 0) === "B0")
            verify(chunk.data(SurveyChunk.StationNameRole, 1) === "C0")
            verify(chunk.data(SurveyChunk.StationNameRole, 2) === "A0")
            verify(chunk.data(SurveyChunk.StationNameRole, 3) === "A1")
            verify(chunk.data(SurveyChunk.StationNameRole, 4) === "A2")

            // Only one cell highlighted (focused)
            let otherCell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox.1." + SurveyChunk.StationNameRole)
            let focusedCell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox.3." + SurveyChunk.StationNameRole)
            verify(focusedCell !== null)
            verify(otherCell !== null)
            verify(focusedCell.focus === true)
            verify(otherCell.focus === false)
            verify(mainWindow.Window.window.activeFocusItem === focusedCell)
        }

        function test_insertBelowOnEmptyChunk() {
            addSurvey();

            let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view");
            verify(view !== null)

            function dataBoxAt(row, column) {
                view.positionViewAtIndex(row, ListView.Contain)
                waitForRendering(rootId)
                let cell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + "." + column)
                verify(cell !== null)
                mouseClick(cell)
                return cell
            }

            function openContextMenu(row, column) {
                let cell = dataBoxAt(row, column)
                mouseClick(cell, cell.width / 2, cell.height / 2, Qt.RightButton)
                waitForRendering(rootId)
            }

            function triggerMenuItemFromLoader(row, column, loaderObjectName, itemObjectName) {
                let loaderPath = "rootId->tripPage->view->dataBox." + row + "." + column + "->" + loaderObjectName
                let loader = ObjectFinder.findObjectByChain(mainWindow, loaderPath)
                verify(loader !== null)
                verify(loader.item !== null)

                let item = findChild(loader.item, itemObjectName)
                verify(item !== null)
                item.triggered()
                waitForRendering(rootId)
            }

            let trip = RootData.pageView.currentPageItem.currentTrip as Trip
            verify(trip !== null)
            let chunk = trip.chunk(0)
            verify(chunk !== null)

            // Default survey editor should start with a single empty chunk.
            verify(chunk.stationCount >= 1)

            // Right click on the first station cell and insert below (currently aborts).
            openContextMenu(3, SurveyChunk.StationNameRole)
            triggerMenuItemFromLoader(3, SurveyChunk.StationNameRole, "stationMenuLoader", "stationMenuInsertBelow")

            // If the abort is fixed, we should have another empty station row.
            verify(chunk.stationCount === 3)

                    verify(chunk.data(SurveyChunk.StationNameRole, 0) === "")
                    verify(chunk.data(SurveyChunk.StationNameRole, 1) === "")
                    verify(chunk.data(SurveyChunk.StationNameRole, 2) === "")
        }

        function test_insertBelowLastStationWithOneShot() {
            addSurvey();

            let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view");
            verify(view !== null)
            let editorModel = view.model
            verify(editorModel !== null)

            function dataBoxAt(row, column) {
                view.positionViewAtIndex(row, ListView.Contain)
                waitForRendering(rootId)
                let cell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + "." + column)
                verify(cell !== null)
                mouseClick(cell)
                return cell
            }

            function keyClickText(text) {
                for(let i = 0; i < text.length; i++) {
                    let ch = text[i]
                    if(ch >= "0" && ch <= "9") {
                        keyClick(ch.charCodeAt(0), 0)
                    } else if(ch === ".") {
                        keyClick(46, 0)
                    } else if(ch === "-") {
                        keyClick(45, 0)
                    } else {
                        keyClick(ch)
                    }
                }
            }

            function setCell(row, column, text) {
                dataBoxAt(row, column)
                keyClickText(text)
                keyClick(16777220, 0) //Return
                waitForRendering(rootId)
            }

            function openContextMenu(row, column) {
                let cell = dataBoxAt(row, column)
                mouseClick(cell, cell.width / 2, cell.height / 2, Qt.RightButton)
                waitForRendering(rootId)
            }

            function triggerMenuItemFromLoader(row, column, loaderObjectName, itemObjectName) {
                let loaderPath = "rootId->tripPage->view->dataBox." + row + "." + column + "->" + loaderObjectName
                let loader = ObjectFinder.findObjectByChain(mainWindow, loaderPath)
                verify(loader !== null)
                verify(loader.item !== null)

                let item = findChild(loader.item, itemObjectName)
                verify(item !== null)
                item.triggered()
                waitForRendering(rootId)
            }

            let trip = RootData.pageView.currentPageItem.currentTrip as Trip
            verify(trip !== null)
            let chunk = trip.chunk(0)
            verify(chunk !== null)

            // Base data: A0 -> A1, one shot
            setCell(1, SurveyChunk.StationNameRole, "A0")
            setCell(3, SurveyChunk.StationNameRole, "A1")
            setCell(2, SurveyChunk.ShotDistanceRole, "10")

            compare(chunk.stationCount, 2)
            compare(chunk.shotCount, 1)

            // Virtual trailing rows should be exposed while focused.
            let baseVirtualStationRow = editorModel.toModelRow(
                        editorModel.rowIndex(chunk, 2, SurveyEditorRowIndex.StationRow))
            let baseVirtualShotRow = editorModel.toModelRow(
                        editorModel.rowIndex(chunk, 1, SurveyEditorRowIndex.ShotRow))
            verify(baseVirtualStationRow >= 0)
            verify(baseVirtualShotRow >= 0)

                    // wait(100000)

            // Insert below the last station
            openContextMenu(3, SurveyChunk.StationNameRole)
            triggerMenuItemFromLoader(3, SurveyChunk.StationNameRole, "stationMenuLoader", "stationMenuInsertBelow")

            wait(100)

            // Inserting below the last non-empty station is blocked while a virtual tail exists.
            compare(chunk.stationCount, 2)
            compare(chunk.shotCount, 1)
            compare(chunk.data(SurveyChunk.StationNameRole, 0), "A0")
            compare(chunk.data(SurveyChunk.StationNameRole, 1), "A1")
            verify(editorModel.toModelRow(
                       editorModel.rowIndex(chunk, 2, SurveyEditorRowIndex.StationRow)) >= 0)
            verify(editorModel.toModelRow(
                       editorModel.rowIndex(chunk, 3, SurveyEditorRowIndex.StationRow)) < 0)
            verify(editorModel.toModelRow(
                       editorModel.rowIndex(chunk, 1, SurveyEditorRowIndex.ShotRow)) >= 0)
            verify(editorModel.toModelRow(
                       editorModel.rowIndex(chunk, 2, SurveyEditorRowIndex.ShotRow)) < 0)
        }

        function test_insertDisabledOnVirtualStationAndShot() {
            addSurvey();

            let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view");
            verify(view !== null)
            let editorModel = view.model
            verify(editorModel !== null)

            function dataBoxAt(row, column) {
                view.positionViewAtIndex(row, ListView.Contain)
                waitForRendering(rootId)
                let cell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + "." + column)
                verify(cell !== null)
                mouseClick(cell)
                return cell
            }

            function keyClickText(text) {
                for(let i = 0; i < text.length; i++) {
                    let ch = text[i]
                    if(ch >= "0" && ch <= "9") {
                        keyClick(ch.charCodeAt(0), 0)
                    } else {
                        keyClick(ch)
                    }
                }
            }

            function setCell(row, column, text) {
                dataBoxAt(row, column)
                keyClickText(text)
                keyClick(16777220, 0) //Return
                waitForRendering(rootId)
            }

            function openContextMenu(row, column) {
                let cell = dataBoxAt(row, column)
                mouseClick(cell, cell.width / 2, cell.height / 2, Qt.RightButton)
                waitForRendering(rootId)
            }

            function menuItemEnabled(row, column, loaderObjectName, itemObjectName) {
                let loaderPath = "rootId->tripPage->view->dataBox." + row + "." + column + "->" + loaderObjectName
                let loader = ObjectFinder.findObjectByChain(mainWindow, loaderPath)
                verify(loader !== null)
                verify(loader.item !== null)
                let item = findChild(loader.item, itemObjectName)
                verify(item !== null)
                return item.enabled
            }

            let trip = RootData.pageView.currentPageItem.currentTrip as Trip
            verify(trip !== null)
            let chunk = trip.chunk(0)
            verify(chunk !== null)

            // Base data: A0 -> A1 with one shot, which should expose one virtual station/shot while focused.
            setCell(1, SurveyChunk.StationNameRole, "A0")
            setCell(3, SurveyChunk.StationNameRole, "A1")
            setCell(2, SurveyChunk.ShotDistanceRole, "10")

            let virtualStationRow = editorModel.toModelRow(
                        editorModel.rowIndex(chunk, chunk.stationCount, SurveyEditorRowIndex.StationRow))
            let virtualShotRow = editorModel.toModelRow(
                        editorModel.rowIndex(chunk, chunk.shotCount, SurveyEditorRowIndex.ShotRow))
            verify(virtualStationRow >= 0)
            verify(virtualShotRow >= 0)

            // Station virtual row: insert above/below should be disabled.
            openContextMenu(virtualStationRow, SurveyChunk.StationNameRole)
            verify(menuItemEnabled(virtualStationRow, SurveyChunk.StationNameRole, "stationMenuLoader", "stationMenuInsertAbove") === false)
            verify(menuItemEnabled(virtualStationRow, SurveyChunk.StationNameRole, "stationMenuLoader", "stationMenuInsertBelow") === false)

            // Shot virtual row: insert above/below should be disabled.
            openContextMenu(virtualShotRow, SurveyChunk.ShotDistanceRole)
            verify(menuItemEnabled(virtualShotRow, SurveyChunk.ShotDistanceRole, "shotMenuLoader", "shotMenuInsertAbove") === false)
            verify(menuItemEnabled(virtualShotRow, SurveyChunk.ShotDistanceRole, "shotMenuLoader", "shotMenuInsertBelow") === false)
        }

        function test_trimAfterInsertShotAboveLastStationAndFocusChange() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwProject/Phake Cave 3000.cw");
            RootData.futureManagerModel.waitForFinished();

            let cave = RootData.region.cave(0)
            verify(cave !== null)
            let tripFromRegion = cave.trip(0)
            verify(tripFromRegion !== null)
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=" + cave.name + "/Trip=" + tripFromRegion.name
            tryVerify(() => { return RootData.pageView.currentPageItem.objectName === "tripPage" })

            let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view");
            verify(view !== null)
            let editorModel = view.model
            verify(editorModel !== null)

            function dataBoxAt(row, column) {
                view.positionViewAtIndex(row, ListView.Contain)
                waitForRendering(rootId)
                let cell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + "." + column)
                verify(cell !== null)
                mouseClick(cell)
                return cell
            }

            function keyClickText(text) {
                for(let i = 0; i < text.length; i++) {
                    let ch = text[i]
                    if(ch >= "0" && ch <= "9") {
                        keyClick(ch.charCodeAt(0), 0)
                    } else {
                        keyClick(ch)
                    }
                }
            }

            function setCell(row, column, text) {
                dataBoxAt(row, column)
                keyClickText(text)
                keyClick(16777220, 0) //Return
                waitForRendering(rootId)
            }

            function clearCell(row, column) {
                dataBoxAt(row, column)
                keyClick(16777220, 0) //Return
                keyClick(16777223, 0) //Delete
                keyClick(16777220, 0) //Return
                waitForRendering(rootId)
            }

            function openContextMenu(row, column) {
                let cell = dataBoxAt(row, column)
                mouseClick(cell, cell.width / 2, cell.height / 2, Qt.RightButton)
                waitForRendering(rootId)
            }

            function triggerMenuItemFromLoader(row, column, loaderObjectName, itemObjectName) {
                let loaderPath = "rootId->tripPage->view->dataBox." + row + "." + column + "->" + loaderObjectName
                let loader = ObjectFinder.findObjectByChain(mainWindow, loaderPath)
                verify(loader !== null)
                verify(loader.item !== null)
                let item = findChild(loader.item, itemObjectName)
                verify(item !== null)
                item.triggered()
                waitForRendering(rootId)
            }

            function findStationRowForChunk(chunk, stationIndex) {
                for(let row = 0; row < editorModel.rowCount(); ++row) {
                    let rowIndex = editorModel.data(editorModel.index(row, 0), SurveyEditorModel.RowIndexRole)
                    if(rowIndex.chunk === chunk
                            && rowIndex.rowType === SurveyEditorRowIndex.StationRow
                            && rowIndex.indexInChunk === stationIndex)
                    {
                        return row
                    }
                }
                return -1
            }

            let trip = RootData.pageView.currentPageItem.currentTrip as Trip
            verify(trip !== null)
            let firstChunk = trip.chunk(0)
            verify(firstChunk !== null)
            let secondChunk = trip.chunk(1)
            verify(secondChunk !== null)

            // Focus first chunk and verify virtual tail is shown.
            let firstChunkStation0Row = editorModel.toModelRow(
                        editorModel.rowIndex(firstChunk, 0, SurveyEditorRowIndex.StationRow))
            verify(firstChunkStation0Row >= 0)
            let firstChunkStation0Box = dataBoxAt(firstChunkStation0Row, SurveyChunk.StationNameRole)
            verify(firstChunkStation0Box.focus === true)

            tryVerify(() => {
                          return editorModel.toModelRow(
                                     editorModel.rowIndex(firstChunk, firstChunk.stationCount, SurveyEditorRowIndex.StationRow)) >= 0
                      })
            tryVerify(() => {
                          return editorModel.toModelRow(
                                     editorModel.rowIndex(firstChunk, firstChunk.shotCount, SurveyEditorRowIndex.ShotRow)) >= 0
                      })

            // Find last non-empty station, insert station above it, then clear moved station data.
            let lastNonEmptyStationIndex = -1
            for(let i = firstChunk.stationCount - 1; i >= 1; --i) {
                if(firstChunk.data(SurveyChunk.StationNameRole, i) !== "") {
                    lastNonEmptyStationIndex = i
                    break
                }
            }
            verify(lastNonEmptyStationIndex >= 1)
            let lastNonEmptyStationName = firstChunk.data(SurveyChunk.StationNameRole, lastNonEmptyStationIndex)
            verify(lastNonEmptyStationName !== "")

            let lastNonEmptyStationRow = editorModel.toModelRow(
                        editorModel.rowIndex(firstChunk, lastNonEmptyStationIndex, SurveyEditorRowIndex.StationRow))
            verify(lastNonEmptyStationRow >= 0)
            openContextMenu(lastNonEmptyStationRow, SurveyChunk.StationNameRole)
            triggerMenuItemFromLoader(lastNonEmptyStationRow, SurveyChunk.StationNameRole, "stationMenuLoader", "stationMenuInsertAbove")

            let movedStationIndex = -1
            for(let i = firstChunk.stationCount - 1; i >= 1; --i) {
                if(firstChunk.data(SurveyChunk.StationNameRole, i) === lastNonEmptyStationName) {
                    movedStationIndex = i
                    break
                }
            }
            verify(movedStationIndex >= 1)

            let movedStationRow = editorModel.toModelRow(
                        editorModel.rowIndex(firstChunk, movedStationIndex, SurveyEditorRowIndex.StationRow))
            verify(movedStationRow >= 0)
            clearCell(movedStationRow, SurveyChunk.StationLeftRole)
            clearCell(movedStationRow, SurveyChunk.StationRightRole)
            clearCell(movedStationRow, SurveyChunk.StationUpRole)
            clearCell(movedStationRow, SurveyChunk.StationDownRole)
            let contentYBeforeNameDelete = view.contentY
            clearCell(movedStationRow, SurveyChunk.StationNameRole)
            let contentYAfterDelete = view.contentY
            if(contentYBeforeNameDelete > 10) {
                verify(contentYAfterDelete > 1)
                verify(view.atYBeginning === false)
            }
            verify(Math.abs(contentYAfterDelete - contentYBeforeNameDelete) < view.height)

            // While still focused on first chunk, there should be exactly one virtual station/shot pair.
            verify(editorModel.toModelRow(
                       editorModel.rowIndex(firstChunk, firstChunk.stationCount, SurveyEditorRowIndex.StationRow)) >= 0)
            verify(editorModel.toModelRow(
                       editorModel.rowIndex(firstChunk, firstChunk.shotCount, SurveyEditorRowIndex.ShotRow)) >= 0)
            compare(editorModel.toModelRow(
                        editorModel.rowIndex(firstChunk, firstChunk.stationCount + 1, SurveyEditorRowIndex.StationRow)), -1)
            compare(editorModel.toModelRow(
                        editorModel.rowIndex(firstChunk, firstChunk.shotCount + 1, SurveyEditorRowIndex.ShotRow)), -1)

            // Focus next chunk.
            let secondChunkStation0Row = findStationRowForChunk(secondChunk, 0)
            verify(secondChunkStation0Row >= 0)
            let secondRowIndexBeforeClick = editorModel.data(
                        editorModel.index(secondChunkStation0Row, 0), SurveyEditorModel.RowIndexRole)
            verify(secondRowIndexBeforeClick.chunk === secondChunk)
            verify(secondRowIndexBeforeClick.rowType === SurveyEditorRowIndex.StationRow)
            verify(secondRowIndexBeforeClick.indexInChunk === 0)

            view.positionViewAtIndex(secondChunkStation0Row, ListView.Contain)
            waitForRendering(rootId)
            let secondChunkStation0Box = null
            tryVerify(() => {
                          secondChunkStation0Box = ObjectFinder.findObjectByChain(
                                      mainWindow,
                                      "rootId->tripPage->view->dataBox." + secondChunkStation0Row + "." + SurveyChunk.StationNameRole)
                          if(secondChunkStation0Box === null) {
                              return false
                          }
                          return secondChunkStation0Box.dataValue.rowIndex.chunk === secondChunk
                                  && secondChunkStation0Box.dataValue.rowIndex.rowType === SurveyEditorRowIndex.StationRow
                                  && secondChunkStation0Box.dataValue.rowIndex.indexInChunk === 0
                      })
            mouseClick(secondChunkStation0Box)
            verify(secondChunkStation0Box.focus === true)

            // First chunk should no longer expose virtual rows after focus moves.
            compare(editorModel.toModelRow(
                        editorModel.rowIndex(firstChunk, firstChunk.stationCount, SurveyEditorRowIndex.StationRow)), -1)
            compare(editorModel.toModelRow(
                        editorModel.rowIndex(firstChunk, firstChunk.shotCount, SurveyEditorRowIndex.ShotRow)), -1)

            // Chunk separation should remain intact and title row for chunk 1 must still exist.
            let firstChunkLastStationRow = editorModel.toModelRow(
                        editorModel.rowIndex(firstChunk, firstChunk.stationCount - 1, SurveyEditorRowIndex.StationRow))
            let secondChunkTitleRow = editorModel.toModelRow(
                        editorModel.rowIndex(secondChunk, -1, SurveyEditorRowIndex.TitleRow))
            let firstChunkTitleRow = editorModel.toModelRow(
                        editorModel.rowIndex(firstChunk, -1, SurveyEditorRowIndex.TitleRow))
            verify(firstChunkLastStationRow >= 0)
            verify(secondChunkTitleRow >= 0)
            verify(firstChunkTitleRow >= 0)
            compare(secondChunkTitleRow, firstChunkLastStationRow + 1)
            compare(secondChunkTitleRow, firstChunkTitleRow + firstChunk.stationCount + firstChunk.shotCount + 1)

            view.positionViewAtIndex(secondChunkTitleRow, ListView.Contain)
            waitForRendering(rootId)
            let secondChunkTitleDelegate = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->chunkErrorDelegate." + secondChunkTitleRow)
            verify(secondChunkTitleDelegate !== null)

            let rowBeforeTitle = editorModel.data(editorModel.index(secondChunkTitleRow - 1, 0), SurveyEditorModel.RowIndexRole)
            let rowAtTitle = editorModel.data(editorModel.index(secondChunkTitleRow, 0), SurveyEditorModel.RowIndexRole)
            let rowAfterTitle = editorModel.data(editorModel.index(secondChunkTitleRow + 1, 0), SurveyEditorModel.RowIndexRole)
            verify(rowBeforeTitle.chunk === firstChunk)
            verify(rowAtTitle.chunk === secondChunk)
            verify(rowAtTitle.rowType === SurveyEditorRowIndex.TitleRow)
            verify(rowAfterTitle.chunk === secondChunk)

            // Focused second chunk should expose exactly one virtual station/shot pair.
            verify(editorModel.toModelRow(
                       editorModel.rowIndex(secondChunk, secondChunk.stationCount, SurveyEditorRowIndex.StationRow)) >= 0)
            verify(editorModel.toModelRow(
                       editorModel.rowIndex(secondChunk, secondChunk.shotCount, SurveyEditorRowIndex.ShotRow)) >= 0)
            compare(editorModel.toModelRow(
                        editorModel.rowIndex(secondChunk, secondChunk.stationCount + 1, SurveyEditorRowIndex.StationRow)), -1)
            compare(editorModel.toModelRow(
                        editorModel.rowIndex(secondChunk, secondChunk.shotCount + 1, SurveyEditorRowIndex.ShotRow)), -1)
        }

        function test_stationNameArrowSwitchesFocusedChunkVirtualRows() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/test_cwProject/Phake Cave 3000.cw");
            RootData.futureManagerModel.waitForFinished();

            let cave = RootData.region.cave(0)
            verify(cave !== null)
            let tripFromRegion = cave.trip(0)
            verify(tripFromRegion !== null)
            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=" + cave.name + "/Trip=" + tripFromRegion.name
            tryVerify(() => { return RootData.pageView.currentPageItem.objectName === "tripPage" })

            let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view")
            verify(view !== null)
            let editorModel = view.model
            verify(editorModel !== null)

            let trip = RootData.pageView.currentPageItem.currentTrip as Trip
            verify(trip !== null)
            verify(trip.chunkCount >= 2)

            wait(1);

            let chunks = []
            let initialRealEmptyTail = []
            for(let i = 0; i < trip.chunkCount; ++i) {
                let chunk = trip.chunk(i)
                verify(chunk !== null)
                chunks.push(chunk)
                initialRealEmptyTail.push(false)
            }

            function clickStationName(row) {
                view.positionViewAtIndex(row, ListView.Contain)
                // waitForRendering(rootId)
                        wait(1);
                let cell = null
                tryVerify(() => {
                              cell = ObjectFinder.findObjectByChain(
                                          mainWindow,
                                          "rootId->tripPage->view->dataBox." + row + "." + SurveyChunk.StationNameRole)
                              return cell !== null
                          })
                mouseClick(cell)
                tryVerify(() => { return editorModel.focusedRow === row
                                      && editorModel.focusedRole === SurveyChunk.StationNameRole })
            }

            function focusedChunkAndType() {
                let focusedRow = editorModel.focusedRow
                verify(focusedRow >= 0)
                let rowIndex = editorModel.data(editorModel.index(focusedRow, 0), SurveyEditorModel.RowIndexRole)
                return rowIndex
            }

            function chunkIndexOf(chunk) {
                for(let i = 0; i < chunks.length; ++i) {
                    if(chunks[i] === chunk) {
                        return i
                    }
                }
                return -1
            }

            function keyName(key) {
                if(key === Qt.Key_Down) {
                    return "Down"
                } else if(key === Qt.Key_Up) {
                    return "Up"
                }
                return "" + key
            }

            function shotValue(data) {
                if(data !== null && data !== undefined && data.value !== undefined) {
                    return data.value
                }
                return data
            }

            function isEmptyValue(value) {
                return value === "" || value === null || value === undefined
            }

            function chunkHasRealEmptyTail(chunk) {
                if(chunk.stationCount <= 0 || chunk.shotCount <= 0) {
                    return false
                }

                let stationIndex = chunk.stationCount - 1
                let shotIndex = chunk.shotCount - 1

                let stationNameEmpty = isEmptyValue(chunk.data(SurveyChunk.StationNameRole, stationIndex))
                let shotDistanceEmpty = isEmptyValue(shotValue(chunk.data(SurveyChunk.ShotDistanceRole, shotIndex)))
                return stationNameEmpty && shotDistanceEmpty
            }

            function verifyFocusedRowInListView() {
                let focusedRow = editorModel.focusedRow
                verify(focusedRow >= 0)
                view.positionViewAtIndex(focusedRow, ListView.Contain)
                waitForRendering(rootId)
                let cell = null
                tryVerify(() => {
                              cell = ObjectFinder.findObjectByChain(
                                          mainWindow,
                                          "rootId->tripPage->view->dataBox." + focusedRow + "." + SurveyChunk.StationNameRole)
                              return cell !== null
                          })
                verify(cell.visible === true)
            }

            function modelRoleForChunkRole(chunkRole) {
                switch(chunkRole) {
                case SurveyChunk.StationNameRole: return SurveyEditorModel.StationNameRole
                case SurveyChunk.StationLeftRole: return SurveyEditorModel.StationLeftRole
                case SurveyChunk.StationRightRole: return SurveyEditorModel.StationRightRole
                case SurveyChunk.StationUpRole: return SurveyEditorModel.StationUpRole
                case SurveyChunk.StationDownRole: return SurveyEditorModel.StationDownRole
                case SurveyChunk.ShotDistanceRole: return SurveyEditorModel.ShotDistanceRole
                case SurveyChunk.ShotCompassRole: return SurveyEditorModel.ShotCompassRole
                case SurveyChunk.ShotBackCompassRole: return SurveyEditorModel.ShotBackCompassRole
                case SurveyChunk.ShotClinoRole: return SurveyEditorModel.ShotClinoRole
                case SurveyChunk.ShotBackClinoRole: return SurveyEditorModel.ShotBackClinoRole
                default: return -1
                }
            }

            function verifyVisibleDataBoxesMatchModel(tag) {
                let boxes = []
                collectDataBoxes(view, boxes)
                for(let i = 0; i < boxes.length; ++i) {
                    let box = boxes[i]
                    if(box === null || box === undefined || box.visible !== true) {
                        continue
                    }

                    let row = box.listViewIndex
                    let chunkRole = box.dataValue.chunkDataRole
                    let modelRole = modelRoleForChunkRole(chunkRole)
                    if(modelRole < 0 || row < 0) {
                        continue
                    }

                    let modelIndex = editorModel.index(row, 0)
                    let modelBoxData = editorModel.data(modelIndex, modelRole)
                    let modelValue = modelBoxData.reading.value
                    let boxValue = box.dataValue.reading.value
                    compare(boxValue,
                            modelValue,
                            tag + " value mismatch row=" + row + " chunkRole=" + chunkRole)
                }
            }

            function verifyOnlyFocusedChunkHasVirtualRows(visibleChunk) {
                for(let i = 0; i < chunks.length; ++i) {
                    let chunk = chunks[i]
                    let virtualStationRow = editorModel.toModelRow(
                                editorModel.rowIndex(chunk, chunk.stationCount, SurveyEditorRowIndex.StationRow))
                    let virtualShotRow = editorModel.toModelRow(
                                editorModel.rowIndex(chunk, chunk.shotCount, SurveyEditorRowIndex.ShotRow))

                    if(chunk === visibleChunk) {
                        let hasVirtualTail = virtualStationRow >= 0 && virtualShotRow >= 0
                        let hasRealEmptyTail = initialRealEmptyTail[i] && chunkHasRealEmptyTail(chunk)
                        verify(hasVirtualTail || hasRealEmptyTail)
                    } else {
                        tryVerify(() => {
                                      return editorModel.toModelRow(
                                                 editorModel.rowIndex(chunk, chunk.stationCount, SurveyEditorRowIndex.StationRow)) === -1
                                  })
                        tryVerify(() => {
                                      return editorModel.toModelRow(
                                                 editorModel.rowIndex(chunk, chunk.shotCount, SurveyEditorRowIndex.ShotRow)) === -1
                                  })
                        if(!initialRealEmptyTail[i]) {
                            compare(chunkHasRealEmptyTail(chunk), false)
                        }
                    }
                }
            }

            function verifyUnfocusedChunksAreClean(focusedChunk, tag) {
                for(let i = 0; i < chunks.length; ++i) {
                    let chunk = chunks[i]
                    if(chunk === focusedChunk) {
                        continue
                    }

                    let virtualStationRow = editorModel.toModelRow(
                                editorModel.rowIndex(chunk, chunk.stationCount, SurveyEditorRowIndex.StationRow))
                    let virtualShotRow = editorModel.toModelRow(
                                editorModel.rowIndex(chunk, chunk.shotCount, SurveyEditorRowIndex.ShotRow))

                    compare(virtualStationRow, -1, tag + " chunk[" + i + "] should not have virtual station row")
                    compare(virtualShotRow, -1, tag + " chunk[" + i + "] should not have virtual shot row")

                    if(!initialRealEmptyTail[i]) {
                        compare(chunkHasRealEmptyTail(chunk), false, tag + " chunk[" + i + "] should not gain real empty tail")
                    }
                }
            }

            function stepUntilStationChunk(targetChunk, key, maxSteps) {
                for(let i = 0; i < maxSteps; ++i) {
                    keyClick(key, 0)
                            wait(1);
                    // waitForRendering(rootId)
                    let rowIndex = focusedChunkAndType()
                    verifyVisibleDataBoxesMatchModel("step " + i + " key=" + keyName(key))
                    verifyUnfocusedChunksAreClean(rowIndex.chunk,
                                                  "step " + i + " key=" + keyName(key))
                    if(rowIndex.chunk === targetChunk
                            && rowIndex.rowType === SurveyEditorRowIndex.StationRow)
                    {
                        verifyFocusedRowInListView()
                        return
                    }
                }
                verify(false, "Failed to navigate to target chunk station row")
            }

            let firstChunk = chunks[0]
            let firstStation0Row = editorModel.toModelRow(
                        editorModel.rowIndex(firstChunk, 0, SurveyEditorRowIndex.StationRow))
            verify(firstStation0Row >= 0)
            clickStationName(firstStation0Row)
            for(let i = 0; i < chunks.length; ++i) {
                initialRealEmptyTail[i] = chunkHasRealEmptyTail(chunks[i])
            }
            verifyVisibleDataBoxesMatchModel("initial")
            verifyFocusedRowInListView()
            verifyOnlyFocusedChunkHasVirtualRows(firstChunk)

            let maxSteps = editorModel.rowCount() + 10
            for(let i = 1; i < chunks.length; ++i) {
                let targetChunk = chunks[i]
                stepUntilStationChunk(targetChunk, Qt.Key_Down, maxSteps)
                verifyOnlyFocusedChunkHasVirtualRows(targetChunk)
            }

            for(let i = chunks.length - 2; i >= 0; --i) {
                let targetChunk = chunks[i]
                stepUntilStationChunk(targetChunk, Qt.Key_Up, maxSteps)
                verifyOnlyFocusedChunkHasVirtualRows(targetChunk)
            }
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

            // wait(1000000)

            ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox.2.5->excludeMenuButton")
            let excludeMenuButton = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox.2.5->excludeMenuButton")
            let excludeMenu = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox.2.5->excludeMenuButton->menuLoader")
            tryVerify(() => { return excludeMenu.active === false})

            mouseClick(excludeMenuButton)

            tryVerify(() => { return excludeMenu.active })
            tryVerify(() => { return excludeMenu.item.visible })

            let menuItem = findChild(excludeMenu, "excludeDistanceMenuItem")
            mouseClick(menuItem);

            tryVerify(() => { return excludeMenu.item.visible === false })

            let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view");
            view.positionViewAtEnd()

            //Make sure the distance has gone down
            let totalLengthText_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->totalLengthText")
            tryCompare(totalLengthText_obj1, "text", "Total Length: 5 m");

            //Include it again
            mouseClick(excludeMenuButton)

            tryVerify(() => { return excludeMenu.item.visible })

            menuItem = findChild(excludeMenu, "excludeDistanceMenuItem")
            mouseClick(menuItem);

            tryVerify(() => { return excludeMenu.item.visible === false })

            //Make sure the distance has gone down
            totalLengthText_obj1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->totalLengthText")
            tryCompare(totalLengthText_obj1, "text", "Total Length: 15 m");
        }

        /**
          Test arrow keys for navigation
          */
        function test_arrowNavigation() {
            TestHelper.loadProjectFromFile(RootData.project, "://datasets/tst_SurveyDataEntry/navTest.cw");

            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Cave 1/Trip=Trip 1"

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
                let model = currentItem.model
                let maxSteps = surveyView.count + 10

                if(currentItem.dataValue.rowType === SurveyEditorRowIndex.ShotRow
                        && columnIndex === SurveyChunk.ShotCompassRole
                        && backsight.checked && frontsight.checked)
                {
                    currentItem = downArrow(currentItem, currentItem.listViewIndex, SurveyChunk.ShotBackCompassRole)
                } else if(currentItem.dataValue.rowType === SurveyEditorRowIndex.ShotRow
                          && columnIndex === SurveyChunk.ShotClinoRole
                          && backsight.checked && frontsight.checked)
                {
                    currentItem = downArrow(currentItem, currentItem.listViewIndex, SurveyChunk.ShotBackClinoRole)
                }

                for(let i = 0; i < maxSteps; i++) {
                    let currentCell = model.cellIndex(currentItem.listViewIndex,
                                                      currentItem.dataValue.chunkDataRole)
                    let nextCell = model.nextCell(currentCell,
                                                  SurveyEditorModel.Down,
                                                  frontsight.checked,
                                                  backsight.checked)
                    if(!model.isCellValid(nextCell)) {
                        break;
                    }
                    currentItem = downArrow(currentItem, nextCell.modelRow, nextCell.dataRole);
                }

                //Scroll to the top
                surveyView.positionViewAtBeginning()
                waitForRendering(rootId)
            }

            function upArrowOnFullColumn(currentItem, rowIndex, columnIndex) {
                let surveyView = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view")
                let model = currentItem.model

                //Scroll to the bottom
                surveyView.positionViewAtEnd()
                waitForRendering(rootId)

                if(currentItem.dataValue.rowType === SurveyEditorRowIndex.ShotRow
                        && columnIndex === SurveyChunk.ShotBackCompassRole
                        && backsight.checked && frontsight.checked)
                {
                    currentItem = upArrow(currentItem, currentItem.listViewIndex, SurveyChunk.ShotCompassRole)
                } else if(currentItem.dataValue.rowType === SurveyEditorRowIndex.ShotRow
                          && columnIndex === SurveyChunk.ShotBackClinoRole
                          && backsight.checked && frontsight.checked)
                {
                    currentItem = upArrow(currentItem, currentItem.listViewIndex, SurveyChunk.ShotClinoRole)
                }

                let maxSteps = surveyView.count + 10
                for(let i = 0; i < maxSteps; i++) {
                    let currentCell = model.cellIndex(currentItem.listViewIndex,
                                                      currentItem.dataValue.chunkDataRole)
                    let previousCell = model.nextCell(currentCell,
                                                      SurveyEditorModel.Up,
                                                      frontsight.checked,
                                                      backsight.checked)
                    if(!model.isCellValid(previousCell)) {
                        break;
                    }
                    currentItem = upArrow(currentItem, previousCell.modelRow, previousCell.dataRole);
                }
            }

            function testRow(row, column, direction, testBacksights = true, startWithBacksights = false) {
                let view = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view");
                view.positionViewAtIndex(row, ListView.Contain)


                let cell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + "." + column)
                cell.model.setFocusedCell(cell.model.cellIndex(cell.listViewIndex, cell.dataValue.chunkDataRole))

                verify(frontsight.checked === true)
                verify(backsight.checked === false)
                verify(cell.model.focusedRow === row)
                verify(cell.model.focusedRole === column)

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
                    tryVerify(() => {
                                  cell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + "." + startColumn)
                                  return cell !== null
                              })
                    cell.model.setFocusedCell(cell.model.cellIndex(cell.listViewIndex, cell.dataValue.chunkDataRole))
                    tryVerify(() => {
                                  cell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + "." + startColumn)
                                  return cell !== null
                              })
                    cell.model.setFocusedCell(cell.model.cellIndex(cell.listViewIndex, cell.dataValue.chunkDataRole))

                    let focusedName = focusedDataBoxObjectName()
                    let focusedItem = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->" + focusedName)
                    if(focusedItem !== null) {
                        cell = focusedItem
                    }

                    direction(cell, cell.listViewIndex, cell.dataValue.chunkDataRole);

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

                    tryVerify(() => {
                                  cell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + "." + column)
                                  return cell !== null
                              })
                    cell.model.setFocusedCell(cell.model.cellIndex(cell.listViewIndex, cell.dataValue.chunkDataRole))
                    tryVerify(() => {
                                  cell = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox." + row + "." + column)
                                  return cell !== null
                              })
                    cell.model.setFocusedCell(cell.model.cellIndex(cell.listViewIndex, cell.dataValue.chunkDataRole))
                    focusedName = focusedDataBoxObjectName()
                    focusedItem = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->" + focusedName)
                    if(focusedItem !== null) {
                        cell = focusedItem
                    }

                    direction(cell, cell.listViewIndex, cell.dataValue.chunkDataRole);

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

            // // //Test Down arrow
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

            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Cave 1/Trip=Trip 1"

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
                currentItem = previousTab(currentItem, 13, SurveyChunk.StationNameRole);

                currentItem = previousTab(null, 11, SurveyChunk.StationDownRole);

                //This is a hack to get this testcase to work. Focus is lost some how
                //when reverse tabing, I'm not sure why. The testcase fail intermidantly without this
                //It seems to work just fine on the real interface
                currentItem = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox.11.4")
                wait(100)
                mouseClick(currentItem)

                currentItem = previousTab(null, 11, SurveyChunk.StationUpRole);
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
                currentItem = previousTab(currentItem, 9, SurveyChunk.StationNameRole);

                currentItem = previousTab(null, 7, SurveyChunk.StationDownRole);
                currentItem = previousTab(null, 7, SurveyChunk.StationUpRole);
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

            station1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox.1.0")
            mouseClick(station1)

            tabTestFrontAndBackSight();

            // ----- Backsights only ----
            //Scroll to the top
            surveyView.positionViewAtBeginning();
            waitForRendering(surveyView)

            mouseClick(frontsight);

            station1 = ObjectFinder.findObjectByChain(mainWindow, "rootId->tripPage->view->dataBox.1.0")
            mouseClick(station1)

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

            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Cave 1/Trip=Trip 1"

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

            RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Cave 1/Trip=Trip 1"

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

        //     RootData.pageSelectionModel.currentPageAddress = "Source/Data/Cave=Cave 1/Trip=Trip 1"

        //     tryVerify(()=>{ return RootData.pageView.currentPageItem.objectName === "tripPage" });

        //     waitForRendering(RootData.pageView.currentPageItem)
        //     wait(1000000);
        // }
    }
}
