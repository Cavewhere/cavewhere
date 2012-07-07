// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0
import Cavewhere 1.0

FloatingGroupBox {
    id: floatingGroup

    property ImageResolution resolution

    titleText: "Image Info"

    Row {
        spacing: 5
        x: floatingGroup.margin
        y: floatingGroup.margin

        Button {
            id: setResolution
            width: 24
            //      onClicked: northUpInteractionActivated()
        }

        LabelWithHelp {
            id: labelId
//            helpArea: northUpHelp
            text: "Image Resolution"
            anchors.verticalCenter: parent.verticalCenter
        }


        UnitValueInput {
            unitValue: resolution
        }
    }

}
