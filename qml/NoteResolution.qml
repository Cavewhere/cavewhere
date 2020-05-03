/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick 1.0 as QQ // to target S60 5th Edition or Maemo 5
import QtQuick 2.0 as QQ
import QtQuick.Layouts 1.1
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

    title: "Image Info"

    ColumnLayout {
        id: layoutId
        RowLayout {
            Button {
                id: setResolution
                iconSource: "qrc:/icons/measurement.png"
                onClicked: activateDPIInteraction()
            }

            LabelWithHelp {
                id: labelId
                text: "Image Resolution"
                helpArea: resolutionHelpAreaId
            }

            UnitValueInput {
                unitValue: resolution
            }

            ContextMenuButton {

                Controls.Menu {
                    Controls.MenuItem {
                        text: {
                            var tripName = ""
                            if(note !== null && note.parentTrip() !== null) {
                                tripName = note.parentTrip().name
                            }
                            return "Propagate resolution for each note in " + tripName
                        }
                        onTriggered: note.propagateResolutionNotesInTrip();
                    }

                    Controls.MenuItem {
                        text: "Reset to original"
                        onTriggered: note.resetImageResolution()
                    }
                }

            }
        }

        HelpArea {
             id: resolutionHelpAreaId
             width: layoutId.width
             text: "The number of pixels per unit of length in the image. This property allows
                    cavewhere to propertly scale the image."
        }
    }
}
