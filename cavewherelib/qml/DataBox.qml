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
                    ("dataBox." + listViewIndex + "." + cellRole) :
                    ("templateCell." + listViewIndex + "." + cellRole)

    property alias dataValidator: editor.validator

    property alias aboutToDelete: removeBoxId.visible

    //Space starts the next chunk from the cells that make up a shot, which is
    //the cells the chunk stores a reading for — a splay's reading stands
    //outside that flow, so the model answers it from the cell
    readonly property bool startsChunkOnSpace: dataBox.model !== null
                                               && dataBox.model.isChunkCell(dataBox.cellRole)
    readonly property ErrorModel errorModel: dataValue.errorModel
    required property QC.ButtonGroup errorButtonGroup

    //The index informantion from cwSurveyEditorModel
    required property cwSurveyEditorBoxData dataValue

    cellRole: dataBox.dataValue.cellRole
    indexInChunk: dataBox.dataValue.indexInChunk
    chunk: dataBox.dataValue.chunk
    editing: editor.isEditting

    property int editTargetRow: -1
    property int editTargetRole: -1

    //The editor host for whichever window this box is in — see
    //CoreClickTextInput._shadowEditor
    readonly property ShadowEditorHost _shadowEditor:
        (WindowOverlay.overlay as AppOverlay)?.shadowEditor ?? null

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

    //! Starts the trip's next chunk, or focuses the empty one it already has
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
                               //editor.openEditor() handed off to the host, so
                               //the typed character goes in through it
                               dataBox._shadowEditor?.setEditorText(event.text)
                               dataBox._shadowEditor?.clearSelection()
                           }
                       }

    QQ.Keys.onSpacePressed: {
        if(dataBox.startsChunkOnSpace) {
            dataBox.addNewChunk();
        }
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
            //The cell the edit commits to, not the reading behind it: a splay's
            //cells are the editor's own and name no chunk role
            dataBox.editTargetRow = dataBox.listViewIndex
            dataBox.editTargetRole = dataBox.cellRole
            dataBox.state = 'MiddleTyping';
        }

        //The editor's own handler takes the click before the cell's does, so a
        //splay move waiting for a station to land on is finished from here
        onClicked: {
            if(!dataBox.shouldTakeTap()) {
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

    //While this cell is being typed into, its keys come from the shared editor
    //rather than from the cell itself.
    //
    //Gated by state instead of installed by a QQ.PropertyChanges, which is how
    //this used to work, for two reasons: a PropertyChanges resolves its target
    //exactly once, so it cannot follow _shadowEditor; and reaching through a
    //property with grouped syntax needs the host's type fully resolved while
    //this file is still being compiled, which is order-dependent between two
    //types that name each other and fails intermittently with "Invalid grouped
    //property access". A Connections target is an ordinary binding, so neither
    //applies.
    QQ.Connections {
        target: dataBox._shadowEditor
        enabled: dataBox.state === "MiddleTyping"

        function onPressKeyPressed(pressKeyEvent) {
            if(pressKeyEvent.key === Qt.Key_Tab ||
               pressKeyEvent.key === 1 + Qt.Key_Tab ||
               pressKeyEvent.key === Qt.Key_Space)
            {
                var commited = editor.commitChanges()
                if(!commited) { return; }
            }

            if(pressKeyEvent.key === Qt.Key_Space && dataBox.startsChunkOnSpace) {
                dataBox.addNewChunk();
            }

            //Tab to the next entry on enter
            if(pressKeyEvent.key === Qt.Key_Enter ||
               pressKeyEvent.key === Qt.Key_Return) {

                dataBox.handleNextTab()
                pressKeyEvent.accepted = true;
            }

            //Use the default key handling the host provides
            dataBox._shadowEditor.defaultKeyHandling();

            //Handle the tabbing
            dataBox.handleTab(pressKeyEvent);

            if(pressKeyEvent.accepted) {
                //Have the editor commit changes
                dataBox.state = ''; //Default state
            }
        }

        function onEditorFocusChanged(editorFocus) {
            if(!editorFocus) {
                dataBox.state = '';
            }
        }

        function onEscapePressed() {
            dataBox.state = ''; //Default state
            dataBox.forceActiveFocus()
        }

        function onEnterPressed() {
            var commited = editor.commitChanges();
            if(commited) {
                dataBox.forceActiveFocus()
            }
        }
    }

    states: [

        QQ.State {
            name: "MiddleTyping"

            QQ.PropertyChanges {
                dataBox.z: 1
            }
        }
    ]
}
