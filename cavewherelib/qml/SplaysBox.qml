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

    //The station's name cell, which is what the station menu acts through
    required property cwSurveyEditorBoxData stationData

    required property int splayCount
    required property bool splaysExpanded

    //The chunk's trailing blank station stands for a station that hasn't been
    //surveyed yet, so it can carry no splays until it's named
    required property bool isVirtual

    //True while the pointer is over the cell, which is when it offers to take
    //a station's first splay
    readonly property bool hovered: hoverHandlerId.hovered

    //The chip and its ⋯ take the taps that land on them, so a tap the cell
    //itself receives on a station that has them is a tap that missed both — and
    //it leaves the cluster as the user found it. A station with no splays
    //carries neither control, so nothing there can take a tap and the whole
    //cell is the target its "+" button marks
    readonly property bool clusterHasControls: splaysBox.splayCount > 0

    //The table's one challenge for deleting a whole cluster, owned by
    //SurveyEditor so it outlives the recycled row that asks for it
    required property SplayRemoveAskBox splayRemoveChallenge

    cellRole: SurveyEditorCellIndex.StationSplaysCell
    indexInChunk: splaysBox.rowIndex.indexInChunk
    chunk: splaysBox.rowIndex.chunk

    //The same menu the rest of the station's row pops, so the column the
    //cluster hangs from offers the station's actions like every other
    rightClickMenuLoader: stationMenuId

    function toggleExpanded() {
        //A recycled delegate re-evaluates this with its model already gone
        if(splaysBox.model === null || splaysBox.isVirtual) {
            return
        }

        const wasExpanded = splaysBox.splaysExpanded
        splaysBox.model.toggleSplaysExpanded(splaysBox.rowIndex)

        //A station with no splays opens on its blank row alone, and the caret
        //goes straight into it — manual entry when the download dies is tab to
        //Splays, Enter, type
        if(!wasExpanded && splaysBox.splayCount === 0) {
            splaysBox.model.setFocusedCell(splaysBox.model.splayEntryCell(splaysBox.rowIndex))
        }
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

    onTapped: {
        if(!splaysBox.clusterHasControls) {
            splaysBox.toggleExpanded()
        }
    }

    QQ.HoverHandler {
        id: hoverHandlerId
    }

    //A station with no splays shows nothing until the pointer is over it, and
    //then the button that says splays can be typed here
    QQ.Loader {
        anchors.centerIn: parent
        active: !splaysBox.clusterHasControls
                && !splaysBox.isVirtual
                && splaysBox.hovered

        sourceComponent: SplayEntryButton {
            objectName: "splayEntryButton"
        }
    }

    StationMenu {
        id: stationMenuId
        model: splaysBox.model
        dataValue: splaysBox.stationData
        listViewIndex: splaysBox.listViewIndex
        removePreview: splaysBox.removePreview
        splayClusterRow: splaysBox.rowIndex
        splayCount: splaysBox.splayCount
        splayRemoveChallenge: splaysBox.splayRemoveChallenge
    }

    //The chip and the ⋯ that acts on the cluster it counts. Most stations carry
    //no splays, and they keep a bare cell — the whole row of them stays
    //uninstantiated rather than laying out what it will never show
    QQ.Loader {
        anchors.centerIn: parent
        active: splaysBox.clusterHasControls

        sourceComponent: QQ.Row {
            spacing: Theme.flowSpacing

            QQ.Rectangle {
                objectName: "splayChip"
                anchors.verticalCenter: parent.verticalCenter

                width: chipRowId.implicitWidth + Theme.delegatePadding * 2
                height: chipRowId.implicitHeight + Theme.delegatePadding

                radius: height * 0.5
                color: Theme.splaySurface
                border.color: Theme.splayBorder
                border.width: 1

                //Takes the exclusive grab, so the cell's own handler stays out
                //of a tap meant for the chip
                QQ.TapHandler {
                    gesturePolicy: QQ.TapHandler.ReleaseWithinBounds

                    onSingleTapped: {
                        if(splaysBox.handleCellTap()) {
                            splaysBox.toggleExpanded()
                        }
                    }
                }

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

            SplayMenuButton {
                objectName: "splayClusterMenuButton"
                anchors.verticalCenter: parent.verticalCenter

                onTapped: {
                    if(splaysBox.handleCellTap()) {
                        stationMenuId.active = true
                        stationMenuId.item.popup()
                    }
                }
            }
        }
    }
}
