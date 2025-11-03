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
    required property NoteLiDAR note
    // Derived convenience handle
    readonly property NoteLiDARTransformation noteTransform: note ? note.noteTransformation : null
    // Optional, for interactive “set north with tool”
    property NoteLiDARNorthInteraction northInteraction
    // Optional, if you still want to expose scale interaction
    property NoteLiDARScaleInteraction scaleInteraction
    property NoteLiDARUpInteraction upInteraction
    property InteractionManager interactionManager

    visible: note !== null
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

            // --- North (auto/manual control) -----------------------------------
            CheckableGroupBox {
                id: northAutoGroup
                text: "Auto Calculate"
                backgroundColor: Theme.floatingWidgetColor
                checked: editor.note ? editor.note.autoCalculateNorth : true
                contentsVisible: true
                enabled: editor.note !== null
                Layout.fillWidth: true

                onCheckedChanged: {
                    if (editor.note && editor.note.autoCalculateNorth !== checkbox.checked) {
                        editor.note.autoCalculateNorth = checkbox.checked
                    }
                }

                ColumnLayout {
                    spacing: 8

                    RowLayout {
                        spacing: 8

                        // Optional tool hook (mouse interaction to set north)
                        NoteToolIconButton {
                            id: setNorthToolBtn
                            enabled: !northAutoGroup.checked && editor.interactionManager && editor.northInteraction
                            iconSource: "qrc:/icons/svg/north.svg"
                            toolTipText: enabled ? "Set north with tool" : "To enable tool, uncheck Auto Calculate"
                            onClicked: editor.interactionManager.active(editor.northInteraction)
                        }

                        LabelWithHelp {
                            id: northLabel
                            text: "North"
                            helpArea: northHelp
                        }

                        ClickTextInput {
                            id: northField
                            objectName: "northField"
                            readOnly: northAutoGroup.checked
                            text: editor.noteTransform ? editor.noteTransform.northUp.toFixed(1) : "0.0"
                            onFinishedEditting: (newText) => {
                                if (editor.noteTransform && !northAutoGroup.checked) {
                                    editor.noteTransform.northUp = Number(newText)
                                }
                            }
                            Layout.preferredWidth: 90
                        }

                    }

                    HelpArea {
                        id: northHelp
                        Layout.fillWidth: true
                        text: "Rotate the LiDAR note so that map-up corresponds to <b>true north</b> " +
                              "(or your chosen reference). This writes <code>northUp</code> on the base transform."
                    }
                }
            }

            // --- Up direction (axis + sign) ------------------------------------
            RowLayout {
                spacing: 8

                NoteToolIconButton {
                    id: setUpToolBtn
                    enabled: editor.interactionManager && editor.upInteraction &&
                             editor.noteTransform && editor.noteTransform.upMode === NoteLiDARTransformation.UpMode.Custom
                    iconSource: "qrc:/icons/svg/up.svg"
                    toolTipText: enabled ? "Set up direction with tool" : "To enable tool, set up direction to Custom"
                    onClicked: editor.interactionManager.active(editor.upInteraction)
                }

                LabelWithHelp {
                    id: upModeLabel
                    text: "Up"
                    helpArea: upModeHelp
                }

                Controls.ComboBox {
                    id: upModeCombo
                    objectName: "upModeCombo"
                    implicitWidth: 200
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
            ColumnLayout {
                spacing: 6

                PaperScaleInput {
                    id: scaleInput
                    scaleObject: editor.noteTransform ? editor.noteTransform.scaleObject : null
                    scaleHelp: scaleHelp
                    autoScaling: false
                    usingInteraction: !!(editor.interactionManager && editor.scaleInteraction)
                    onPaperLabel: "In Model"
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

                // ErrorHelpArea {
                //     Layout.fillWidth: true
                //     visible: {
                //         if (scaleInput.scaleObject) {
                //             return Math.abs(scaleInput.scaleObject.scale - 1.0) > 1e-6
                //         }
                //         return false
                //     }
                //     text: "Warning: scale is not 1:1. Verify your LiDAR export units."
                // }
            }
        }
    }
}
