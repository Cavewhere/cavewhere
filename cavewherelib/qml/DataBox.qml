/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib
import QtQuick.Controls as QC
import QtQuick.Controls as QC

SurveyEditorCell {
    id: dataBox
    objectName: listViewIndex >= 0 ?
                    ("dataBox." + listViewIndex + "." + dataValue.chunkDataRole) :
                    ("templateCell." + listViewIndex + "." + dataValue.chunkDataRole)

    property alias dataValidator: editor.validator

    property alias aboutToDelete: removeBoxId.visible
    readonly property ErrorModel errorModel: dataValue.errorModel
    required property QC.ButtonGroup errorButtonGroup

    //The index informantion from cwSurveyEditorModel
    required property cwSurveyEditorBoxData dataValue

    cellRole: dataBox.dataValue.chunkDataRole
    indexInChunk: dataBox.dataValue.indexInChunk
    chunk: dataBox.dataValue.chunk
    editing: editor.isEditting

    property int editTargetRow: -1
    property int editTargetRole: -1

    property GlobalShadowTextInput _globalShadowTextInput: GlobalShadowTextInput
    property GlobalTextInputHelper _globalTextInput: GlobalShadowTextInput.textInput

    signal enteredPressed();
    signal deletePressed();

    //Uncomment to visualize indexes for the box
    // QC.Label {
    //     color: "red"
    //     font.pixelSize: 10
    //     text: dataBox.objectName + "\nF:" + dataBox.focus
    //     z: 1
    // }

    function deletePressedHandler() {
        editor.text = "";
        editor.openEditor();
        state = 'MiddleTyping';
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
        if(dataBox.highlightVisible) {
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
                let row = model.modelRowForCellRole(lastChunk, 0, SurveyEditorCellIndex.StationNameCell)
                model.setFocusedCell(model.cellIndex(row, SurveyEditorCellIndex.StationNameCell))
                return;
            }
        }

        dataValue.chunk.parentTrip.addNewChunk();
    }

    onEnteredPressed: {
        editor.openEditor()
    }

    onDeletePressed: {
        deletePressedHandler()
    }

    RemoveDataRectangle {
        id: removeBoxId
        visible: false
        anchors.fill: parent
        anchors.rightMargin: -1
        z: 1
    }

    QQ.Keys.onPressed: (event) => {
                           if(dataBox.handleNavigationKey(event)) {
                               return;
                           }

                           //This cell swallows the rest: an unhandled key here
                           //would reach the view and scroll it
                           event.accepted = true

                           switch(event.key) {
                               case Qt.Key_Enter:
                               case Qt.Key_Return:
                               dataBox.state = 'MiddleTyping'
                               editor.openEditor()
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


    onDataValueChanged: {
        dataBox.syncFocusState()
    }

    DoubleClickTextInput {
        id: editor
        anchors.fill: parent
        autoResize: true
        text: dataBox.dataValue.reading.value

        onFinishedEditting: (newText) => {
                                model.setDataAt(model.cellIndex(dataBox.editTargetRow, dataBox.editTargetRole), newText)
                                dataBox.state = ""; //Go back to the default state
                                dataBox.forceActiveFocus();
                            }

        onStartedEditting: {
            dataBox.editTargetRow = dataBox.listViewIndex
            dataBox.editTargetRole = dataBox.dataValue.chunkDataRole
            dataBox.state = 'MiddleTyping';
        }

        //The editor's own handler takes the click before the cell's does, so a
        //splay move waiting for a station to land on is finished from here
        onClicked: {
            if(dataBox.model !== null && dataBox.model.splayMoveActive) {
                dataBox.handleSplayMoveTap()
                return
            }

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
                color: Theme.transparent
                visible: errorBorderLoaderId.shouldBeVisible || errorIcon.checked

                RoundButton {
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
                    QC.ButtonGroup.group: dataBox.errorButtonGroup

                    background: QQ.Rectangle {
                        implicitWidth: 12
                        implicitHeight: 12
                        color: errorIcon.down || errorIcon.checked ? Theme.surfaceRaised : Theme.surfaceMuted
                        border.color: Theme.border
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
