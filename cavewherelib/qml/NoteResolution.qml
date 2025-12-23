/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// import QtQuick as QQ // to target S60 5th Edition or Maemo 5
import QtQuick.Layouts
import QtQuick as QQ;
import cavewherelib
import QtQuick.Controls as QC

FloatingGroupBox {
    id: floatingGroup

    property Note note;
    property ImageResolution resolution: {
        if(note !== null) {
            return note.imageResolution;
        }
        return null;
    }
    readonly property bool resolutionValid: resolution !== null &&
                                            Number.isFinite(resolution.value) &&
                                            resolution.value > 0.0

    signal activateDPIInteraction

    title: "Image Info"

    ColumnLayout {
        id: layoutId
        RowLayout {
            RoundButton {
                id: setResolution
                objectName: "setResolution"
                icon.source: "qrc:/icons/svg/measurement.svg"
                onClicked: floatingGroup.activateDPIInteraction()
                radius: 2
            }

            LabelWithHelp {
                id: labelId
                text: "Image Resolution"
                helpArea: resolutionHelpAreaId
            }

            UnitValueInput {
                objectName: "noteDPI"
                unitValue: floatingGroup.resolution
                validator: DPIValidator { }
            }

            ContextMenuButton {

                menu:QC.Menu {
                    QC.MenuItem {
                        text: {
                            var tripName = ""
                            if(floatingGroup.note !== null && floatingGroup.note.parentTrip() !== null) {
                                tripName = floatingGroup.note.parentTrip().name
                            }
                            return "Propagate resolution for each note in " + tripName
                        }
                        onTriggered: floatingGroup.note.propagateResolutionNotesInTrip();
                    }

                    QC.MenuItem {
                        text: "Reset to original"
                        onTriggered: floatingGroup.note.resetImageResolution()
                    }
                }

            }
        }

        ErrorHelpArea {
             id: resolutionErrorAreaId
             Layout.fillWidth: true
             visible: !floatingGroup.resolutionValid
             text: "Invalid DPI. Value must be finite and greater than 0."
        }

        HelpArea {
             id: resolutionHelpAreaId
             implicitWidth: layoutId.width
             text: "The number of pixels per unit of length in the image. This property allows" +
                    "cavewhere to propertly scale the image."
        }
    }
}
