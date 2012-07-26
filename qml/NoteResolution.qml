// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0
import Cavewhere 1.0

FloatingGroupBox {
    id: floatingGroup

    property Note note;
    property ImageResolution resolution: {
        if(note !== null) {
            return note.imageResolution;
        }
        return null;
    }

    signal activateDPIInteraction

    titleText: "Image Info"

    Row {
        spacing: 5
        x: floatingGroup.margin
        y: floatingGroup.margin

        Button {
            id: setResolution
            width: 24
            onClicked: activateDPIInteraction()
        }

        LabelWithHelp {
            id: labelId
            text: "Image Resolution"
            anchors.verticalCenter: parent.verticalCenter
        }


        UnitValueInput {
            unitValue: resolution
            anchors.verticalCenter: parent.verticalCenter
        }

        Button {
            id: moreButton
            iconSource: "qrc:/icons/moreArrowDown.png"
            iconSize: Qt.size(8, 8)
            height: 12
            anchors.verticalCenter: parent.verticalCenter

            onClicked: {
                resolutionPopupMenu.popupOnTopOf(moreButton, 0, moreButton.height)
            }

            ContextMenu {
                id: resolutionPopupMenu

                MenuItem {
                    text: {
                        var tripName = ""
                        if(note !== null && note.parentTrip() !== null) {
                            tripName = note.parentTrip().name
                        }
                        return "<b>Propagate resolution</b> for each note in " + tripName
                    }
                    onTriggered: note.propagateResolutionNotesInTrip();
                }

                MenuItem {
                    text: "<b>Reset</b> to original"
                    onTriggered: note.resetImageResolution()
                }

            }

        }


    }

}
