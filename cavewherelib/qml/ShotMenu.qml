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

    required property cwSurveyEditorBoxData dataValue
    required property RemovePreview removePreview

    active: false

    sourceComponent: QQ.Component {
        QC.Menu {
            id: menuId
            objectName: "shotMenuRoot"

            function stationLabel(index) {
                if(dataValue.chunk === null) {
                    return "empty station"
                }
                let name = dataValue.chunk.data(SurveyChunk.StationNameRole, index)
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
                             && dataValue.chunk.canRemoveShot(dataValue.indexInChunk, SurveyChunk.Above)
                    onHighlightedChanged: {
                        if(highlighted) {
                            previewRemoveShot(SurveyChunk.Above)
                        } else {
                            clearRemovePreview()
                        }
                    }
                    onTriggered: {
                        clearRemovePreview()
                        dataValue.chunk.removeShot(dataValue.indexInChunk, SurveyChunk.Above)
                    }
                }
                QC.MenuItem {
                    objectName: "shotMenuRemoveBelow"
                    text: "and " + stationLabel(dataValue.indexInChunk + 1)
                    enabled: dataValue.chunk !== null
                             && dataValue.chunk.canRemoveShot(dataValue.indexInChunk, SurveyChunk.Below)
                    onHighlightedChanged: {
                        if(highlighted) {
                            previewRemoveShot(SurveyChunk.Below)
                        } else {
                            clearRemovePreview()
                        }
                    }
                    onTriggered: {
                        clearRemovePreview()
                        dataValue.chunk.removeShot(dataValue.indexInChunk, SurveyChunk.Below)
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
                    text: "Shot above"
                    enabled: dataValue.chunk !== null
                    onTriggered: {
                        dataValue.chunk.insertShot(dataValue.indexInChunk, SurveyChunk.Above)
                    }
                }

                QC.MenuItem {
                    objectName: "shotMenuInsertBelow"
                    text: "Shot below"
                    enabled: dataValue.chunk !== null
                    onTriggered: {
                        dataValue.chunk.insertShot(dataValue.indexInChunk, SurveyChunk.Below)
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
