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
    //alternating bands
    required property int indexInChunk

    //The chunk the row belongs to. A recycled delegate holds no chunk between
    //rows, and a cell with nothing behind it draws no background
    property SurveyChunk chunk: null

    //The model row the cell sits on, for the actions that name a row rather
    //than a reading — expanding a splay cluster, landing a splay move. A cell
    //that offers none of them leaves it at the empty row it starts with
    property cwSurveyEditorRowIndex rowIndex

    //A cell a splay move can land on. While one is armed, a tap here moves the
    //splays onto this cell's station instead of taking focus; every other
    //cell's tap calls the move off
    property bool acceptsSplayMove: false

    //How the cell's background reads: shots alternate in bands, station cells
    //take one flat fill, and a splay cell takes the accent that keeps a cluster
    //reading as a group. Which of the three a cell is follows from its column,
    //so the model answers it
    readonly property bool stationCell: cell.chunk !== null
                                        && cell.model !== null
                                        && cell.model.isStationCell(cell.cellRole)
    readonly property bool shotCell: cell.chunk !== null
                                     && cell.model !== null
                                     && cell.model.isShotCell(cell.cellRole)
    readonly property bool splayCell: cell.chunk !== null
                                      && cell.model !== null
                                      && cell.model.isSplayCell(cell.cellRole)

    //The shot or station of the cell's chunk that holds the keyboard, or -1.
    //A splay row's index is the station it hangs from, so focus in a splay
    //selects its station
    readonly property bool focusInChunk: cell.chunk !== null
                                         && cell.model !== null
                                         && cell.model.focusedRowIndex.chunk === cell.chunk
    readonly property int selectedShotIndex: cell.focusInChunk
                                             && cell.model.isShotCell(cell.model.focusedRole)
                                             ? cell.model.focusedRowIndex.indexInChunk : -1
    readonly property int selectedStationIndex: cell.focusInChunk
                                                && (cell.model.isStationCell(cell.model.focusedRole)
                                                    || cell.model.isSplayCell(cell.model.focusedRole))
                                                ? cell.model.focusedRowIndex.indexInChunk : -1

    readonly property bool stationSelected: (cell.stationCell || cell.splayCell)
                                            && cell.selectedStationIndex === cell.indexInChunk

    //A selected shot reaches into the lower half of its from station's cells
    //and the upper half of its to station's, the halves its own cells cover
    readonly property bool lowerHalfSelected: cell.stationCell
                                              && cell.selectedShotIndex === cell.indexInChunk
    readonly property bool upperHalfSelected: cell.stationCell
                                              && cell.selectedShotIndex >= 0
                                              && cell.selectedShotIndex === cell.indexInChunk - 1

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

    //! The fill a cell takes at rest: shots alternate by shot, a splay takes
    //! its accent, and a station one flat fill
    function restFill(): QQ.color {
        if(cell.shotCell) {
            return cell.indexInChunk % 2 === 0 ? Theme.background : Theme.rowAlternate
        }
        if(cell.splayCell) {
            return Theme.splaySurface
        }
        return Theme.background
    }

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

        //A cell built while the view still has it off-scene can't take the
        //focus, and asking again costs nothing once it can — which is what
        //carries the caret into a splay cluster opening under it. An open
        //editor already holds the focus for the cell, so it keeps it
        if(selected && !cell.activeFocus && !cell.editing) {
            cell.forceActiveFocus()
        }

        cell.focus = selected
    }

    function takeFocus() {
        //A recycled delegate can be tapped with its model already gone
        if(cell.listViewIndex < 0 || cell.model === null) {
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

    //! Finishes an armed splay move: this cell's station takes the splays, or
    //! anywhere else calls the move off
    function handleSplayMoveTap() {
        if(cell.acceptsSplayMove && cell.model.isSplayMoveTarget(cell.rowIndex)) {
            cell.model.commitSplayMove(cell.rowIndex)
        } else {
            cell.model.cancelSplayMove()
        }
    }

    //! The arbitration every left tap inside the cell shares, whether it landed
    //! on the cell itself or on a control a subtype put there: an armed splay
    //! move takes the tap. Answers whether the tap is the caller's to act on.
    //! Callers that focus the cell some other way than the model's focus, such
    //! as an editor taking the keyboard, ask this and then focus themselves
    function shouldTakeTap(): bool {
        if(cell.model === null) {
            return false
        }

        if(cell.model.splayMoveActive) {
            cell.handleSplayMoveTap()
            return false
        }

        return true
    }

    //! shouldTakeTap with the focus step the plain cells share. Answers whether
    //! the caller should carry on with what the tap was for
    function handleCellTap(): bool {
        if(!cell.shouldTakeTap()) {
            return false
        }

        cell.takeFocus()
        return true
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

    //The view builds a delegate before it parents it into the scene, so a cell
    //that arrives already focused asks for the focus again once it has landed
    onParentChanged: cell.syncFocusState()

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

    QQ.Rectangle {
        id: backgroundId
        objectName: "cellFill"
        anchors.fill: parent
        visible: cell.stationCell || cell.shotCell || cell.splayCell
        color: {
            const selected = cell.shotCell ? cell.selectedShotIndex === cell.indexInChunk
                                           : cell.stationSelected
            return selected ? Theme.rowCurrent : cell.restFill()
        }
    }

    QQ.Rectangle {
        id: selectedShotHalfId
        objectName: "cellSelectedShotHalf"
        anchors.left: parent.left
        anchors.right: parent.right
        y: cell.upperHalfSelected ? 0 : parent.height / 2
        height: parent.height / 2
        visible: cell.upperHalfSelected || cell.lowerHalfSelected
        color: Theme.rowCurrent
    }

    QQ.Rectangle {
        id: borderId
        anchors.fill: parent
        border.color: Theme.border
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
                            //A right click asks for the menu whatever else is
                            //going on, so it skips the move arbitration
                            if(button === Qt.RightButton) {
                                cell.takeFocus()
                                cell.rightTapped()
                                return
                            }

                            if(cell.handleCellTap()) {
                                cell.tapped()
                            }
                        }
    }
}
