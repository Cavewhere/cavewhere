/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ
import QtQml 2.2
import QtQuick.Controls 1.0 as Controls
import QtQuick.Layouts 1.0
import Cavewhere 1.0
import "Theme.js" as Theme

QQ.Item {
    id: editor

    property Scrap scrap
    property NoteTransform noteTransform
    property NoteNorthInteraction northInteraction
    property NoteScaleInteraction scaleInteraction
    property InteractionManager interactionManager
    property int scrapType: currentScrapType()

    //This function is useful for Binding in the combobox. The combobox
    //doesn't update correctly if bound to just scrapType
    function currentScrapType() {
        return scrap ? scrap.viewMatrix.type : -1
    }

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
                    onCurrentIndexChanged: if(scrap) {
                                               scrap.viewMatrix.type = currentIndex;
                                           }

                    Binding {
                        target: typeComboBox
                        property: "currentIndex"
                        value: currentScrapType()
                    }
                }
            }

            HelpArea {
                id: scrapTypeHelpId
                text: "The scrap type designates how CaveWhere will interperate the scrap. A running
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
                        Layout.fillWidth: true
                        text: {
                            switch(scrapType) {
                            case ScrapViewMatrix.Plan:
                                return "You can set the direction of <b>north</b> relative to page for a scrap.
                                Setting this incorrectly may cause warping issues."
                            case ScrapViewMatrix.RunningProfile:
                                return "You can set the direction of <b>up</b> (the direction oppsite of gravity) relative to page for a scrap. Setting this incorrectly may cause warping issues."
                            case ScrapViewMatrix.ProjectProfile:
                                return "You can set the direction of <b>up</b> (the direction oppsite of gravity) relative to page for a scrap. Setting this incorrectly may cause warping issues."
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
                        Layout.fillWidth: true
                        text: "You can set the <b>scale</b> of the scrap. Setting this incorrectly may cause warping issues."
                    }

                    RowLayout {
                        id: azimuthInput
                        visible: upInputId.scrapType == ScrapViewMatrix.ProjectedProfile
                        LabelWithHelp {
                            id: azimuthLabelId
                            text: "Azimuth"
                            helpArea: azimuthHelpAreaId
                        }

                        ClickTextInput {
                            id: azimuthTextInputId
                            text: scrap.viewMatrix.azimuth
                            onFinishedEditting: {
                                scrap.viewMatrix.azimuth = newText
                            }
                        }
                    }

                    HelpArea {
                        id: azimuthHelpAreaId
                        Layout.fillWidth: true
                        text: "Azimuth help"
                    }

                    ErrorHelpArea {
                        Layout.fillWidth: true
                        visible: {
                            if(scaleInputId.scaleObject) {
                                return scaleInputId.scaleObject.scale >= 1.0
                            }
                            return false;
                        }

                        text: {"A scale of 1:1 or smaller is bad! You might need to add more station to create a shot or enter scale manually."}
                    }
                }
            }
        }
    }

    states: [
        QQ.State {
            when: scrap !== null

            QQ.PropertyChanges {
                target: editor
                noteTransform: scrap.noteTransformation
            }

            QQ.PropertyChanges {
                target: checkableBoxId
                checked: scrap.calculateNoteTransform
                onCheckedChanged: scrap.calculateNoteTransform = checked
            }

        }

    ]
}
