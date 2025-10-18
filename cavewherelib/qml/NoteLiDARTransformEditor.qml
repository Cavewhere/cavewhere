/**************************************************************************
**
**    NoteLiDARTransformEditor.qml
**    Editor for cwNoteLiDARTransformation (northUp + UpMode + upSign + scale)
**
**************************************************************************/

import QtQuick as QQ
import QtQml
import QtQuick.Controls as Controls
import QtQuick.Layouts
import cavewherelib
import "qrc:/qt/qml/cavewherelib/qml/Theme.js" as Theme

QQ.Item {
    id: editor
    objectName: "noteLiDARTransformEditor"

    // ---- Required/expected API ------------------------------------------------
    required property NoteLiDARTransformation noteTransform
    // Optional, for interactive “set north with tool”
    property NoteNorthInteraction northInteraction
    // Optional, if you still want to expose scale interaction
    property NoteScaleInteraction scaleInteraction
    property InteractionManager interactionManager

    visible: noteTransform !== null
    implicitHeight: floatingBox.implicitHeight
    implicitWidth: floatingBox.implicitWidth

    // ---- Bind interactions to the active transformation ----------------------
    QQ.Binding {
        target: editor.northInteraction
        property: "noteTransform"
        value: editor.noteTransform
        when: editor.northInteraction !== null
    }

    QQ.Binding {
        target: editor.scaleInteraction
        property: "noteTransform"
        value: editor.noteTransform
        when: editor.scaleInteraction !== null
    }

    // ---- Helpers --------------------------------------------------------------
    readonly property var upModeEntries: [
        { label: "Custom",        value: NoteLiDARTransformation.UpMode.Custom },
        { label: "X is up",       value: NoteLiDARTransformation.UpMode.XisUp },
        { label: "Y is up (PolyCam)", value: NoteLiDARTransformation.UpMode.YisUp },
        { label: "Z is up",       value: NoteLiDARTransformation.UpMode.ZisUp }
    ]

    function upModeToIndex(mode) {
        for (let i = 0; i < upModeEntries.length; ++i) {
            if (upModeEntries[i].value === mode) {
                return i
            }
        }
        return 0
    }

    function indexToUpMode(idx) {
        return upModeEntries[Math.max(0, Math.min(idx, upModeEntries.length - 1))].value
    }

    FloatingGroupBox {
        id: floatingBox
        title: "LiDAR Note Transform"
        borderWidth: 0

        ColumnLayout {
            id: column
            Layout.margins: 0
            spacing: 10

            // --- North (degrees) ------------------------------------------------
            RowLayout {
                spacing: 8

                LabelWithHelp {
                    id: northLabel
                    text: "North (°)"
                    helpArea: northHelp
                }

                ClickTextInput {
                    id: northField
                    objectName: "northField"
                    text: editor.noteTransform ? editor.noteTransform.northUp.toFixed(1) : "0.0"
                    onFinishedEditting: (newText) => {
                        if (editor.noteTransform) {
                            editor.noteTransform.northUp = Number(newText)
                        }
                    }
                    Layout.preferredWidth: 90
                }

                Text { text: "°" }

                // Optional tool hook (mouse interaction to set north)
                Controls.Button {
                    id: setNorthToolBtn
                    visible: editor.interactionManager && editor.northInteraction
                    text: "Set with tool"
                    onClicked: editor.interactionManager.active(editor.northInteraction)
                }
            }

            HelpArea {
                id: northHelp
                Layout.fillWidth: true
                text: "Rotate the LiDAR note so that map-up corresponds to <b>true north</b> " +
                      "(or your chosen reference). This writes <code>northUp</code> on the base transform."
            }

            // --- Up direction (mode + sign) ------------------------------------
            RowLayout {
                spacing: 8

                LabelWithHelp {
                    id: upModeLabel
                    text: "Up direction"
                    helpArea: upModeHelp
                }

                Controls.ComboBox {
                    id: upSignCombo
                    objectName: "upSignCombo"
                    // implicitWidth: 10
                    model: ["+", "- (flip)"]
                    currentIndex: editor.noteTransform && editor.noteTransform.upSign < 0 ? 1 : 0
                    onActivated: {
                        if (editor.noteTransform) {
                            editor.noteTransform.upSign = (currentIndex === 1) ? -1.0 : 1.0
                        }
                    }
                }

                Controls.ComboBox {
                    id: upModeCombo
                    objectName: "upModeCombo"
                    implicitWidth: 300
                    model: upModeEntries.map(e => e.label)
                    currentIndex: editor.noteTransform ? editor.upModeToIndex(editor.noteTransform.upMode) : 0
                    onActivated: {
                        if (editor.noteTransform) {
                            editor.noteTransform.upMode = editor.indexToUpMode(currentIndex)
                        }
                    }
                }


            }

            HelpArea {
                id: upModeHelp
                Layout.fillWidth: true
                text: "Choose which source axis represents <b>Up</b> before north rotation and scaling. " +
                      "<ul>" +
                      "<li><b>X is up / Y is up / Z is up</b>: Treat the chosen axis as 'up'.</li>" +
                      "<li><b>Custom</b>: Use the <code>upRotation</code> quaternion (advanced rigs).</li>" +
                      "<li><b>Flip</b> with <i>up sign</i> = -1 to invert the chosen up axis.</li>" +
                      "</ul>"
            }

            // --- Custom up rotation (quaternion) --------------------------------
            // Shown only when UpMode.Custom is selected. Basic, numeric input for power users.
            RowLayout {
                id: quatRow
                visible: editor.noteTransform && editor.noteTransform.upMode === NoteLiDARTransformation.UpMode.Custom
                spacing: 8

                LabelWithHelp {
                    text: "Custom up rotation (xyzw)"
                    helpArea: customQuatHelp
                }

                ClickTextInput {
                    id: quatX; Layout.preferredWidth: 70
                    text: editor.noteTransform ? editor.noteTransform.upRotation.x.toFixed(4) : "0.0000"
                    onFinishedEditting: (t) => { if (editor.noteTransform) {
                            let q = editor.noteTransform.upRotation; q.x = Number(t); editor.noteTransform.upRotation = q; } }
                }
                ClickTextInput {
                    id: quatY; Layout.preferredWidth: 70
                    text: editor.noteTransform ? editor.noteTransform.upRotation.y.toFixed(4) : "0.0000"
                    onFinishedEditting: (t) => { if (editor.noteTransform) {
                            let q = editor.noteTransform.upRotation; q.y = Number(t); editor.noteTransform.upRotation = q; } }
                }
                ClickTextInput {
                    id: quatZ; Layout.preferredWidth: 70
                    text: editor.noteTransform ? editor.noteTransform.upRotation.z.toFixed(4) : "0.0000"
                    onFinishedEditting: (t) => { if (editor.noteTransform) {
                            let q = editor.noteTransform.upRotation; q.z = Number(t); editor.noteTransform.upRotation = q; } }
                }
                ClickTextInput {
                    id: quatW; Layout.preferredWidth: 70
                    text: editor.noteTransform ? editor.noteTransform.upRotation.scalar.toFixed(4) : "1.0000"
                    onFinishedEditting: (t) => { if (editor.noteTransform) {
                            // QQuaternion uses (scalar=w) + vector (x,y,z)
                            let q = editor.noteTransform.upRotation; q.scalar = Number(t); editor.noteTransform.upRotation = q; } }
                }
            }

            HelpArea {
                id: customQuatHelp
                visible: quatRow.visible
                Layout.fillWidth: true
                text: "Quaternion applied first to align the LiDAR’s native up to your desired 'page up'. " +
                      "Use only if <b>UpMode = Custom</b>."
            }

            // --- Scale (should generally be 1:1 for LiDAR) ----------------------
            CheckableGroupBox {
                id: scaleBox
                objectName: "scaleGroup"
                backgroundColor: Theme.floatingWidgetColor
                text: "Scale"
                checked: false   // keep manual by default for LiDAR
                // When checked, PaperScaleInput becomes 'auto scaling' (disabled edits)

                ColumnLayout {
                    spacing: 6

                    PaperScaleInput {
                        id: scaleInput
                        scaleObject: editor.noteTransform ? editor.noteTransform.scaleObject : null
                        scaleHelp: scaleHelp
                        autoScaling: scaleBox.checked
                        onScaleInteractionActivated: if (editor.interactionManager && editor.scaleInteraction) {
                            editor.interactionManager.active(editor.scaleInteraction)
                        }
                    }

                    HelpArea {
                        id: scaleHelp
                        Layout.fillWidth: true
                        text: "LiDAR notes should typically be <b>1:1</b>. If your source is already metrically " +
                              "correct, leave scale at 1. Adjust only if your capture app exported in pixels or " +
                              "non-metric units."
                    }

                    ErrorHelpArea {
                        Layout.fillWidth: true
                        visible: {
                            if (scaleInput.scaleObject) {
                                return Math.abs(scaleInput.scaleObject.scale - 1.0) > 1e-6
                            }
                            return false
                        }
                        text: "Warning: scale is not 1:1. Verify your LiDAR export units."
                    }
                }
            }
        }
    }
}
