/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// The Splays column's cell. It holds no reading, so it takes the grid cell's
// look and focus behavior from SurveyEditorCell and adds nothing but a count —
// no editor, no error model. Focusing it and pressing Enter or Space opens the
// station's splay cluster.
SurveyEditorCell {
    id: splaysBox
    objectName: "splaysBox." + splaysBox.listViewIndex

    required property cwSurveyEditorRowIndex rowIndex

    //The station's name cell, which is what the station menu acts through
    required property cwSurveyEditorBoxData stationData

    required property int splayCount
    required property bool splaysExpanded

    cellRole: SurveyEditorCellIndex.StationSplaysCell
    indexInChunk: splaysBox.rowIndex.indexInChunk
    chunk: splaysBox.rowIndex.chunk

    //The same menu the rest of the station's row pops, so the column the
    //cluster hangs from offers the station's actions like every other
    rightClickMenuLoader: stationMenuId

    function toggleExpanded() {
        splaysBox.model.toggleSplaysExpanded(splaysBox.rowIndex)
    }

    QQ.Keys.onPressed: (event) => {
                           if(splaysBox.handleNavigationKey(event)) {
                               return
                           }
                           switch(event.key) {
                           case Qt.Key_Enter:
                           case Qt.Key_Return:
                           case Qt.Key_Space:
                               splaysBox.toggleExpanded()
                               event.accepted = true
                               break
                           }
                       }

    onTapped: splaysBox.toggleExpanded()

    StationMenu {
        id: stationMenuId
        model: splaysBox.model
        dataValue: splaysBox.stationData
        listViewIndex: splaysBox.listViewIndex
        removePreview: splaysBox.removePreview
    }

    //Most stations carry no splays, so the chip stays uninstantiated rather than
    //laying out text it will never show
    QQ.Loader {
        active: splaysBox.splayCount > 0
        anchors.centerIn: parent

        sourceComponent: QQ.Rectangle {
            objectName: "splayChip"

            width: chipRowId.implicitWidth + Theme.delegatePadding * 2
            height: chipRowId.implicitHeight + Theme.delegatePadding

            radius: height * 0.5
            color: Theme.splaySurface
            border.color: Theme.splayBorder
            border.width: 1

            QQ.Row {
                id: chipRowId
                anchors.centerIn: parent
                spacing: Theme.flowSpacing

                //A glyph rather than the chevron SVG the rest of the app uses:
                //Icon colorizes through a MultiEffect layer, which costs an
                //offscreen texture in every recycled row and renders as nothing
                //in offscreen tests
                QC.Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "▶"
                    color: Theme.splayText
                    font.pixelSize: Theme.fontSizeCaption
                    rotation: splaysBox.splaysExpanded ? 90 : 0

                    QQ.Behavior on rotation {
                        QQ.NumberAnimation { duration: 120 }
                    }
                }

                QC.Label {
                    objectName: "splayChipCount"
                    anchors.verticalCenter: parent.verticalCenter
                    text: splaysBox.splayCount
                    color: Theme.splayText
                    font.pixelSize: Theme.fontSizeSmall
                }
            }
        }
    }
}
