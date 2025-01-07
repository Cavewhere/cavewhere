/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib
import QtQuick.Controls as Controls;
// import QtQuick.Layouts
// import "Navigation.js" as NavigationHandler

QQ.Item {
    id: dataBox

    property alias dataValue: editor.text
    property alias dataValidator: editor.validator
    property SurveyChunk surveyChunk; //For hooking up signals and slots in subclasses
    property SurveyChunkView surveyChunkView;
    property SurveyChunkTrimmer surveyChunkTrimmer; //For interaction
    property alias aboutToDelete: removeBoxId.visible
    property ErrorModel errorModel: null

    property int rowIndex: -1
    property int dataRole

    property GlobalShadowTextInput _globalShadowTextInput: GlobalShadowTextInput
    property GlobalTextInputHelper _globalTextInput: GlobalShadowTextInput.textInput

    signal rightClick(var mouse);
    signal enteredPressed();
    signal deletePressed();
    signal tabPressed();

    //    signal rightClicked(int index)
    //    signal splitOn(int index)

    //color : Qt.rgba(201, 230, 245, 255);

    //This causes memory leaks in qt 4.7.1!!!
    //    QQ.Behavior on y { QQ.PropertyAnimation { duration: 250 } }
    //    QQ.Behavior on opacity  { QQ.PropertyAnimation { duration: 250 } }

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
            tabPressed();
            surveyChunkView.tab(rowIndex, dataRole)
            eventKey.accepted = true
        } else if(eventKey.key === 1 + Qt.Key_Tab) {
            //Shift tab -- 1 + Qt.Key_Tab is a hack but it works
            surveyChunkView.previousTab(rowIndex, dataRole)
            eventKey.accepted = true
        }
    }

    function errorImageSource(errorType) {
        switch(errorType) {
        case CwError.Fatal:
            return "qrc:icons/stopSignError.png";
        case CwError.Warning:
            return "qrc:icons/warning.png"
        default:
            return "";
        }
    }

    function errorBorderColor(errorType) {
        function errorColor(errorType) {
            switch(errorType) {
            case CwError.Fatal:
                return "#960800";
            case CwError.Warning:
                return "#FF7600"
            default:
                return "black";
            }
        }

        //This simulates highlight. The error box will overdraw
        //and cover the databox highlighting
        let color = errorColor(errorType)
        if(interalHighlight.visible) {
            return Qt.darker(color);
        }
        return color
    }

    function errorAppearance(func) {
        if(errorModel !== null) {
            if(errorModel.fatalCount > 0) {
                return func(CwError.Fatal);
            } else if(errorModel.warningCount > 0) {
                return func(CwError.Warning);
            }
        }
        return ""
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

    //    onErrorsChanged: {
    //        console.log("Errors changed!" + errors)

    //        var color = ""
    //        var iconSource = ""
    //        var errorsVisible = false;
    //        for(var errorIndex in errors) {
    //            var error = errors[errorIndex]

    ////            console.log("Error:" + error)

    //            if(!error.suppressed) {
    //                errorsVisible = true;
    //            }

    //            color = errorBorderColor(error);
    //            iconSource = errorImageSource(error);
    //            if(error.type === CwError.Fatal) {
    //                break;
    //            }

    //        }

    //        errorBorder.shouldBeVisible = errorsVisible
    //        errorBorder.border.color = color
    //        errorIcon.iconSource = iconSource
    //    }

    Controls.Menu {
        id: rightClickMenu

        Controls.MenuItem {
            text: "Remove Chunk"
            onTriggered: {
                dataBox.surveyChunk.parentTrip.removeChunk(dataBox.surveyChunk)
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

    QQ.MouseArea {
        anchors.fill: parent

        enabled: !editor.isEditting

        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onClicked: (mouse) => {
                       dataBox.focus = true

                       if(mouse.button == Qt.RightButton) {
                           dataBox.rightClick(mouse)
                       }
                   }
    }

    QQ.Rectangle {
        id: backgroundStation
        anchors.fill: parent

        gradient: QQ.Gradient {
            QQ.GradientStop {
                position: dataBox.rowIndex % 2 === 0 ? 1.0 : 0.0
                color:  "#DDF2FF"
            }
            QQ.GradientStop {
                position: dataBox.rowIndex % 2 === 0 ? 0.4 : 0.6
                color:  "white"
            }
        }

        visible: dataBox.surveyChunk !== null && dataBox.surveyChunk.isStationRole(dataBox.dataRole)
    }

    QQ.Rectangle {
        id: backgroundShot
        property bool offsetColor: dataBox.rowIndex % 2 === 0 && dataBox.surveyChunk !== null && dataBox.surveyChunk.isShotRole(dataBox.dataRole)
        anchors.fill: parent
        color: offsetColor ? "#DDF2FF" : "white"
    }

    QQ.Rectangle {
        id: border
        anchors.fill: parent
        border.color:  "lightgray"
        color: "#00000000"
        border.width: 1
    }

    QQ.Rectangle {
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

        onFinishedEditting: (newText) => {
                                dataBox.surveyChunk.setData(dataBox.dataRole, dataBox.rowIndex, newText)
                                dataBox.state = ""; //Go back to the default state
                                dataBox.forceActiveFocus();
                            }

        onStartedEditting: {
            dataBox.state = 'MiddleTyping';
        }

        onClicked: {
            dataBox.focus = true
        }
    }

    QQ.Rectangle {
        id: errorBorder
        property bool shouldBeVisible: dataBox.errorModel !== null && (dataBox.errorModel.fatalCount > 0 || dataBox.errorModel.warningCount > 0)

        anchors.fill: parent
        anchors.margins: 1
        border.width: 1
        border.color: dataBox.errorAppearance(dataBox.errorBorderColor)
        color: "#00000000"
        visible: shouldBeVisible || errorIcon.checked

        Controls.RoundButton {
            id: errorIcon
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 2
            radius: 0
            icon.source: dataBox.errorAppearance(dataBox.errorImageSource)
            checkable: true
            // icon.width: 10
            // icon.height: 10
            implicitWidth: 10
            implicitHeight: 10
            // globalClickToUncheck: true
            // radius: 0
        }

        ErrorListQuoteBox {
            visible: errorIcon.checked
            errors:  dataBox.errorModel !== null ? dataBox.errorModel.errors : null
            errorIcon: errorIcon
        }
    }

    QQ.Keys.onPressed: (event) => {
                           handleTab(event);
                           surveyChunkView.navigationArrow(rowIndex, dataRole, event.key);

                           if(event.key === Qt.Key_Backspace) {
                               deletePressedHandler();
                               return;
                           }

                           if(editor.validator.validate(event.text) > 0 && event.text.length > 0) {
                               dataBox.state = 'MiddleTyping'
                               editor.openEditor()
                               GlobalShadowTextInput.textInput.text  = event.text
                               GlobalShadowTextInput.clearSelection() //GlobalShowTextInput is what's opened from editor.openEditor
                           }
                       }

    QQ.Keys.onSpacePressed: {
        var trip = surveyChunk.parentTrip;
        if(trip.chunkCount > 0) {
            var lastChunkIndex = trip.chunkCount - 1
            var lastChunk = trip.chunk(lastChunkIndex);
            if(lastChunk.isStationAndShotsEmpty()) {
                (dataBox.surveyChunkView.parent as SurveyChunkGroupView).setFocus(lastChunkIndex)
                return;1
            }
        }

        surveyChunk.parentTrip.addNewChunk();
    }

    QQ.Keys.onEnterPressed: {
        enteredPressed()
    }

    QQ.Keys.onReturnPressed: {
        enteredPressed();
    }

    QQ.Keys.onDeletePressed: {
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

        QQ.State {
            name: "MiddleTyping"

            QQ.PropertyChanges {
                // dataBox._globalTextInput.onEnterPressed: {
                //     console.log("sauce")
                // }

                dataBox._globalTextInput.onPressKeyPressed: {
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
                    GlobalShadowTextInput.textInput.defaultKeyHandling();

                    //Handle the tabbing
                    dataBox.handleTab(pressKeyEvent);

                    if(pressKeyEvent.accepted) {
                        //Have the editor commit changes
                        dataBox.state = ''; //Default state

                    }

                }

                dataBox._globalTextInput.onFocusChanged: {
                    if(!focus) {
                        dataBox.state = '';
                    }
                }

                dataBox._globalShadowTextInput.onEscapePressed: {
                    dataBox.state = ''; //Default state
                    dataBox.forceActiveFocus()
                }

                dataBox._globalShadowTextInput.onEnterPressed: {
                    var commited = editor.commitChanges();
                    if(commited) {
                        dataBox.focus = true
                    }
                }
            }

            QQ.PropertyChanges {
                dataBox {
                    z: 1
                }
            }
        }
    ]
}
