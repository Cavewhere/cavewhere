import QtQuick 1.0
import QtDesktop 0.1 as Desktop
import Cavewhere 1.0

Rectangle {
    id: editor

    property NoteTransform noteTransform
    property NoteNorthInteraction northInteraction
    property InteractionManager interactionManager

    visible: noteTransform !== null
    color: style.floatingWidgetColor
    radius: style.floatingWidgetRadius
    height: childrenRect.height + 10
    width: 300

    Style {
        id: style
    }

    Binding {
        target: northInteraction
        property: "noteTransform"
        value: noteTransform
    }

    Column {
        id: column1
        anchors.left: parent.left
        anchors.right: parent.right
        //anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 5
        anchors.rightMargin: 5
        y: 5
        spacing: 5

        NorthUpInput {
            id: northUpInputId
            anchors.left: parent.left
            anchors.right: parent.right
            noteTransform: editor.noteTransform
            onNorthUpInteractionActivated: interactionManager.active(northInteraction)
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

//        Row {
//            Text {
//                text: "PPI"
//            }

//            DoubleClickTextInput {
//                id: ppiId
//                text: noteTransform.pixelsPerInch
//                onFinishedEditting: noteTransform.pixelsPerInch = newText
//            }

//            Text {
//                text: "Pixels / in"
//                font.italic: true
//            }
//        }
    }
}
