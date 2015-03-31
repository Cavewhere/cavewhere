/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0
import Cavewhere 1.0
import QtQuick.Controls 1.2 as Controls;

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

    title: qsTr("Image Info")

    Row {
        spacing: 5

        Button {
            id: setResolution
            width: 24
            onClicked: activateDPIInteraction()
        }

        LabelWithHelp {
            id: labelId
            text: qsTr("Image Resolution")
            anchors.verticalCenter: parent.verticalCenter
        }

        UnitValueInput {
            unitValue: resolution
            anchors.verticalCenter: parent.verticalCenter
        }

        ContextMenuButton {
            anchors.verticalCenter: parent.verticalCenter

            Controls.Menu {
                Controls.MenuItem {
                    text: {
                        var tripName = ""
                        if(note !== null && note.parentTrip() !== null) {
                            tripName = note.parentTrip().name
                        }
                        return qsTr("<b>Propagate resolution</b> for each note in ") + tripName
                    }
                    onTriggered: note.propagateResolutionNotesInTrip();
                }

                Controls.MenuItem {
                    text: qsTr("<b>Reset</b> to original")
                    onTriggered: note.resetImageResolution()
                }
            }

        }
    }
}
