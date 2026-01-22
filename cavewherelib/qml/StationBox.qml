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

    StationMenu {
        id: removeMenuId
        dataValue: stationBox.dataValue
        removePreview: stationBox.removePreview
    }

    rightClickMenuLoader: removeMenuId

    function commitAutoStation() {
        var stationName = dataValue.rowIndex.chunk.guessLastStationName();
        dataValue.rowIndex.chunk.setData(dataValue.chunkDataRole, dataValue.rowIndex.indexInChunk, stationName);
    }

    onFocusChanged: {
        // console.log("Focus changed!" + focus + " " + this + " Window activeFocus:" + stationBox.window.activeFocusItem) ;

        if(focus) {
            //Make sure it's visible to the user
            if(dataValue.chunk) {
                var lastStationIndex = dataValue.chunk.stationCount - 1

                //Try to guess for new stations what the next station is
                //Make sure the station is the last station in the chunk
                if(lastStationIndex  === dataValue.indexInChunk) {

                    //Make sure the data is empty
                    if(dataValue.reading.value === "") {
                        var guessedstationName = dataValue.chunk.guessLastStationName();
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

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            color: Theme.textSubtle
            text: "Press Tab"
            font.pixelSize: 14
            font.bold: true
            horizontalAlignment: Qt.AlignHCenter
        }

        Text {
            id: stationName
            anchors.horizontalCenter: parent.horizontalCenter
            color: Theme.text
            //            font.pixelSize: 11
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
