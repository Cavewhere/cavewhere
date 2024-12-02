/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick as QQ // to target S60 5th Edition or Maemo 5
import QtQuick.Layouts
import cavewherelib
import QtQuick.Controls as Controls;

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
            Controls.Button {
                id: setResolution
                icon.source: "qrc:/icons/svg/measurement.svg"
                onClicked: floatingGroup.activateDPIInteraction()
            }

            LabelWithHelp {
                id: labelId
                text: "Image Resolution"
                helpArea: resolutionHelpAreaId
            }

            UnitValueInput {
                unitValue: floatingGroup.resolution
            }

            ContextMenuButton {

                Controls.Menu {
                    Controls.MenuItem {
                        text: {
                            var tripName = ""
                            if(floatingGroup.note !== null && floatingGroup.note.parentTrip() !== null) {
                                tripName = floatingGroup.note.parentTrip().name
                            }
                            return "Propagate resolution for each note in " + tripName
                        }
                        onTriggered: floatingGroup.note.propagateResolutionNotesInTrip();
                    }

                    Controls.MenuItem {
                        text: "Reset to original"
                        onTriggered: floatingGroup.note.resetImageResolution()
                    }
                }

            }
        }

        HelpArea {
             id: resolutionHelpAreaId
             implicitWidth: layoutId.width
             text: "The number of pixels per unit of length in the image. This property allows" +
                    "cavewhere to propertly scale the image."
        }
    }
}
