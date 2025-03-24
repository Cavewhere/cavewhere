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
    objectName: "dataBox." + rowIndex + "." + dataRole

    property alias dataValue: editor.text
    property alias dataValidator: editor.validator
    required property SurveyChunk surveyChunk; //For hooking up signals and slots in subclasses
    // required property SurveyChunkView surveyChunkView;
    required property SurveyChunkTrimmer surveyChunkTrimmer; //For interaction
    property alias aboutToDelete: removeBoxId.visible
    property ErrorModel errorModel: null
    required property Controls.ButtonGroup errorButtonGroup
    property KeyNavigationContainer navigation: KeyNavigationContainer {}

    required property int rowIndex
    required property int dataRole
    required property QQ.ListView view

    property GlobalShadowTextInput _globalShadowTextInput: GlobalShadowTextInput
    property GlobalTextInputHelper _globalTextInput: GlobalShadowTextInput.textInput

    signal rightClick(var mouse);
    signal enteredPressed();
    signal deletePressed();
    signal tabPressed();

    //    signal rightClicked(int index)
    //    signal splitOn(int index)

    function deletePressedHandler() {
        dataValue = '';
        editor.openEditor();
        state = 'MiddleTyping';
    }

    function handleNavigation(navProperty) {
        if(navigation[navProperty] !== null) {
            let item = navigation[navProperty].item;

            if(navigation[navProperty].indexOffset !== 0) {
                view.currentIndex = index

                let itemIndex = -1;
                for(let childIndex in view.currentItem.children) {
                    if(view.currentItem.children[childIndex] === item) {
                        itemIndex = childIndex;
                        break;
                    }
                }

                if(itemIndex >= 0) {
                    let nextCurrentIndex = view.currentIndex + navigation[navProperty].indexOffset

                    //1 because title is at index 0
                    while(nextCurrentIndex >= 1 && nextCurrentIndex < view.count) {
                        // console.log("NextCurrentIndex:" + view.currentIndex + " " + nextCurrentIndex);
                        view.currentIndex = nextCurrentIndex;
                        if(view.currentItem.children[itemIndex].visible) {
                            view.currentItem.children[itemIndex].forceActiveFocus()
                            break;
                        }
                        nextCurrentIndex = view.currentIndex + navigation[navProperty].indexOffset
                    }
                } else {
                    let childrenItemStr = "";
                    for(let childIndex in view.currentItem.children) {
                        childrenItemStr += "\n\t" + view.currentItem.children[childIndex];
                    }

                    throw "Couldn't find \"" + item + "\" try setting offsetIndex = 0, in list:" + childrenItemStr;
                }
            } else {
                if(item !== null) {
                    item.forceActiveFocus()
                }
            }
        }
    }

    function handleTab(eventKey) {
        // console.log("HandleTab!")
        if(eventKey.key === Qt.Key_Tab) {
            // console.log("Tab pressed! on " + dataBox.objectName)
            tabPressed();
            handleNavigation("tabNext");
            eventKey.accepted = true
        } else if(eventKey.key === 1 + Qt.Key_Tab) {
            //Shift tab -- 1 + Qt.Key_Tab is a hack but it works
            // console.log("Tab Shift pressed! on " + dataBox.objectName)
            handleNavigation("tabPrevious");
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

    QQ.Keys.onPressed: (event) => {
        handleTab(event);
        switch(event.key) {
        case Qt.Key_Left:
        case Qt.Key_Right:
        case Qt.Key_Up:
        case Qt.Key_Down:
        case Qt.Key_Backspace:
            deletePressedHandler();
            return;
        }

        // surveyChunkView.navigationArrow(rowIndex, dataRole, event.key);

        // if(event.key === Qt.Key_Backspace) {
        // }

        // if(editor.validator.validate(event.text) > 0 && event.text.length > 0) {
        //     dataBox.state = 'MiddleTyping'
        //     editor.openEditor()
        //     GlobalShadowTextInput.textInput.text  = event.text
        //     GlobalShadowTextInput.clearSelection() //GlobalShowTextInput is what's opened from editor.openEditor
        // }
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
            dataBox.view.currentIndex = dataBox.rowIndex
            dataBox.forceActiveFocus();
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

        // QQ.Keys.onPressed: (event) => {
        //     handleTab(event);
        //     switch(event.key) {
        //     case Qt.Key_Left:
        //     case Qt.Key_Right:
        //     case Qt.Key_Up:
        //     case Qt.Key_Down:
        //     case Qt.Key_Backspace:
        //         deletePressedHandler();
        //         return;
        //     }

        //     surveyChunkView.navigationArrow(rowIndex, dataRole, event.key);

        //     // if(event.key === Qt.Key_Backspace) {
        //     // }

        //     if(editor.validator.validate(event.text) > 0 && event.text.length > 0) {
        //         dataBox.state = 'MiddleTyping'
        //         editor.openEditor()
        //         GlobalShadowTextInput.textInput.text  = event.text
        //         GlobalShadowTextInput.clearSelection() //GlobalShowTextInput is what's opened from editor.openEditor
        //     }
        // }


        // QQ.Keys.onPressed: (event) => {
        //                        handleTab(event);
        //                        surveyChunkView.navigationArrow(rowIndex, dataRole, event.key);

        //                        if(event.key === Qt.Key_Backspace) {
        //                            deletePressedHandler();
        //                            return;
        //                        }

        //                        if(editor.validator.validate(event.text) > 0 && event.text.length > 0) {
        //                            dataBox.state = 'MiddleTyping'
        //                            editor.openEditor()
        //                            GlobalShadowTextInput.textInput.text  = event.text
        //                            GlobalShadowTextInput.clearSelection() //GlobalShowTextInput is what's opened from editor.openEditor
        //                        }
        //                    }

        // QQ.Keys.onSpacePressed: {
        //     var trip = surveyChunk.parentTrip;
        //     if(trip.chunkCount > 0) {
        //         var lastChunkIndex = trip.chunkCount - 1
        //         var lastChunk = trip.chunk(lastChunkIndex);
        //         if(lastChunk.isStationAndShotsEmpty()) {
        //             (dataBox.surveyChunkView.parent as SurveyChunkGroupView).setFocus(lastChunkIndex)
        //             return;
        //         }
        //     }

        //     console.log("space bar 2 area click, new chunk!");
        //     surveyChunk.parentTrip.addNewChunk();
        // }

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
                //            surveyChunkView.ensureDataBoxVisible(rowIndex, dataRole)
                //            surveyChunkTrimmer.chunk = surveyChunk;
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
                            console.log("Space bar, new chunk!");
                            surveyChunk.parentTrip.addNewChunk();
                        }

                        //Tab to the next entry on enter
                        if(pressKeyEvent.key === Qt.Key_Enter ||
                           pressKeyEvent.key === Qt.Key_Return) {
                            handleNavigation("tabNext")
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
                }

                QQ.PropertyChanges {
                    dataBox {
                        z: 1
                    }
                }
            }
        ]
    }
}
