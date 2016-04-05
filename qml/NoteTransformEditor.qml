/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0
import QtQuick.Controls 1.0 as Controls
import QtQuick.Layouts 1.0
import Cavewhere 1.0
import "Theme.js" as Theme

Item {
    id: editor

    property Scrap scrap
    property NoteTransform noteTransform
    property NoteNorthInteraction northInteraction
    property NoteScaleInteraction scaleInteraction
    property InteractionManager interactionManager
    property int scrapType: scrap ? scrap.type : -1

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

    FloatingGroupBox {
        title: "Scrap Info"
        borderWidth: 0

        ColumnLayout {
            id: columnLayoutId

            RowLayout {
                LabelWithHelp {
                    text: "Type"
                    helpArea: scrapTypeHelpId
                }

                Controls.ComboBox {
                    id: typeComboBox
                    implicitWidth: 175
                    model: scrap ? scrap.types : null
                    onCurrentIndexChanged: if(scrap) { scrap.type = currentIndex; }

                    Binding {
                        target: typeComboBox
                        property: "currentIndex"
                        value: scrapType
                    }
                }
            }

            HelpArea {
                id: scrapTypeHelpId
                text: "The scrap type designates how Cavewhere will interperate the scrap. A running
                        profile will warp the scrap vertically along the centerline. Plan will warp
                        the scrap in plan view"
                width: columnLayoutId.width
            }

            CheckableGroupBox {
                id: checkableBoxId
                backgroundColor: Theme.floatingWidgetColor
                text: "Auto Calculate"


                ColumnLayout {
                    id: column1

                    NoteUpInput {
                        id: upInputId
                        scrapType: editor.scrapType
                        noteTransform: editor.noteTransform
                        onNorthUpInteractionActivated: interactionManager.active(northInteraction)
                        northUpHelp: northUpHelpArea
                        enable: !checkableBoxId.checked
                    }

                    HelpArea {
                        id: northUpHelpArea
                        width: scaleInputId.width
                        text: {
                            switch(scrapType) {
                            case Scrap.Plan:
                                return "You can set the direction of <b>north</b> relative to page for a scrap.
                                Setting this incorrectly may cause warping issues."
                            case Scrap.RunningProfile:
                                return "You can set the direction of <b>up</b> (the direction oppsite of gravity) relative to page for a scrap.
                                Setting this incorrectly may cause warping issues."
                            default:
                                    return "Error..."
                            }
                        }
                    }

                    PaperScaleInput {
                        id: scaleInputId
                        scaleObject: editor.noteTransform == null ? null : editor.noteTransform.scaleObject
                        scaleHelp: scaleHelpAreaId
                        onScaleInteractionActivated: interactionManager.active(scaleInteraction)
                        autoScaling: checkableBoxId.checked
                    }

                    HelpArea {
                        id: scaleHelpAreaId
                        width: scaleInputId.width
                        text: "You can set the <b>scale</b> of the scrap."
                    }
                }
            }
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
                target: checkableBoxId
                checked: scrap.calculateNoteTransform
                onCheckedChanged: scrap.calculateNoteTransform = checked
            }

        }

    ]
}
