/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// The Splays column's cell. It holds no reading, so it's a DataBox in look and
// focus behavior only — no editor, no error model, no remove preview. Focusing
// it and pressing Enter or Space opens the station's splay cluster, which is
// also the way a station with no splays at all gets its first one.
QQ.Item {
    id: splaysBox
    objectName: "splaysBox." + splaysBox.listViewIndex

    required property SurveyEditorModel model
    required property cwSurveyEditorRowIndex rowIndex

    //Index in the ListView
    required property int listViewIndex

    required property int splayCount
    required property bool splaysExpanded

    property TripCalibration calibration: null
    readonly property bool frontSights: calibration !== null && calibration.frontSights
    readonly property bool backSights: calibration !== null && calibration.backSights

    readonly property int dataRole: SurveyChunk.StationSplaysRole

    function shouldHaveFocus(): bool {
        if(listViewIndex < 0) {
            return false
        }
        return model.focusedRow === listViewIndex && model.focusedRole === splaysBox.dataRole
    }

    function syncFocusState() {
        const selected = shouldHaveFocus()
        if(focus === selected) {
            return
        }
        if(selected) {
            forceActiveFocus()
        }
        focus = selected
    }

    function takeFocus() {
        model.setFocusedCell(model.cellIndex(splaysBox.listViewIndex, splaysBox.dataRole))
    }

    function moveFocus(navKey: int) {
        const currentCell = model.cellIndex(splaysBox.listViewIndex, splaysBox.dataRole)
        model.setFocusedCell(model.nextCell(currentCell,
                                            navKey,
                                            splaysBox.frontSights,
                                            splaysBox.backSights))
    }

    function toggleExpanded() {
        model.toggleSplaysExpanded(splaysBox.rowIndex)
    }

    QQ.Component.onCompleted: syncFocusState()

    onListViewIndexChanged: syncFocusState()

    onFocusChanged: {
        if(focus) {
            takeFocus()
        }
    }

    QQ.Keys.onPressed: (event) => {
                           switch(event.key) {
                           case Qt.Key_Tab:
                               splaysBox.moveFocus(SurveyEditorModel.Tab)
                               break
                           case 1 + Qt.Key_Tab:
                               //Shift tab -- 1 + Qt.Key_Tab is a hack but it works
                               splaysBox.moveFocus(SurveyEditorModel.BackTab)
                               break
                           case Qt.Key_Left:
                               splaysBox.moveFocus(SurveyEditorModel.Left)
                               break
                           case Qt.Key_Right:
                               splaysBox.moveFocus(SurveyEditorModel.Right)
                               break
                           case Qt.Key_Up:
                               splaysBox.moveFocus(SurveyEditorModel.Up)
                               break
                           case Qt.Key_Down:
                               splaysBox.moveFocus(SurveyEditorModel.Down)
                               break
                           case Qt.Key_Enter:
                           case Qt.Key_Return:
                           case Qt.Key_Space:
                               splaysBox.toggleExpanded()
                               break
                           default:
                               return
                           }
                           event.accepted = true
                       }

    QQ.Connections {
        target: splaysBox.model

        function onFocusedRowChanged() {
            splaysBox.syncFocusState()
        }

        function onFocusedRoleChanged() {
            splaysBox.syncFocusState()
        }
    }

    QQ.Rectangle {
        id: backgroundId
        anchors.fill: parent

        gradient: QQ.Gradient {
            QQ.GradientStop {
                position: splaysBox.rowIndex.indexInChunk % 2 === 0 ? 1.0 : 0.0
                color: Theme.surfaceRaised
            }
            QQ.GradientStop {
                position: splaysBox.rowIndex.indexInChunk % 2 === 0 ? 0.4 : 0.6
                color: Theme.surface
            }
        }
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
        visible: splaysBox.shouldHaveFocus()
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

    QQ.TapHandler {
        onSingleTapped: {
            splaysBox.takeFocus()
            splaysBox.toggleExpanded()
        }
    }
}
