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

    required property cwSurveyEditorBoxData dataValue
    required property RemovePreview removePreview

    active: true

    sourceComponent: QQ.Component {
        QC.Menu {
            id: menuId

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
                title: "Remove shot"
                enabled: dataValue.chunk !== null

                QC.MenuItem {
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

            QC.MenuSeparator { }

            RemoveChunkMenuItem {
                chunk: dataValue.chunk
            }
        }
    }
}
