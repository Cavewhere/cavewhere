import QtQuick 1.1
import Cavewhere 1.0

Interaction {
    id: interaction

    property BasePanZoomInteraction basePanZoomInteraction
    property ImageItem imageItem
    property NoteTransform noteTransform
    property alias transformUpdater: scaleLengthItem.transformUpdater

    focus: visible
    Keys.onEscapePressed: {
        interaction.state = ""
        interaction.deactivate();
    }

    QtObject {
        id: privateData
        property variant firstLocation;
        property variant secondLocation;
    }

    MouseArea {
        id: mouseAreaId
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onPressed: {
            if(pressedButtons == Qt.RightButton) {
                basePanZoomInteraction.panFirstPoint(Qt.point(mouse.x, mouse.y))
            }
        }

        onMousePositionChanged: {
            if(pressedButtons == Qt.RightButton) {
                basePanZoomInteraction.panMove(Qt.point(mouse.x, mouse.y))
            }
        }

        onClicked: {
            if(mouse.button === Qt.LeftButton) {
                privateData.firstLocation = imageItem.mapQtViewportToNote(Qt.point(mouse.x, mouse.y));
                scaleLengthItem.p1 = privateData.firstLocation
                scaleLengthItem.p2 = scaleLengthItem.p1
                interaction.state = "WaitForSecondClick"
            }
        }
    }

    ScaleLengthItem {
        id: scaleLengthItem
        anchors.fill: parent
    }

    Rectangle {
        id: lengthRect

        visible: false

        radius: style.floatingWidgetRadius
        color: style.floatingWidgetColor

        width: row.width + row.x * 2
        height: row.height + row.y * 2

        Style {
            id: style
        }

        Length {
            id: length
        }

        Row {
            id: row

            x: 3
            y: 3

            spacing: 3

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "<b>In cave length</b>"
            }

            Row {
                anchors.verticalCenter: parent.verticalCenter

                ClickTextInput {
                    id: caveUnits
                    text: length.value
                    onFinishedEditting: length.value = newText
                }

                UnitInput {
                    unitModel: length.unitNames
                    unit: Units.Meters //For meters
                    onNewUnit: length.unit = unit
                }
            }

            Button {
                anchors.verticalCenter: parent.verticalCenter

                text: "Done"
                onClicked: {
                    var imageSize = imageItem.imageProperties.size
                    var dotPerMeter = imageItem.imageProperties.dotsPerMeter
                    var scale = noteTransform.calculateScale(privateData.firstLocation,
                                                             privateData.secondLocation,
                                                             length,
                                                             imageSize,
                                                             dotPerMeter);

                    noteTransform.scale = scale
//                    noteTransform.scaleNumerator.unit = Units.Unitless

//                    noteTransform.scaleDenominator.value = 1 / scale
//                    noteTransform.scaleDenominator.unit = Units.Unitless

                    interaction.state = ""
                    interaction.deactivate();
                }
            }
        }
    }


    HelpBox {
        id: helpBoxId
        text: "<b>Click</b> the length's first point"
        visible: true
    }


    states: [
        State {
            name: "WaitForSecondClick"

            PropertyChanges {
                target: mouseAreaId

                hoverEnabled: true

                onMousePositionChanged: {
                    if(pressedButtons == Qt.RightButton) {
                        basePanZoomInteraction.panMove(Qt.point(mouse.x, mouse.y))
                    } else {
                        scaleLengthItem.p2 = imageItem.mapQtViewportToNote(Qt.point(mouse.x, mouse.y));
                    }
                }

                onClicked: {
                    if(mouse.button === Qt.LeftButton) {
                        lengthRect.x = mouse.x - lengthRect.width * 0.5
                        lengthRect.y = mouse.y + 10

                        privateData.secondLocation = imageItem.mapQtViewportToNote(Qt.point(mouse.x, mouse.y));
                        interaction.state = "WaitForDone"
                    }
                }
            }

            PropertyChanges {
                target: helpBoxId
                text: "<b> Click </b> the length's second point"
            }
        },

        State {
            name: "WaitForDone"

            PropertyChanges {
                target: lengthRect
                visible: true
            }

            PropertyChanges {
                target: helpBoxId
                visible: false
            }
        }
    ]
}
