import QtQuick 1.1
import QtDesktop 0.1 as Desktop
import Cavewhere 1.0

Rectangle {
    id: editor

    property Scrap scrap
    property NoteTransform noteTransform: scrap.noteTransformation
    property NoteNorthInteraction northInteraction
    property NoteScaleInteraction scaleInteraction
    property InteractionManager interactionManager

    visible: noteTransform !== null
    color: style.floatingWidgetColor
    radius: style.floatingWidgetRadius
    height: column1.height + column1.y * 2.0
    width: column1.width + column1.x * 2.0

    Style {
        id: style
    }

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

    Column {
        id: column1
        x: 3
        y: 3
        spacing: 3

        Desktop.CheckBox {
            id: autoTransformCheckBox
            text: "Auto Calculate"
            checked: scrap.calculateNoteTransform
            onCheckedChanged: scrap.calculateNoteTransform = checked
        }

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
            text: "You can set the <b>scale</b> of the scrap.  Cavewhere only use <b>scale</b> to
                    help you automatically label stations."
        }
    }
}
