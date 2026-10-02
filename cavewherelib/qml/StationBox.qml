/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

DataBox {
    id: stationBox

    // property var window: QQ.Window.window

    // The page-level inline fix editor this cell's caret opens, and the one
    // handle the cell has on the cave's fixes. Null leaves the caret off, so a
    // host that offers no way to edit a fix simply doesn't offer the actions.
    property FixStationPopup fixStationPopup: null

    // Whether one of the cave's fixes anchors this station. Read from the survey
    // model's StationFixedRole rather than looked up here — see
    // DrySurveyComponent. Independent of the caret above: a badge is worth
    // showing even where fixing isn't offered.
    property bool stationIsFixed: false

    readonly property string stationNameValue: stationBox.dataValue.reading.value

    // A station is where a splay move lands, so while one is armed this cell
    // stops being a place to type and becomes a place to click.
    readonly property bool splayMoveActive: stationBox.model !== null
                                            && stationBox.model.splayMoveActive
    readonly property bool splayMoveTarget: stationBox.splayMoveActive
                                            && stationBox.model.isSplayMoveTarget(stationBox.rowIndex)
    readonly property bool splayMoveSource: stationBox.splayMoveActive
                                            && stationBox.model.isSplayMoveSource(stationBox.rowIndex)

    // The station the splays are leaving steps back so the stations that can
    // take them read as the ones to click
    readonly property real splayMoveSourceOpacity: 0.4
    readonly property int splayMoveOutlineWidth: 2

    // The outline draws inside the cell's own border, so the neighboring
    // distance cell has no stroke of ours to clip
    readonly property int splayMoveOutlineInset: 1

    // A blank name has nothing to anchor — that's the trailing virtual station
    // row, which shouldn't offer to fix itself.
    readonly property bool canFix: stationBox.fixStationPopup !== null
                                   && stationBox.stationNameValue.trim() !== ""

    acceptsSplayMove: true

    opacity: stationBox.splayMoveSource ? stationBox.splayMoveSourceOpacity : 1.0

    StationMenu {
        id: removeMenuId
        model: stationBox.model
        dataValue: stationBox.dataValue
        listViewIndex: stationBox.listViewIndex
        removePreview: stationBox.removePreview
    }

    rightClickMenuLoader: removeMenuId

    function guessedStationName() {
        return model.guessStationNameAt(model.cellIndex(listViewIndex, stationBox.cellRole))
    }

    function commitAutoStation() {
        var stationName = guessedStationName();
        model.setDataAt(model.cellIndex(listViewIndex, stationBox.cellRole), stationName)
    }

    onFocusChanged: {
        // console.log("Focus changed!" + focus + " " + this + " Window activeFocus:" + stationBox.window.activeFocusItem) ;

        if(focus) {
            //Make sure it's visible to the user
            if(dataValue.chunk) {
                var lastStationIndex = dataValue.chunk.stationCount - 1

                // Try to guess for new stations on the trailing station row.
                // With virtual rows enabled, that row may be one past the real last station.
                let isTrailingStationRow =
                        (lastStationIndex === dataValue.indexInChunk)
                        || (dataValue.chunk.stationCount === dataValue.indexInChunk)
                if(isTrailingStationRow) {

                    // Make sure the data is empty.
                    let currentValue = dataValue.reading.value
                    if(currentValue === "" || currentValue === null || currentValue === undefined) {
                        var guessedstationName = guessedStationName();
                        if(guessedstationName !== "") {
                            stationName.text = guessedstationName
                            state = "AutoNameState"
                        }
                    }
                }
            }
        }
    }

    // Every station row would otherwise carry an outline that is only ever seen
    // while a move is armed, which is almost never
    QQ.Loader {
        anchors.fill: parent
        anchors.margins: stationBox.splayMoveOutlineInset
        active: stationBox.splayMoveTarget
        z: 1

        sourceComponent: QQ.Rectangle {
            objectName: "splayMoveTargetOutline"
            color: Theme.transparent
            border.color: Theme.splayBorder
            border.width: stationBox.splayMoveOutlineWidth
        }
    }

    QQ.Rectangle {
        id: guessAreaBackground
        //        radius: 5
        color: Theme.floatingWidgetColor
        anchors.centerIn: parent
        visible: false
        border.color: Theme.border

        QQ.MouseArea {
            anchors.fill: parent
            onClicked: {
                stationBox.commitAutoStation()
            }
        }
    }

    QQ.Column {
        id: guessArea
        anchors.centerIn: parent

        visible: false

        QC.Label {
            anchors.horizontalCenter: parent.horizontalCenter
            color: Theme.textSubtle
            text: "Press Tab"
            font.pixelSize: Theme.fontSizeBody
            font.bold: true
            horizontalAlignment: Qt.AlignHCenter
        }

        QC.Label {
            id: stationName
            anchors.horizontalCenter: parent.horizontalCenter
            //            font.pixelSize: Theme.fontSizeCaption
        }
    }

    DataBoxBadge {
        objectName: "fixedStationBadge"
        visible: stationBox.stationIsFixed
        text: "Fixed"
    }

    // The same caret the exclude-distance menu uses one column over, so the two
    // cell-level actions share an affordance rather than resembling one.
    DataBoxCaret {
        active: stationBox.focus && stationBox.canFix
        buttonObjectName: "fixStationMenuButton"

        menu: QC.Menu {
            objectName: "fixStationMenu"

            QC.MenuItem {
                objectName: "markStationFixedMenuItem"
                // One action, two labels: opening the editor creates the fix
                // when there isn't one, which is what "mark as fixed" means.
                text: stationBox.stationIsFixed
                      ? qsTr("Edit Fixed Coordinates...")
                      : qsTr("Mark Station as Fixed")
                onTriggered: stationBox.fixStationPopup.openFor(stationBox.stationNameValue, stationBox)
            }

            QC.MenuItem {
                objectName: "removeStationFixMenuItem"
                text: qsTr("Remove Fix")
                enabled: stationBox.stationIsFixed
                onTriggered: stationBox.fixStationPopup.removeFixFor(stationBox.stationNameValue)
            }
        }
    }

    states: [
        QQ.State {
            name: "AutoNameState"

            QQ.PropertyChanges {
                stationBox {

                    onTabPressed: {
                        commitAutoStation()
                    }

                    onDeletePressed: {
                        state = ""
                        deletePressedHandler()
                    }

                    onFocusChanged: {
                        if(!focus) {
                            state = ""
                        }
                    }
                }
            }

            QQ.PropertyChanges {
                guessArea {
                    visible: true
                }
            }

            QQ.PropertyChanges {
                guessAreaBackground {
                    visible: true
                    width: guessArea.width + 6
                    height: guessArea.height
                }
            }
        }
    ]
}
