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
    required property RemovePreview removePreview

    active: false

    sourceComponent: QQ.Component {
        QC.Menu {
            id: menuId
            objectName: "stationMenuRoot"

            function stationLabel(index) {
                if(dataValue.chunk === null || stationMenuLoader.model === null) {
                    return "empty station"
                }
                let row = stationMenuLoader.model.toModelRow(
                            stationMenuLoader.model.rowIndex(
                                dataValue.chunk,
                                index,
                                SurveyEditorRowIndex.StationRow))
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

            onClosed: {
                clearRemovePreview()
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
                             && stationMenuLoader.model.canRemoveStation(dataValue.boxIndex, SurveyChunk.Above)
                    onHighlightedChanged: {
                        if(highlighted) {
                            previewRemoveStation(SurveyChunk.Above)
                        } else {
                            clearRemovePreview()
                        }
                    }
                    onTriggered: {
                        clearRemovePreview()
                        stationMenuLoader.model.removeStation(dataValue.boxIndex, SurveyChunk.Above)
                    }
                }
                QC.MenuItem {
                    objectName: "stationMenuRemoveBelow"
                    text: "and the shot <b>below</b>"
                    enabled: dataValue.chunk !== null
                             && stationMenuLoader.model !== null
                             && stationMenuLoader.model.canRemoveStation(dataValue.boxIndex, SurveyChunk.Below)
                    onHighlightedChanged: {
                        if(highlighted) {
                            previewRemoveStation(SurveyChunk.Below)
                        } else {
                            clearRemovePreview()
                        }
                    }
                    onTriggered: {
                        clearRemovePreview()
                        stationMenuLoader.model.removeStation(dataValue.boxIndex, SurveyChunk.Below)
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
                             && stationMenuLoader.model.canInsertStation(dataValue.boxIndex, SurveyChunk.Above)
                    onTriggered: {
                        stationMenuLoader.model.insertStation(dataValue.boxIndex, SurveyChunk.Above)
                    }
                }

                QC.MenuItem {
                    objectName: "stationMenuInsertBelow"
                    text: "Station below"
                    enabled: dataValue.chunk !== null
                             && stationMenuLoader.model !== null
                             && stationMenuLoader.model.canInsertStation(dataValue.boxIndex, SurveyChunk.Below)
                    onTriggered: {
                        stationMenuLoader.model.insertStation(dataValue.boxIndex, SurveyChunk.Below)
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
