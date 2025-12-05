/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQml
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

QQ.Item {
    id: editor
    objectName: "noteTransformEditor"

    required property Scrap scrap
    property NoteTranformation noteTransform
    required property NoteNorthInteraction northInteraction
    property NoteScaleInteraction scaleInteraction
    property InteractionManager interactionManager
    property int scrapType: currentScrapType()

    //This function is useful for Binding in the combobox. The combobox
    //doesn't update correctly if bound to just scrapType
    function currentScrapType() {
        return scrap ? scrap.type : -1
    }

    visible: noteTransform !== null

    implicitHeight: floatingBox.height
    implicitWidth: floatingBox.width


    QQ.Binding {
        target: editor.northInteraction
        property: "noteTransform"
        value: editor.noteTransform
    }

    QQ.Binding {
        target: editor.northInteraction
        property: "upText"
        value: {
            if(editor.scrap) {
                switch(editor.scrap.type) {
                case Scrap.Plan:
                    return "north"
                case Scrap.RunningProfile:
                    return "up"
                case Scrap.ProjectedProfile:
                    return "up"
                }
            }
        }
    }

    QQ.Binding {
        target: editor.scaleInteraction
        property: "noteTransform"
        value: editor.noteTransform
    }

    FloatingGroupBox {
        id: floatingBox
        title: "Scrap Info"
        borderWidth: 0

        ColumnLayout {
            id: columnLayoutId

            RowLayout {
                LabelWithHelp {
                    text: "Type"
                    helpArea: scrapTypeHelpId
                }

                QC.ComboBox {
                    id: typeComboBox
                    objectName: "typeComboBox"
                    implicitWidth: 175
                    model: editor.scrap ? editor.scrap.types : null
                    onActivated: if(editor.scrap) {
                                     editor.scrap.type = currentIndex;
                                 }
                    currentIndex: editor.currentScrapType()
                }
            }

            HelpArea {
                id: scrapTypeHelpId
                text: "The scrap type designates how CaveWhere will interperate the scrap.<br>" +
"<ul>" +
"<li><b>Plan</b>: Projects the scrap on a plane aligned with the ground.</li>" +
"<li><b>Running Profile</b>: Creates a projected profile for each shot and wraps the scrap along each scrap." +
"This projection is generally the best for <b>horizontal passage</b> with profiles because it will wrap around" +
"corners.</li>" +
"<li><b>Projected Profiles<\b>: Projects the scrap on vertial plane aligned azimuth settings. This projection" +
"is generally the best for <b>deep pits</b> where the profile is drawn on a single plane.<\\li><\\ul>"

                implicitWidth: columnLayoutId.width
            }

            CheckableGroupBox {
                id: checkableBoxId
                objectName: "autoCalculate"
                backgroundColor: Theme.floatingWidgetColor
                text: "Auto Calculate"


                ColumnLayout {
                    id: column1

                    NoteUpInput {
                        id: upInputId
                        scrapType: editor.scrapType
                        noteTransform: editor.noteTransform
                        onNorthUpInteractionActivated: {

                            editor.interactionManager.active(editor.northInteraction)
                        }
                        northUpHelp: northUpHelpArea
                        enable: !checkableBoxId.checked
                    }

                    HelpArea {
                        id: northUpHelpArea
                        Layout.fillWidth: true
                        text: {
                            switch(editor.scrapType) {
                            case Scrap.Plan:
                                return "You can set the direction of <b>north</b> relative to page for a scrap." +
                                "Setting this incorrectly may cause warping issues."
                            case Scrap.RunningProfile:
                                return "You can set the direction of <b>up</b> (the direction oppsite of gravity) relative to page for a scrap. Setting this incorrectly may cause warping issues."
                            case Scrap.ProjectedProfile:
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
                        onScaleInteractionActivated: editor.interactionManager.active(editor.scaleInteraction)
                        autoScaling: checkableBoxId.checked
                    }

                    HelpArea {
                        id: scaleHelpAreaId
                        Layout.fillWidth: true
                        text: "You can set the <b>scale</b> of the scrap. Setting this incorrectly may cause warping issues."
                    }

                    RowLayout {
                        id: azimuthInput
                        visible: upInputId.scrapType == Scrap.ProjectedProfile
                        LabelWithHelp {
                            id: azimuthLabelId
                            text: "Azimuth is"
                            helpArea: azimuthHelpAreaId
                        }

                        QC.ComboBox {
                            id: directionComboBoxId
                            objectName: "directionComboBox"

                            enabled: !checkableBoxId.checked

                            function isValid() {
                                return editor.scrap
                                        && editor.scrap.viewMatrix
                                        && (editor.scrap.viewMatrix as ProjectedProfileScrapViewMatrix)
                            }

                            model: {
                                if(directionComboBoxId.isValid()) {
                                    let matrix = (editor.scrap.viewMatrix as ProjectedProfileScrapViewMatrix)
                                    return matrix.directionTypes
                                }
                                return null
                            }

                            currentIndex: {
                                if(directionComboBoxId.isValid()) {
                                    let matrix = (editor.scrap.viewMatrix as ProjectedProfileScrapViewMatrix)
                                    return matrix.direction
                                }
                                return -1;
                            }

                            onActivated: {
                                if(directionComboBoxId.isValid()) {
                                    editor.scrap.viewMatrix.direction = currentIndex
                                }
                            }
                        }

                        ClickTextInput {
                            id: azimuthTextInputId
                            text: {
                                if(directionComboBoxId.isValid()) {
                                    let matrix = (editor.scrap.viewMatrix as ProjectedProfileScrapViewMatrix)
                                    return matrix.azimuth.toFixed(1)
                                }
                                return 0.0
                            }
                            readOnly: checkableBoxId.checked
                            onFinishedEditting: (newText) => {
                                editor.scrap.viewMatrix.azimuth = newText
                            }
                        }

                        Text {
                            text: "°"
                        }
                    }

                    HelpArea {
                        id: azimuthHelpAreaId
                        Layout.fillWidth: true
                        text: "The projected profile azimuth direction. By default this uses <i>" +
"looking at </i> which is as if you were looking through the page and is normal to the scrap plane." +
"Other options are parellel with the page:" +
"<ul>" +
"    <li> <i>left → right </i> The azimuth going from the left side to the right side of the page</li>" +
"    <li> <i>left ← right </i> The azimuth going from the right side to the left side of the page</li>" +
"</ul>"
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
            when: editor.scrap !== null

            QQ.PropertyChanges {
                editor {
                    noteTransform: scrap.noteTransformation
                }
            }

            QQ.PropertyChanges {
                checkableBoxId {
                    checked: scrap.calculateNoteTransform
                    onCheckedChanged: scrap.calculateNoteTransform = checked
                }
            }

        }

    ]
}
