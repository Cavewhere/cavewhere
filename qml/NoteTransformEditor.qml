import QtQuick 2.0
import QtDesktop 1.0 as Desktop
import Cavewhere 1.0

Item {
    id: editor

    property Scrap scrap
    property NoteTransform noteTransform
    property NoteNorthInteraction northInteraction
    property NoteScaleInteraction scaleInteraction
    property InteractionManager interactionManager

    visible: noteTransform !== null

    height: childrenRect.height
    width: childrenRect.width

    Binding {
        target: northInteraction
        property: "noteTransform"
        value: noteTransform
    }

    Binding {
        target: scaleInteraction
        property: "noteTransform"
        value: noteTransform
    }

    Style {
        id: style
    }

    Text {
        id: scrapInfoText
        z: 1
        anchors.right: checkBoxGroup.right
        anchors.bottom: checkBoxGroup.top
        anchors.rightMargin: 5

        font.bold:  true
        text: "Scrap Info"
    }

    Rectangle {
        id: backgroundRect

        color: style.floatingWidgetColor
        radius: style.floatingWidgetRadius
        height: checkBoxGroup.height + scrapInfoText.height + autoTransformCheckBox.height / 2.0
        width: checkBoxGroup.width + checkBoxGroup.x * 2.0
        //        y: groupAreaRect.height / 2.0
    }

    Rectangle {
        id: checkBoxGroup
        border.width: 1
        radius: style.floatingWidgetRadius
        color: "#00000000"

        anchors.top: autoTransformCheckBox.verticalCenter
        anchors.left: column1.left
        anchors.right: column1.right
        anchors.bottom: column1.bottom

        anchors.leftMargin: -3
        anchors.rightMargin: -3
        anchors.bottomMargin: -3
    }

    Rectangle {
        color: backgroundRect.color
        anchors.fill: autoTransformCheckBox
    }

    Desktop.CheckBox {
        id: autoTransformCheckBox
        text: "Auto Calculate"
        anchors.left: checkBoxGroup.left
        anchors.leftMargin: 6

        y: scrapInfoText.height / 2.0
    }

    Column {
        id: column1
        spacing: 3

        x: 6

        anchors.top: checkBoxGroup.top
        anchors.topMargin: 8

        NoteNorthUpInput {
            id: northUpInputId
            noteTransform: editor.noteTransform
            onNorthUpInteractionActivated: interactionManager.active(northInteraction)
            northUpHelp: northUpHelpArea
            enable: !autoTransformCheckBox.checked
        }

        HelpArea {
            id: northUpHelpArea
            width: scaleInputId.width
            text: "You can set the direction of <b>north</b> relative to page for a scrap.
            Cavewhere only uses <b>north</b> to help you automatically label stations."
        }

        NoteScaleInput {
            id: scaleInputId
            noteTransform: editor.noteTransform
            scaleHelp: scaleHelpAreaId
            onScaleInteractionActivated: interactionManager.active(scaleInteraction)
            autoScaling: autoTransformCheckBox.checked
        }

        HelpArea {
            id: scaleHelpAreaId
            width: scaleInputId.width
            text: "You can set the <b>scale</b> of the scrap."
        }
    }

    states: [
        State {
            when: scrap !== null

            PropertyChanges {
                target: editor
                noteTransform: scrap.noteTransformation
            }

            PropertyChanges {
                target: autoTransformCheckBox
                checked: scrap.calculateNoteTransform
                onCheckedChanged: scrap.calculateNoteTransform = checked
            }

        }

    ]
}
