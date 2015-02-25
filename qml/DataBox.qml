/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import Cavewhere 1.0
import QtQuick.Controls 1.2 as Controls;
import "Navigation.js" as NavigationHandler

Item {
    id: dataBox

    property alias dataValue: editor.text
    property alias dataValidator: editor.validator
    property SurveyChunk surveyChunk; //For hooking up signals and slots in subclasses
    property SurveyChunkView surveyChunkView;
    property SurveyChunkTrimmer surveyChunkTrimmer; //For interaction
    property alias aboutToDelete: removeBoxId.visible

    property int rowIndex: -1
    property int dataRole

    signal rightClick(var mouse);
    signal enteredPressed();
    signal deletePressed();

    //    signal rightClicked(int index)
    //    signal splitOn(int index)

    //color : Qt.rgba(201, 230, 245, 255);

    //This causes memory leaks in qt 4.7.1!!!
    //    Behavior on y { PropertyAnimation { duration: 250 } }
    //    Behavior on opacity  { PropertyAnimation { duration: 250 } }

    //    onDataValidatorChanged: {
    //        dataTextInput.validator = dataValidator;
    //    }

    function deletePressedHandler() {
        dataValue = '';
        editor.openEditor();
        state = 'MiddleTyping';
    }

    function handleTab(eventKey) {
        if(eventKey.key === Qt.Key_Tab) {
            surveyChunkView.tab(rowIndex, dataRole)
            eventKey.accepted = true
        } else if(eventKey.key === 1 + Qt.Key_Tab) {
            //Shift tab -- 1 + Qt.Key_Tab is a hack but it works
            surveyChunkView.previousTab(rowIndex, dataRole)
            eventKey.accepted = true
        }
    }

    onEnteredPressed: {
        editor.openEditor()
    }

    onDeletePressed: {
        deletePressedHandler()
    }

    onRightClick: {
        //Show menu
        rightClickMenu.popup();
    }

    Controls.Menu {
        id: rightClickMenu

        Controls.MenuItem {
            text: "Remove Chunk"
            onTriggered: {
                surveyChunk.parentTrip.removeChunk(surveyChunk)
            }

//            onContainsMouseChanged: {
//                var lastStationIndex = surveyChunk.stationCount() - 1;
//                var lastShotIndex = surveyChunk.shotCount() - 1;

//                if(containsMouse) {
//                    surveyChunkView.showRemoveBoxsOnStations(0, lastStationIndex);
//                    surveyChunkView.showRemoveBoxsOnShots(0, lastShotIndex);
//                } else {
//                    surveyChunkView.hideRemoveBoxs();
//                }
//            }
        }
    }

    RemoveDataRectangle {
        id: removeBoxId
        visible: false
        anchors.fill: parent
        anchors.rightMargin: -1
        z: 1
    }

    MouseArea {
        anchors.fill: parent

        enabled: !editor.isEditting

        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onClicked: {
            dataBox.focus = true

            if(mouse.button == Qt.RightButton) {
                rightClick(mouse)
            }
        }
    }

    Rectangle {
        id: backgroundStation
        anchors.fill: parent

        gradient: Gradient {
            GradientStop {
                position: rowIndex % 2 === 0 ? 1.0 : 0.0
                color:  "#DDF2FF"
            }
            GradientStop {
                position: rowIndex % 2 === 0 ? 0.4 : 0.6
                color:  "white"
            }
        }

        visible: surveyChunk !== null && surveyChunk.isStationRole(dataRole)

    }

    Rectangle {
        id: backgroundShot
        anchors.fill: parent
        color: "#DDF2FF"
        visible: rowIndex % 2 === 0 && surveyChunk !== null && surveyChunk.isShotRole(dataRole)
    }

    Rectangle {
        id: border
        anchors.fill: parent
        border.color: "lightgray"
        color: "#00000000"
        border.width: 1
    }

    Rectangle {
        id: interalHighlight
        border.color: "black"
        anchors.fill: parent
        anchors.margins: 1
        border.width: 1
        color: "#00000000"
        visible: dataBox.focus || editor.isEditting
    }

    DoubleClickTextInput {
        id: editor
        anchors.fill: parent
        autoResize: true

        onFinishedEditting: {
            surveyChunk.setData(dataRole, rowIndex, newText)
            dataBox.state = ""; //Go back to the default state
            dataBox.forceActiveFocus();
        }

        onStartedEditting: {
            dataBox.state = 'MiddleTyping';
        }
    }

    Keys.onPressed: {
        handleTab(event);
        surveyChunkView.navigationArrow(rowIndex, dataRole, event.key);

        if(event.key === Qt.Key_Backspace) {
            deletePressedHandler();
            return;
        }

        if(editor.validator.validate(event.text) > 0 && event.text.length > 0) {
            dataBox.state = 'MiddleTyping'
            editor.openEditor()
            globalShadowTextInput.textInput.text  = event.text
            globalShadowTextInput.clearSelection() //GlobalShowTextInput is what's opened from editor.openEditor
        }
    }

    Keys.onSpacePressed: {
        var trip = surveyChunk.parentTrip;
        if(trip.numberOfChunks > 0) {
            var lastChunkIndex = trip.numberOfChunks - 1
            var lastChunk = trip.chunk(lastChunkIndex);
            if(lastChunk.isStationAndShotsEmpty()) {
                surveyChunkView.parent.setFocus(lastChunkIndex)
                return;
            }
        }

        surveyChunk.parentTrip.addNewChunk();
    }

    Keys.onEnterPressed: {
        enteredPressed()
    }

    Keys.onReturnPressed: {
        enteredPressed();
    }

    Keys.onDeletePressed: {
        deletePressed();
    }

    onFocusChanged: {
        if(focus) {
            //Make sure it's visible to the user
            surveyChunkView.ensureDataBoxVisible(rowIndex, dataRole)
            surveyChunkTrimmer.chunk = surveyChunk;
        }
    }

    states: [

        State {
            name: "MiddleTyping"

            PropertyChanges {
                target: globalShadowTextInput.textInput

                onPressKeyPressed: {
                    if(pressKeyEvent.key === Qt.Key_Tab ||
                            pressKeyEvent.key === 1 + Qt.Key_Tab ||
                            pressKeyEvent.key === Qt.Key_Space)
                    {
                        var commited = editor.commitChanges()
                        if(!commited) { return; }
                    }

                    if(pressKeyEvent.key === Qt.Key_Space) {
                        surveyChunk.parentTrip.addNewChunk();
                    }

                    //Tab to the next entry on enter
                    if(pressKeyEvent.key === Qt.Key_Enter ||
                            pressKeyEvent.key === Qt.Key_Return) {
                        surveyChunkView.tab(rowIndex, dataRole)
                        pressKeyEvent.accepted = true;
                    }

                    //Use the default keyhanding that the GlobalShadowTextInput has
                    globalShadowTextInput.textInput.defaultKeyHandling();

                    //Handle the tabbing
                    dataBox.handleTab(pressKeyEvent);

                    if(pressKeyEvent.accepted) {
                        //Have the editor commit changes
                        dataBox.state = ''; //Default state

                    }
                }

                onFocusChanged: {
                    if(!focus) {
                        dataBox.state = '';
                    }
                }
            }

            PropertyChanges {
                target: globalShadowTextInput

                onEscapePressed: {
                    dataBox.state = ''; //Default state
                    dataBox.forceActiveFocus()
                }

                onEnterPressed: {
                    var commited = editor.commitChanges();
                    if(commited) {
                        dataBox.focus = true
                    }
                }
            }

            PropertyChanges {
                target: dataBox
                z: 1
            }
        }
    ]
}
