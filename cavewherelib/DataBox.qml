/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib
import QtQuick.Controls as Controls;
import QtQuick.Controls.Basic as BasicControls

QQ.Item {
    id: dataBox
    objectName: "dataBox." + listViewIndex + "." + dataValue.chunkDataRole

    property alias dataValidator: editor.validator

    required property SurveyChunkTrimmer surveyChunkTrimmer; //For interaction
    property alias aboutToDelete: removeBoxId.visible
    readonly property ErrorModel errorModel: dataValue.errorModel
    required property Controls.ButtonGroup errorButtonGroup
    property KeyNavigationContainer navigation: KeyNavigationContainer {}

    //The index informantion from cwSurveyEditorModel
    required property cwSurveyEditorBoxData dataValue

    //Index in the ListView
    required property int listViewIndex

    required property QQ.ListView view

    //For changing the focus to other data boxs
    required property SurveyEditorFocus editorFocus

    readonly property bool hasEditorFocus: shouldHaveFocus()

    property GlobalShadowTextInput _globalShadowTextInput: GlobalShadowTextInput
    property GlobalTextInputHelper _globalTextInput: GlobalShadowTextInput.textInput

    signal rightClick(var mouse);
    signal enteredPressed();
    signal deletePressed();
    signal tabPressed();

    //Uncomment to visualize indexes for the box
    // Text {
    //     color: "red"
    //     font.pixelSize: 10
    //     text: dataBox.objectName + "\nF:" + dataBox.focus + "\nEF:" + hasEditorFocus
    //     z: 1
    // }

    function shouldHaveFocus() {
        return editorFocus.boxIndex === dataValue.boxIndex
    }

    function deletePressedHandler() {
        editor.text = "";
        editor.openEditor();
        state = 'MiddleTyping';
    }

    function handleNextTab() {
        dataBox.editorFocus.setIndex(dataBox.navigation.tabNext())
    }

    function handleTab(eventKey) {
        if(eventKey.key === Qt.Key_Tab) {
            tabPressed();
            handleNextTab();
            eventKey.accepted = true
        } else if(eventKey.key === 1 + Qt.Key_Tab) {
            //Shift tab -- 1 + Qt.Key_Tab is a hack but it works
            dataBox.editorFocus.setIndex(dataBox.navigation.tabPrevious())
            eventKey.accepted = true
        }
    }

    function errorImageSource(errorType) {
        switch(errorType) {
        case CwError.Fatal:
            return "qrc:icons/svg/stopSignError.svg";
        case CwError.Warning:
            return "qrc:icons/svg/warning.svg"
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

    function addNewChunk() {
        var trip = dataValue.chunk.parentTrip;
        if(trip.chunkCount > 0) {
            var lastChunkIndex = trip.chunkCount - 1
            var lastChunk = trip.chunk(lastChunkIndex);
            if(lastChunk.isStationAndShotsEmpty()) {
                dataBox.editorFocus.focusOnLastChunk();
                return;
            }
        }

        dataValue.chunk.parentTrip.addNewChunk();
    }

    onHasEditorFocusChanged: {
        if(hasEditorFocus) {
            forceActiveFocus()
        }
        focus = hasEditorFocus
    }

    //We need to watch on currentIndex changed because the view
    //set the focus on the row delegate. This will disable the focus on the
    //correct databox.
    QQ.Connections {
        target: dataBox.view
        function onCurrentIndexChanged() {
            dataBox.focus = shouldHaveFocus(); //We need to use this instead of property, because property could be out of data, and cause binding loop
            if(dataBox.focus) {
                // console.log("CurrentIndexChanged:" + dataBox.view.currentIndex + " " + editorFocus.boxIndex + dataBox.dataValue.boxIndex
                //             + " hasfocus:" + dataBox.hasEditorFocus
                //             + " correct value:" + (editorFocus.boxIndex === dataBox.dataValue.boxIndex)
                //             + " shouldHaveFocus:" + shouldHaveFocus())
                dataBox.forceActiveFocus()
            }
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
        rightClickMenuLoader.active = true
        rightClickMenuLoader.item.popup();
    }

    QQ.Loader {
        id: rightClickMenuLoader
        active: false

        sourceComponent: Controls.Menu {

            Controls.MenuItem {
                text: "Remove Chunk"
                onTriggered: {
                    dataBox.dataValue.chunk.parentTrip.removeChunk(dataBox.dataValue.chunk)
                }

                //            onContainsMouseChanged: {
                //                var lastStationIndex = index.chunk.stationCount() - 1;
                //                var lastShotIndex = index.chunk.shotCount() - 1;

                //                if(containsMouse) {
                //                    surveyChunkView.showRemoveBoxsOnStations(0, lastStationIndex);
                //                    surveyChunkView.showRemoveBoxsOnShots(0, lastShotIndex);
                //                } else {
                //                    surveyChunkView.hideRemoveBoxs();
                //                }
                //            }
            }
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
                       dataBox.editorFocus.setIndex(dataBox.dataValue.boxIndex)
                       // dataBox.focus = true

                       if(mouse.button === Qt.RightButton) {
                           dataBox.rightClick(mouse)
                       }
                   }
    }

    QQ.Rectangle {
        id: backgroundStation
        anchors.fill: parent

        gradient: QQ.Gradient {
            QQ.GradientStop {
                position: dataBox.dataValue.indexInChunk % 2 === 0 ? 1.0 : 0.0
                color:  "#DDF2FF"
            }
            QQ.GradientStop {
                position: dataBox.dataValue.indexInChunk % 2 === 0 ? 0.4 : 0.6
                color:  "white"
            }
        }

        visible: dataBox.dataValue.chunk !== null
                 && dataBox.dataValue.chunk.isStationRole(dataBox.dataValue.chunkDataRole)
    }

    QQ.Rectangle {
        id: backgroundShot
        property bool offsetColor: dataBox.dataValue.indexInChunk % 2 === 0
                                   && dataBox.dataValue.chunk !== null
                                   && dataBox.dataValue.chunk.isShotRole(dataBox.dataValue.chunkDataRole)
        anchors.fill: parent
        visible: dataBox.dataValue.chunk ? dataBox.dataValue.chunk.isShotRole(dataBox.dataValue.chunkDataRole) : false
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

    QQ.Keys.onPressed: (event) => {
                           event.accepted = true
                           handleTab(event);
                           switch(event.key) {
                               case Qt.Key_Left:
                               dataBox.editorFocus.setIndex(dataBox.navigation.arrowLeft())
                               event.accepted = true
                               break;
                               case Qt.Key_Right:
                               dataBox.editorFocus.setIndex(dataBox.navigation.arrowRight())
                               event.accepted = true
                               break;
                               case Qt.Key_Up:
                               dataBox.editorFocus.setIndex(dataBox.navigation.arrowUp())
                               event.accepted = true
                               break;
                               case Qt.Key_Down:
                               dataBox.editorFocus.setIndex(dataBox.navigation.arrowDown())
                               event.accepted = true
                               break;
                               case Qt.Key_Backspace:
                               // deletePressedHandler();
                               return;
                           }

                           if(dataValidator.validate(event.text) > 0 && event.text.length > 0) {
                               dataBox.state = 'MiddleTyping'
                               editor.openEditor()
                               GlobalShadowTextInput.textInput.text  = event.text
                               GlobalShadowTextInput.clearSelection() //GlobalShowTextInput is what's opened from editor.openEditor
                           }
                       }

    QQ.Keys.onSpacePressed: {
        addNewChunk();
    }


    onFocusChanged: {
        if(focus) {
            surveyChunkTrimmer.chunk = dataValue.chunk;
            editorFocus.setIndex(dataValue.boxIndex)
        }
    }


    DoubleClickTextInput {
        id: editor
        anchors.fill: parent
        autoResize: true
        text: dataBox.dataValue.reading.value

        onFinishedEditting: (newText) => {
                                let chunk = dataBox.dataValue.chunk
                                chunk.setData(dataBox.dataValue.chunkDataRole, dataBox.dataValue.indexInChunk, newText)
                                dataBox.state = ""; //Go back to the default state
                                dataBox.forceActiveFocus();
                            }

        onStartedEditting: {
            dataBox.state = 'MiddleTyping';
        }

        onClicked: {
            dataBox.forceActiveFocus();
        }

        QQ.Loader {
            id: errorBorderLoaderId
            property bool shouldBeVisible: dataBox.errorModel !== null && (dataBox.errorModel.fatalCount > 0 || dataBox.errorModel.warningCount > 0)

            active: shouldBeVisible
            anchors.fill: parent

            //This potentially causue a crash
            // asynchronous: true

            sourceComponent: QQ.Rectangle {
                id: errorBorder
                // property bool shouldBeVisible: dataBox.errorModel !== null && (dataBox.errorModel.fatalCount > 0 || dataBox.errorModel.warningCount > 0)

                anchors.fill: parent
                anchors.margins: 1
                border.width: 1
                border.color: dataBox.errorAppearance(dataBox.errorBorderColor)
                color: "#00000000"
                visible: errorBorderLoaderId.shouldBeVisible || errorIcon.checked

                Controls.RoundButton {
                    id: errorIcon
                    objectName: "errorIcon"

                    property bool hasBeenToggled: false

                    implicitWidth: 12
                    implicitHeight: 12

                    checkable: true
                    radius: 0 //Makes it a square

                    focusPolicy: Qt.NoFocus

                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 2

                    //Make the popup go away when another error button is pressed
                    Controls.ButtonGroup.group: dataBox.errorButtonGroup

                    background: QQ.Rectangle {
                        implicitWidth: 12
                        implicitHeight: 12
                        color: errorIcon.down || errorIcon.checked ? "#d6d6d6" : "#f6f6f6"
                        border.color: "#26282a"
                        border.width: 1
                        radius: 0
                    }

                    QQ.Image {
                        anchors.centerIn: parent
                        source: dataBox.errorAppearance(dataBox.errorImageSource)
                        sourceSize: Qt.size(errorIcon.implicitWidth - 4, errorIcon.implicitHeight - 4)
                    }
                    onClicked: {
                        //ButtonGroup prevents users for unchecking the button
                        //this allows the checkbox to be unchecked by the user
                        if(checked && !hasBeenToggled) {
                            checked = false;
                        }
                        hasBeenToggled = false;
                    }

                    onToggled: {
                        hasBeenToggled = true;
                    }
                }

                ErrorListQuoteBox {
                    visible: errorIcon.checked
                    errors:  dataBox.errorModel !== null ? dataBox.errorModel.errors : null
                    errorIcon: errorIcon
                    quoteBoxObjectName: "errorBox" + dataBox.objectName
                }
            }
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
    }

    states: [

        QQ.State {
            name: "MiddleTyping"

            QQ.PropertyChanges {

                dataBox._globalTextInput.onPressKeyPressed: () => {
                    if(pressKeyEvent.key === Qt.Key_Tab ||
                       pressKeyEvent.key === 1 + Qt.Key_Tab ||
                       pressKeyEvent.key === Qt.Key_Space)
                    {
                        var commited = editor.commitChanges()
                        if(!commited) { return; }
                    }

                    if(pressKeyEvent.key === Qt.Key_Space) {
                        dataBox.addNewChunk();
                    }

                    //Tab to the next entry on enter
                    if(pressKeyEvent.key === Qt.Key_Enter ||
                       pressKeyEvent.key === Qt.Key_Return) {

                        dataBox.handleNextTab()
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
                        dataBox.forceActiveFocus()
                    }
                }

                dataBox.z: 1
            }
        }
    ]

}
