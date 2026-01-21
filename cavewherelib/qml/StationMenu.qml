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
                title: "Remove " + stationLabel(dataValue.indexInChunk)
                enabled: dataValue.chunk !== null

                QC.MenuItem {
                    text: "and the shot <b>above</b>"
                    enabled: dataValue.chunk !== null
                             && dataValue.chunk.canRemoveStation(dataValue.indexInChunk, SurveyChunk.Above)
                    onHighlightedChanged: {
                        if(highlighted) {
                            previewRemoveStation(SurveyChunk.Above)
                        } else {
                            clearRemovePreview()
                        }
                    }
                    onTriggered: {
                        clearRemovePreview()
                        dataValue.chunk.removeStation(dataValue.indexInChunk, SurveyChunk.Above)
                    }
                }
                QC.MenuItem {
                    text: "and the shot <b>below</b>"
                    enabled: dataValue.chunk !== null
                             && dataValue.chunk.canRemoveStation(dataValue.indexInChunk, SurveyChunk.Below)
                    onHighlightedChanged: {
                        if(highlighted) {
                            previewRemoveStation(SurveyChunk.Below)
                        } else {
                            clearRemovePreview()
                        }
                    }
                    onTriggered: {
                        clearRemovePreview()
                        dataValue.chunk.removeStation(dataValue.indexInChunk, SurveyChunk.Below)
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
