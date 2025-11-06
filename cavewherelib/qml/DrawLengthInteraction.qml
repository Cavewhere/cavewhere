/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib
import "qrc:/qt/qml/cavewherelib/qml/Theme.js" as Theme

PanZoomInteraction {
    id: interaction

    property ImageItem imageItem
    property alias doneTextLabel: lengthText.text
    property alias doneEnabled: doneId.enabled
    property alias lengthObject: length
    property alias defaultLengthUnit: length.unit
    property alias zoom: scaleLengthItem.zoom
    property alias lengthValidator: lengthUnitValueId.validator

    //More or less, private data
    property point firstMouseLocation;
    property point secondMouseLocation;

    signal doneButtonPressed;

    function done() {
        scaleLengthItem.visible = false
        interaction.state = ""
        interaction.deactivate();
    }

    focus: visible
    dragAcceptedButtons: Qt.RightButton
    QQ.Keys.onEscapePressed: done()

    QQ.TapHandler {
        id: pressId
        target: interaction.target
        parent: interaction.target
        enabled: interaction.enabled
        onTapped: (eventPoint, button) => {
                      interaction.firstMouseLocation = eventPoint.position
                      scaleLengthItem.p1 = eventPoint.position
                      scaleLengthItem.p2 = scaleLengthItem.p1
                      interaction.state = "WaitForSecondClick"
                      scaleLengthItem.visible = true;
                  }
    }

    QQ.HoverHandler {
        id: hoverId
        enabled: false
        target: interaction.target
        parent: interaction.target
        onPointChanged: {
            scaleLengthItem.p2 = hoverId.point.position
        }
    }

    ScaleLengthItem {
        id: scaleLengthItem
        objectName: "scaleLengthItem"
        anchors.fill: parent
        visible: interaction.visible
        parent: interaction.target
    }

    InteractionFloatingPanel {
        id: lengthRect

        visible: false

        Length {
            id: length
            unit: Units.Meters
        }

        content: QQ.Row {
            id: row
            spacing: 3
            Text {
                id: lengthText
                anchors.verticalCenter: parent.verticalCenter
            }

            UnitValueInput {
                id: lengthUnitValueId
                objectName: "lengthUnitValue"
                unitValue: length
                defaultUnit: Units.LengthUnitless
                anchors.verticalCenter: parent.verticalCenter
            }

            QC.Button {
                id: doneId
                objectName: "doneButton"
                anchors.verticalCenter: parent.verticalCenter
                text: "Done"
                onClicked: interaction.doneButtonPressed()
            }
        }
    }


    HelpBox {
        id: helpBoxId
        text: "<b>Click</b> the length's first point"
        visible: true
    }


    states: [
        QQ.State {
            name: "WaitForSecondClick"

            QQ.PropertyChanges {
                pressId {
                    onTapped: (eventPoint, button) => {
                        scaleLengthItem.p2 = eventPoint.position

                        let pos = interaction.target.mapToItem(lengthRect.parent, eventPoint.position)

                        lengthRect.x = pos.x - lengthRect.width * 0.5
                        lengthRect.y = pos.y + 10

                        interaction.secondMouseLocation = eventPoint.position
                        interaction.state = "WaitForDone"
                    }
                }

                hoverId {
                    enabled: true
                }
            }

            QQ.PropertyChanges {
                helpBoxId {
                    text: "<b> Click </b> the length's second point"
                }
            }
        },

        QQ.State {
            name: "WaitForDone"

            QQ.PropertyChanges {
                lengthRect {
                    visible: true
                }
            }

            QQ.PropertyChanges {
                helpBoxId {
                    visible: false
                }
            }
        }
    ]
}
