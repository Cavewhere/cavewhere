/**************************************************************************
**
**    Copyright (C) 2026
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

QQ.Loader {
    id: shotMenuLoader
    objectName: "shotMenuLoader"

    required property SurveyEditorModel model
    required property cwSurveyEditorBoxData dataValue
    required property int listViewIndex
    required property RemovePreview removePreview

    active: false

    sourceComponent: QQ.Component {
        QC.Menu {
            id: menuId
            objectName: "shotMenuRoot"

            function stationLabel(index) {
                if(dataValue.chunk === null || shotMenuLoader.model === null) {
                    return "empty station"
                }
                let row = shotMenuLoader.model.modelRowForChunkRole(
                            dataValue.chunk,
                            index,
                            SurveyChunk.StationNameRole)
                if(row < 0) {
                    return "empty station"
                }
                let stationBoxData = shotMenuLoader.model.data(
                            shotMenuLoader.model.index(row, 0),
                            SurveyEditorModel.StationNameRole)
                let name = stationBoxData.reading.value
                if(name === "") {
                    return "empty station"
                }
                return name
            }

            function previewRemoveShot(direction) {
                if(removePreview === null || dataValue.chunk === null) {
                    return
                }
                removePreview.previewShot(dataValue.chunk, dataValue.indexInChunk, direction)
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
                objectName: "shotMenuRemoveMenu"
                title: "Remove shot"
                enabled: dataValue.chunk !== null

                QC.MenuItem {
                    objectName: "shotMenuRemoveAbove"
                    text: "and " + stationLabel(dataValue.indexInChunk)
                    enabled: dataValue.chunk !== null
                             && shotMenuLoader.model !== null
                             && shotMenuLoader.model.canRemoveShotAt(listViewIndex, dataValue.chunkDataRole, SurveyChunk.Above)
                    onHighlightedChanged: {
                        if(highlighted) {
                            previewRemoveShot(SurveyChunk.Above)
                        } else {
                            clearRemovePreview()
                        }
                    }
                    onTriggered: {
                        clearRemovePreview()
                        shotMenuLoader.model.removeShotAt(listViewIndex, dataValue.chunkDataRole, SurveyChunk.Above)
                    }
                }
                QC.MenuItem {
                    objectName: "shotMenuRemoveBelow"
                    text: "and " + stationLabel(dataValue.indexInChunk + 1)
                    enabled: dataValue.chunk !== null
                             && shotMenuLoader.model !== null
                             && shotMenuLoader.model.canRemoveShotAt(listViewIndex, dataValue.chunkDataRole, SurveyChunk.Below)
                    onHighlightedChanged: {
                        if(highlighted) {
                            previewRemoveShot(SurveyChunk.Below)
                        } else {
                            clearRemovePreview()
                        }
                    }
                    onTriggered: {
                        clearRemovePreview()
                        shotMenuLoader.model.removeShotAt(listViewIndex, dataValue.chunkDataRole, SurveyChunk.Below)
                    }
                }
            }

            // QC.MenuSeparator { }

            QC.Menu {
                objectName: "shotMenuInsertMenu"
                title: "Insert"
                enabled: dataValue.chunk !== null

                QC.MenuItem {
                    objectName: "shotMenuInsertAbove"
                    text: "Above"
                    enabled: dataValue.chunk !== null
                             && shotMenuLoader.model !== null
                             && shotMenuLoader.model.canInsertShotAt(listViewIndex, dataValue.chunkDataRole, SurveyChunk.Above)
                    onTriggered: {
                        shotMenuLoader.model.insertShotAt(listViewIndex, dataValue.chunkDataRole, SurveyChunk.Above)
                    }
                }

                QC.MenuItem {
                    objectName: "shotMenuInsertBelow"
                    text: "Below"
                    enabled: dataValue.chunk !== null
                             && shotMenuLoader.model !== null
                             && shotMenuLoader.model.canInsertShotAt(listViewIndex, dataValue.chunkDataRole, SurveyChunk.Below)
                    onTriggered: {
                        shotMenuLoader.model.insertShotAt(listViewIndex, dataValue.chunkDataRole, SurveyChunk.Below)
                    }
                }
            }

            QC.MenuSeparator { }

            RemoveChunkMenuItem {
                objectName: "shotMenuRemoveChunk"
                chunk: dataValue.chunk
                removePreview: shotMenuLoader.removePreview
            }
        }
    }
}
