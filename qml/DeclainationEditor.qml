// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 1.1
import Cavewhere 1.0
import "Utils.js" as Utils

GroupBox {

    property Calibration calibration

    contentHeight: column.height
    text: "Declination"

    Column {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right

        Row {
            spacing: 3

            LabelWithHelp {
                id: declination
                text: "Declination"
                helpArea: declinationHelp
            }

            ClickTextInput {
                id: tapeCalInput
                text: Utils.fixed(calibration.declination, 2)

                onFinishedEditting: {
                    calibration.declination = newText
                }
            }
        }

        HelpArea {
            id: declinationHelp
            text: "Declination help"
            anchors.left: parent.left
            anchors.right: parent.right
        }
    }
}

