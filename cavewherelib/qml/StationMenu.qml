/**************************************************************************
**
**    Copyright (C) 2026
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

QQ.Loader {
    id: stationMenuLoader
    objectName: "stationMenuLoader"

    required property SurveyEditorModel model
    required property cwSurveyEditorBoxData dataValue
    required property int listViewIndex
    required property RemovePreview removePreview

    //The station's splay cluster, set by the cell that stands for it. A menu
    //popped from a cell that carries no cluster leaves the count at zero and
    //shows no splay items at all
    property cwSurveyEditorRowIndex splayClusterRow
    property int splayCount: 0
    property SplayRemoveAskBox splayRemoveChallenge: null

    active: false

    sourceComponent: QQ.Component {
        QC.Menu {
            id: menuId
            objectName: "stationMenuRoot"

            //The cluster items only belong to the cell that stands for the
            //cluster, and only while there is something in it to act on
            readonly property bool offersSplayActions: stationMenuLoader.splayCount > 0

            //Losing the cluster is the one action that asks first, so it needs
            //something to ask with
            readonly property bool offersSplayDelete:
                menuId.offersSplayActions
                && stationMenuLoader.splayRemoveChallenge !== null

            function stationLabel(index) {
                if(dataValue.chunk === null || stationMenuLoader.model === null) {
                    return "empty station"
                }
                let row = stationMenuLoader.model.modelRowForCellRole(
                            dataValue.chunk,
                            index,
                            SurveyEditorCellIndex.StationNameCell)
                if(row < 0) {
                    return "empty station"
                }
                let stationBoxData = stationMenuLoader.model.data(
                            stationMenuLoader.model.index(row, 0),
                            SurveyEditorModel.StationNameRole)
                let name = stationBoxData.reading.value
                if(name === "") {
                    return "empty station"
                }
                return name
            }

            function previewRemoveStation(direction) {
                if(removePreview === null || dataValue.chunk === null) {
                    return
                }
                removePreview.previewStation(dataValue.chunk, dataValue.indexInChunk, direction)
            }

            function clearRemovePreview() {
                if(removePreview !== null) {
                    removePreview.clear()
                }
            }

            function splayCountLabel() {
                if(stationMenuLoader.splayCount === 1) {
                    return "the splay"
                }
                return "all " + stationMenuLoader.splayCount + " splays"
            }

            //Drops the challenge under the cell the menu was popped from, kept
            //inside the table so the Splays column, which sits against the
            //table's right edge, doesn't push it off the side
            function askToDeleteSplays() {
                let challenge = stationMenuLoader.splayRemoveChallenge
                let cell = stationMenuLoader.parent
                let pos = cell.mapToItem(challenge.parent, 0, cell.height)

                challenge.rowIndexToRemove = stationMenuLoader.splayClusterRow
                challenge.removeName = splayCountLabel()
                        + " from " + stationLabel(dataValue.indexInChunk)

                //The box is as wide as the name it just took, and that width
                //only settles on the next polish, so the clamp is a binding
                //rather than a number measured against the width the box
                //happened to have while it was hidden
                challenge.x = Qt.binding(function() {
                    return Math.max(0, Math.min(pos.x, challenge.parent.width - challenge.width))
                })
                challenge.y = pos.y
                challenge.show()
            }

            onClosed: {
                clearRemovePreview()
            }

            QC.MenuItem {
                objectName: "stationMenuMoveSplays"
                text: "Move " + menuId.splayCountLabel() + " to…"
                visible: menuId.offersSplayActions
                height: visible ? implicitHeight : 0
                onTriggered: stationMenuLoader.model.startSplayMove(stationMenuLoader.splayClusterRow, true)
            }

            QC.MenuItem {
                objectName: "stationMenuDeleteSplays"
                text: "Delete " + menuId.splayCountLabel()
                visible: menuId.offersSplayDelete
                height: visible ? implicitHeight : 0
                onTriggered: askToDeleteSplays()
            }

            QC.MenuSeparator {
                objectName: "stationMenuSplaySeparator"
                visible: menuId.offersSplayActions
                height: visible ? implicitHeight : 0
            }

            QC.Menu {
                objectName: "stationMenuRemoveMenu"
                title: "Remove " + stationLabel(dataValue.indexInChunk)
                enabled: dataValue.chunk !== null

                QC.MenuItem {
                    objectName: "stationMenuRemoveAbove"
                    text: "and the shot <b>above</b>"
                    enabled: dataValue.chunk !== null
                             && stationMenuLoader.model !== null
                             && stationMenuLoader.model.canRemoveStationAt(stationMenuLoader.model.cellIndex(listViewIndex, dataValue.chunkDataRole), SurveyChunk.Above)
                    onHighlightedChanged: {
                        if(highlighted) {
                            previewRemoveStation(SurveyChunk.Above)
                        } else {
                            clearRemovePreview()
                        }
                    }
                    onTriggered: {
                        clearRemovePreview()
                        stationMenuLoader.model.removeStationAt(stationMenuLoader.model.cellIndex(listViewIndex, dataValue.chunkDataRole), SurveyChunk.Above)
                    }
                }
                QC.MenuItem {
                    objectName: "stationMenuRemoveBelow"
                    text: "and the shot <b>below</b>"
                    enabled: dataValue.chunk !== null
                             && stationMenuLoader.model !== null
                             && stationMenuLoader.model.canRemoveStationAt(stationMenuLoader.model.cellIndex(listViewIndex, dataValue.chunkDataRole), SurveyChunk.Below)
                    onHighlightedChanged: {
                        if(highlighted) {
                            previewRemoveStation(SurveyChunk.Below)
                        } else {
                            clearRemovePreview()
                        }
                    }
                    onTriggered: {
                        clearRemovePreview()
                        stationMenuLoader.model.removeStationAt(stationMenuLoader.model.cellIndex(listViewIndex, dataValue.chunkDataRole), SurveyChunk.Below)
                    }
                }


            }

            QC.Menu {
                objectName: "stationMenuInsertMenu"
                title: "Insert"
                enabled: dataValue.chunk !== null

                QC.MenuItem {
                    objectName: "stationMenuInsertAbove"
                    text: "Station above"
                    enabled: dataValue.chunk !== null
                             && stationMenuLoader.model !== null
                             && stationMenuLoader.model.canInsertStationAt(stationMenuLoader.model.cellIndex(listViewIndex, dataValue.chunkDataRole), SurveyChunk.Above)
                    onTriggered: {
                        stationMenuLoader.model.insertStationAt(stationMenuLoader.model.cellIndex(listViewIndex, dataValue.chunkDataRole), SurveyChunk.Above)
                    }
                }

                QC.MenuItem {
                    objectName: "stationMenuInsertBelow"
                    text: "Station below"
                    enabled: dataValue.chunk !== null
                             && stationMenuLoader.model !== null
                             && stationMenuLoader.model.canInsertStationAt(stationMenuLoader.model.cellIndex(listViewIndex, dataValue.chunkDataRole), SurveyChunk.Below)
                    onTriggered: {
                        stationMenuLoader.model.insertStationAt(stationMenuLoader.model.cellIndex(listViewIndex, dataValue.chunkDataRole), SurveyChunk.Below)
                    }
                }
            }

            QC.MenuSeparator { }

            RemoveChunkMenuItem {
                objectName: "stationMenuRemoveChunk"
                chunk: dataValue.chunk
                removePreview: stationMenuLoader.removePreview
            }
        }
    }
}
