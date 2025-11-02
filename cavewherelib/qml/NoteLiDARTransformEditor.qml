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
    property NoteLiDARNorthInteraction northInteraction
    // Optional, if you still want to expose scale interaction
    property NoteLiDARScaleInteraction scaleInteraction
    property NoteLiDARUpInteraction upInteraction
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

    QQ.Binding {
        target: editor.upInteraction
        property: "noteTransform"
        value: editor.noteTransform
        when: editor.upInteraction !== null
    }

    // ---- Helpers --------------------------------------------------------------
    readonly property var upModeEntries: [
        { label: "+Y is up (PolyCam)",   mode: NoteLiDARTransformation.UpMode.YisUp, sign: 1.0 },
        { label: "-Y is up",             mode: NoteLiDARTransformation.UpMode.YisUp, sign: -1.0 },
        { label: "+Z is up",             mode: NoteLiDARTransformation.UpMode.ZisUp, sign: 1.0 },
        { label: "-Z is up",             mode: NoteLiDARTransformation.UpMode.ZisUp, sign: -1.0 },
        { label: "+X is up",             mode: NoteLiDARTransformation.UpMode.XisUp, sign: 1.0 },
        { label: "-X is up",             mode: NoteLiDARTransformation.UpMode.XisUp, sign: -1.0 },
        { label: "Custom",               mode: NoteLiDARTransformation.UpMode.Custom }
    ]

    function upModeToIndex(mode, sign) {
        const clampedSign = (typeof sign === "number" && sign < 0) ? -1.0 : 1.0
        for (let i = 0; i < upModeEntries.length; ++i) {
            const entry = upModeEntries[i]
            if (entry.mode === mode) {
                if (!("sign" in entry) || entry.sign === clampedSign) {
                    return i
                }
            }
        }
        return 0
    }

    function indexToUpConfig(idx) {
        return upModeEntries[Math.max(0, Math.min(idx, upModeEntries.length - 1))]
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
                NoteToolIconButton {
                    id: setNorthToolBtn
                    visible: editor.interactionManager && editor.northInteraction
                    iconSource: "qrc:/icons/svg/north.svg"
                    toolTipText: "Set north with tool"
                    onClicked: editor.interactionManager.active(editor.northInteraction)
                }
            }

            HelpArea {
                id: northHelp
                Layout.fillWidth: true
                text: "Rotate the LiDAR note so that map-up corresponds to <b>true north</b> " +
                      "(or your chosen reference). This writes <code>northUp</code> on the base transform."
            }

            // --- Up direction (axis + sign) ------------------------------------
            RowLayout {
                spacing: 8

                LabelWithHelp {
                    id: upModeLabel
                    text: "Up direction"
                    helpArea: upModeHelp
                }

                NoteToolIconButton {
                    id: setUpToolBtn
                    visible: editor.interactionManager && editor.upInteraction
                    iconSource: "qrc:/icons/svg/up.svg"
                    toolTipText: "Set up direction with tool"
                    onClicked: editor.interactionManager.active(editor.upInteraction)
                }

                Controls.ComboBox {
                    id: upModeCombo
                    objectName: "upModeCombo"
                    implicitWidth: 300
                    model: upModeEntries.map(e => e.label)
                    currentIndex: editor.noteTransform ? editor.upModeToIndex(editor.noteTransform.upMode, editor.noteTransform.upSign) : 0
                    onActivated: {
                        if (editor.noteTransform) {
                            const entry = editor.indexToUpConfig(currentIndex)
                            editor.noteTransform.upMode = entry.mode
                            if ("sign" in entry) {
                                editor.noteTransform.upSign = entry.sign
                            } else {
                                editor.noteTransform.upSign = 1.0
                            }
                        }
                    }
                }


            }

            HelpArea {
                id: upModeHelp
                Layout.fillWidth: true
                text: "Choose which source axis represents <b>Up</b> before north rotation and scaling. " +
                      "<ul>" +
                      "<li><b>±X / ±Y / ±Z is up</b>: Pick the source axis and sign that should point up.</li>" +
                      "<li><b>Custom</b>: Use the arrow tool button to orient the LiDAR and update the <code>upRotation</code> quaternion.</li>" +
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
                    readOnly: true
                    text: editor.noteTransform ? editor.noteTransform.upRotation.x.toFixed(4) : "0.0000"
                }
                ClickTextInput {
                    id: quatY; Layout.preferredWidth: 70
                    readOnly: true
                    text: editor.noteTransform ? editor.noteTransform.upRotation.y.toFixed(4) : "0.0000"
                }
                ClickTextInput {
                    id: quatZ; Layout.preferredWidth: 70
                    readOnly: true
                    text: editor.noteTransform ? editor.noteTransform.upRotation.z.toFixed(4) : "0.0000"
                }
                ClickTextInput {
                    id: quatW; Layout.preferredWidth: 70
                    readOnly: true
                    text: editor.noteTransform ? editor.noteTransform.upRotation.scalar.toFixed(4) : "1.0000"
                }
            }

            HelpArea {
                id: customQuatHelp
                visible: quatRow.visible
                Layout.fillWidth: true
                text: "Quaternion applied first to align the LiDAR’s native up to your desired 'page up'. " +
                      "Use the up-direction tool button while in Custom mode to adjust it."
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
