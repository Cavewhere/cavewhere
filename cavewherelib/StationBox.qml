/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib
import "qrc:/cavewherelib/cavewherelib/Theme.js" as Theme

DataBox {
    id: stationBox

    function commitAutoStation() {
        var stationName = surveyChunk.guessLastStationName();
        surveyChunk.setData(dataRole, rowIndex, stationName);
        surveyChunkView.tab(rowIndex, dataRole)
    }

    onFocusChanged: {
//        if(focus) {
//            //Make sure it's visible to the user
//            surveyChunkView.ensureDataBoxVisible(rowIndex, dataRole)

//            var lastStationIndex = surveyChunk.stationCount() - 1

//            //Try to guess for new stations what the next station is
//            //Make sure the station is the last station in the chunk
//            if(lastStationIndex  === rowIndex) {

//                //Make sure the data is empty
//                if(dataValue == "") {
//                    var guessedstationName = surveyChunk.guessLastStationName();
//                    if(guessedstationName !== "") {
//                        stationName.text = guessedstationName
//                        state = "AutoNameState"
//                    }
//                }
//            }
//        }
    }

    QQ.Rectangle {
        id: guessAreaBackground
        //        radius: 5
        color: Theme.floatingWidgetColor
        anchors.centerIn: parent
        visible: false
        border.color: "#888888"

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
            color: "#444444"
            text: "Press Tab"
            font.pixelSize: 14
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            id: stationName
            anchors.horizontalCenter: parent.horizontalCenter
            color: "#333333"
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
