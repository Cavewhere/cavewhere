import QtQuick 1.0
import QtDesktop 0.1 as Desktop
import Cavewhere 1.0

Item {
    id: item1

    property NoteTransform noteTransform
    property NoteNorthInteraction northInteraction
    property InteractionManager interactionManager

    visible: noteTransform !== null

    anchors.fill: parent

    Binding {
        target: northInteraction
        property: "noteTransform"
        value: noteTransform
    }

    Column {
        id: column1
        anchors.fill: parent


        Row {

            anchors.left: parent.left
            anchors.right: parent.right

            Button {
                id: setNorthButton

                onClicked: {
                    interactionManager.active(northInteraction)
                }
            }

            Text {
                id: text1
                text: qsTr("North")
            }

            DoubleClickTextInput {
                id: northUpId
                text: noteTransform.northUp
                onFinishedEditting: noteTransform.northUp = newText
            }

            Text {
                id: text3
                x: 133
                y: 6
                width: 6
                height: 15
                text: qsTr("°")
                font.pixelSize: 12
            }
        }

        Row {

            anchors.left: parent.left
            anchors.right: parent.right

            Button {
                id: setLength
            }

            Text {
                id: label
                text: qsTr("Scale")
            }

            DoubleClickTextInput {
                id: numberOfPixelsId
                text: noteTransform.scaleNumerator.value
                onFinishedEditting: noteTransform.scaleNumerator.value = newText
            }



            Text {
                text: ":"
            }

            DoubleClickTextInput {
                id: lengthId
                text: noteTransform.scaleDenominator.value
                onFinishedEditting: noteTransform.scaleDenominator.value = newText
            }


            Desktop.ComboBox {
                id: denominatorUnits

                Desktop.MenuItem {
                    text: "mm"
                }

                Desktop.MenuItem {
                    text: "cm"
                }

                Desktop.MenuItem {
                    text: "m"
                }

                Desktop.MenuItem {
                    text: "ft"
                }
            }

            Text {
                text: ":"
            }

            Desktop.ComboBox {
                id: numeratorUnits

                Desktop.MenuItem {
                    text: "mm"
                }

                Desktop.MenuItem {
                    text: "cm"
                }

                Desktop.MenuItem {
                    text: "m"
                }

                Desktop.MenuItem {
                    text: "ft"
                }
            }
        }

        Row {
            Text {
                text: "PPI"
            }

            DoubleClickTextInput {
                id: ppiId
                text: noteTransform.pixelsPerInch
                onFinishedEditting: noteTransform.pixelsPerInch = newText
            }

            Text {
                text: "Pixels / in"
                font.italic: true
            }
        }
    }
}
