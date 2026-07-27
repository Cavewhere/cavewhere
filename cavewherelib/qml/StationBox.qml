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

    // The page-level inline fix editor this cell's caret opens. Null leaves the
    // caret and badge off, so a host that doesn't offer fixing loses nothing.
    property FixStationPopup fixStationPopup: null

    readonly property FixStationModel fixStations: stationBox.fixStationPopup !== null
                                                   ? stationBox.fixStationPopup.fixStations
                                                   : null

    readonly property string stationNameValue: stationBox.dataValue.reading.value

    // A blank name has nothing to anchor — that's the trailing virtual station
    // row, which shouldn't offer to fix itself.
    readonly property bool canFix: stationBox.fixStations !== null
                                   && stationBox.stationNameValue.trim() !== ""

    // isFixed() is a lookup, not a bindable property, so there is nothing for a
    // binding to depend on when a fix is added, removed or renamed. Bumping this
    // from fixedStationsChanged supplies that dependency — the `>= 0` term below
    // is always true, and is load-bearing: drop it and the badge goes stale.
    // (The station name and the model are already reactive on their own.)
    property int _fixStationsRevision: 0

    readonly property bool stationIsFixed: stationBox._fixStationsRevision >= 0
                                           && stationBox.canFix
                                           && stationBox.fixStations.isFixed(stationBox.stationNameValue)

    QQ.Connections {
        target: stationBox.fixStations
        function onFixedStationsChanged() {
            stationBox._fixStationsRevision++
        }
    }

    StationMenu {
        id: removeMenuId
        model: stationBox.model
        dataValue: stationBox.dataValue
        listViewIndex: stationBox.listViewIndex
        removePreview: stationBox.removePreview
    }

    rightClickMenuLoader: removeMenuId

    function guessedStationName() {
        return model.guessStationNameAt(model.cellIndex(listViewIndex, dataValue.chunkDataRole))
    }

    function commitAutoStation() {
        var stationName = guessedStationName();
        model.setDataAt(model.cellIndex(listViewIndex, dataValue.chunkDataRole), stationName)
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
                onTriggered: stationBox.fixStations.removeFixStation(stationBox.stationNameValue)
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
