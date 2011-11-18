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
    height: childrenRect.height + 5
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
        spacing: 3

        NoteNorthUpInput {
            id: northUpInputId
            anchors.left: parent.left
            anchors.right: parent.right
            noteTransform: editor.noteTransform
            onNorthUpInteractionActivated: interactionManager.active(northInteraction)
        }

        NoteScaleInput {
            id: scaleInputId
            anchors.left: parent.left
            anchors.right: parent.right
            noteTransform: editor.noteTransform
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
