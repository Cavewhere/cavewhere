/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

// One cell of the survey editor's grid. It owns everything a cell has whether
// or not it holds a reading: where it sits in the model, whether the model's
// focus is on it, the keys that move that focus, and the chrome that draws it.
// Subtypes add what the cell holds — DataBox a reading and its editor,
// SplaysBox a station's splay count.
QQ.Item {
    id: cell

    required property SurveyEditorModel model

    //Index in the ListView
    required property int listViewIndex

    //The list the cell scrolls in. The view moves focus onto the row delegate
    //when its currentIndex changes, which takes focus away from the cell
    required property QQ.ListView view

    //Which column of the row this is
    required property int cellRole

    //Which station or shot of the chunk the cell belongs to, for the
    //alternating background
    required property int indexInChunk
    readonly property bool evenRow: cell.indexInChunk % 2 === 0

    //The chunk the row belongs to. A recycled delegate holds no chunk between
    //rows, and a cell with nothing behind it draws no background
    property SurveyChunk chunk: null

    //How the cell's background reads: a station cell gets the gradient that
    //ties a station to the shots around it, a shot cell a flat fill. Which of
    //the two a cell is follows from its column, so the model answers it
    readonly property bool stationCell: cell.chunk !== null
                                        && cell.model !== null
                                        && cell.model.isStationCell(cell.cellRole)
    readonly property bool shotCell: cell.chunk !== null
                                     && cell.model !== null
                                     && cell.model.isShotCell(cell.cellRole)

    //Hovering a remove action strikes through every cell the removal takes
    //with it, whether or not the cell holds a reading
    property RemovePreview removePreview: null
    readonly property bool removePreviewActive: {
        if(cell.removePreview === null || cell.removePreview.chunk === null) {
            return false
        }
        if(cell.chunk !== cell.removePreview.chunk) {
            return false
        }
        if(cell.stationCell) {
            return cell.removePreview.previewChunkRemoval
                    || cell.removePreview.stationIndex === cell.indexInChunk
        }
        if(cell.shotCell) {
            return cell.removePreview.previewChunkRemoval
                    || cell.removePreview.shotIndex === cell.indexInChunk
        }
        return false
    }

    //The menu a right click pops. A cell that offers no actions leaves it null
    property QQ.Loader rightClickMenuLoader: null

    //Something the subtype owns has the cell open, such as an editor. The focus
    //ring stays lit for it, and it keeps the taps it is already receiving
    property bool editing: false
    readonly property bool highlightVisible: cell.shouldHaveFocus() || cell.editing

    property TripCalibration calibration: null
    readonly property bool frontSights: calibration !== null && calibration.frontSights
    readonly property bool backSights: calibration !== null && calibration.backSights

    //A left click on the cell, after it has taken focus
    signal tapped()
    signal rightTapped()
    signal tabPressed()

    function shouldHaveFocus(): bool {
        //A recycled delegate re-evaluates this with its model already gone
        if(cell.listViewIndex < 0 || cell.model === null) {
            return false
        }
        return cell.model.focusedRow === cell.listViewIndex
                && cell.model.focusedRole === cell.cellRole
    }

    function syncFocusState() {
        const selected = cell.shouldHaveFocus()
        if(cell.focus === selected) {
            return
        }
        if(selected) {
            cell.forceActiveFocus()
        }
        cell.focus = selected
    }

    function takeFocus() {
        if(cell.listViewIndex < 0) {
            return
        }
        cell.model.setFocusedCell(cell.model.cellIndex(cell.listViewIndex, cell.cellRole))
    }

    function moveFocus(navKey: int) {
        const currentCell = cell.model.cellIndex(cell.listViewIndex, cell.cellRole)
        cell.model.setFocusedCell(cell.model.nextCell(currentCell,
                                                      navKey,
                                                      cell.frontSights,
                                                      cell.backSights))
    }

    function handleNextTab() {
        cell.moveFocus(SurveyEditorModel.Tab)
    }

    //! Moves the focus for tab and shift-tab, and answers whether the key was one
    function handleTab(eventKey): bool {
        if(eventKey.key === Qt.Key_Tab) {
            cell.tabPressed();
            cell.handleNextTab();
        } else if(eventKey.key === 1 + Qt.Key_Tab) {
            //Shift tab -- 1 + Qt.Key_Tab is a hack but it works
            cell.moveFocus(SurveyEditorModel.BackTab)
        } else {
            return false
        }
        eventKey.accepted = true
        return true
    }

    //! Moves the focus for the arrow keys, and answers whether the key was one
    function handleArrowKey(eventKey): bool {
        switch(eventKey.key) {
        case Qt.Key_Left:
            cell.moveFocus(SurveyEditorModel.Left)
            break
        case Qt.Key_Right:
            cell.moveFocus(SurveyEditorModel.Right)
            break
        case Qt.Key_Up:
            cell.moveFocus(SurveyEditorModel.Up)
            break
        case Qt.Key_Down:
            cell.moveFocus(SurveyEditorModel.Down)
            break
        default:
            return false
        }
        eventKey.accepted = true
        return true
    }

    //! Runs every key that moves the focus between cells, so a subtype's own
    //! Keys.onPressed starts with one call rather than the whole protocol
    function handleNavigationKey(eventKey): bool {
        return cell.handleTab(eventKey) || cell.handleArrowKey(eventKey)
    }

    onFocusChanged: {
        if(cell.focus) {
            cell.takeFocus()
        }
    }

    onRightTapped: {
        if(cell.rightClickMenuLoader !== null) {
            cell.rightClickMenuLoader.active = true
            cell.rightClickMenuLoader.item.popup()
        }
    }

    onCellRoleChanged: cell.syncFocusState()

    onListViewIndexChanged: cell.syncFocusState()

    QQ.Component.onCompleted: cell.syncFocusState()

    //We need to watch on currentIndex changed because the view
    //set the focus on the row delegate. This will disable the focus on the
    //correct cell.
    QQ.Connections {
        target: cell.view
        function onCurrentIndexChanged() {
            cell.syncFocusState()
            if(cell.focus) {
                cell.forceActiveFocus()
            }
        }
    }

    QQ.Connections {
        target: cell.model
        function onFocusedRowChanged() {
            cell.syncFocusState()
        }
        function onFocusedRoleChanged() {
            cell.syncFocusState()
        }
    }

    QQ.Gradient {
        id: stationGradientId

        QQ.GradientStop {
            position: cell.evenRow ? 1.0 : 0.0
            color: Theme.surfaceRaised
        }
        QQ.GradientStop {
            position: cell.evenRow ? 0.4 : 0.6
            color: Theme.surface
        }
    }

    //A station's gradient and a shot's flat fill are the same rectangle, since
    //a cell is never both, and a gradient overrides the color
    QQ.Rectangle {
        id: backgroundId
        anchors.fill: parent
        visible: cell.stationCell || cell.shotCell
        color: cell.evenRow ? Theme.surfaceRaised : Theme.surface
        gradient: cell.stationCell ? stationGradientId : null
    }

    QQ.Rectangle {
        id: borderId
        anchors.fill: parent
        border.color: Theme.borderSubtle
        border.width: 1
        color: Theme.transparent
    }

    QQ.Rectangle {
        id: highlightId
        anchors.fill: parent
        anchors.margins: 1
        border.color: Theme.border
        border.width: 1
        color: Theme.highlight
        visible: cell.highlightVisible
    }

    QQ.Rectangle {
        id: removePreviewLine
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: 2
        color: Theme.text
        visible: cell.removePreviewActive
        z: 2
    }

    QQ.TapHandler {
        enabled: !cell.editing
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onSingleTapped: (eventPoint, button) => {
                            cell.takeFocus()
                            if(button === Qt.RightButton) {
                                cell.rightTapped()
                            } else {
                                cell.tapped()
                            }
                        }
    }
}
