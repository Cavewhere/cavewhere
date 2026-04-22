import QtQuick
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

ColumnLayout {
    id: root

    property SketchSettings sketchSettings: RootData.settings.sketchSettings

    // Preview converts the user's physical millimetres into a
    // world-meter cap at a canonical scale (1:100, zoom 1.0) so
    // the gallery is comparable across users. The caption under the
    // gallery notes this so nobody mistakes the preview's trim
    // boundary for behaviour at their own scale.
    readonly property real _previewScaleRatio: 0.01
    readonly property real _previewMaxHookLength: sketchSettings.maxHookLengthMm
                                                  / 1000.0
                                                  / _previewScaleRatio

    function parseDouble(text) {
        const value = Number(text);
        if (isNaN(value)) {
            return null;
        }
        return value;
    }

    QC.GroupBox {
        title: "Pen Smoothing"
        Layout.fillWidth: true

        ColumnLayout {
            spacing: 8
            anchors {
                left: parent.left
                right: parent.right
            }

            RowLayout {
                spacing: 6
                Layout.fillWidth: true

                InformationButton { showItemOnClick: filterHooksHelp }

                QC.CheckBox {
                    id: filterHooksCheckBoxId
                    objectName: "filterHooksCheckBox"
                    text: "Filter pen hooks"
                    checked: root.sketchSettings.filterPenHooks
                    onToggled: root.sketchSettings.filterPenHooks = checked
                }
            }

            HelpArea {
                id: filterHooksHelp
                Layout.fillWidth: true
                text: "Trims the short reversal spur that appears at the start (and sometimes end) of Apple Pencil strokes when the tip slips during press-down. Off by default; enable when drawing on a stylus-capable device."
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                rowSpacing: 6
                columnSpacing: 6

                Repeater {
                    // Keep the four labels in sync with the sample order
                    // baked into cwHookFilterPreview.cpp's samples() table.
                    model: [
                        { label: "V-hook (head)" },
                        { label: "L-hook (touchdown)" },
                        { label: "V-hook (tail)" },
                        { label: "Clean stroke" },
                    ]

                    ColumnLayout {
                        id: cellId
                        required property int index
                        required property var modelData

                        Layout.fillWidth: true
                        spacing: 2

                        HookFilterPreview {
                            objectName: "hookFilterPreview" + cellId.index
                            Layout.fillWidth: true
                            Layout.preferredHeight: 90
                            sampleIndex:     cellId.index
                            filtered:        root.sketchSettings.filterPenHooks
                            maxHookLength:   root._previewMaxHookLength
                            maxHookFraction: root.sketchSettings.maxHookFraction
                        }

                        QC.Label {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: Theme.fontSizeCaption
                            opacity: 0.75
                            text: cellId.modelData.label
                        }
                    }
                }
            }

            QC.Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeCaption
                opacity: 0.65
                text: "Preview shown at 1:100 scale. Your own scale determines what the cap looks like in practice."
                wrapMode: Text.WordWrap
            }

            RowLayout {
                spacing: 6
                Layout.fillWidth: true
                enabled: root.sketchSettings.filterPenHooks

                InformationButton { showItemOnClick: maxHookLengthHelp }

                QC.Label { text: "Max hook length (screen mm)" }

                ClickTextInput {
                    Layout.fillWidth: true
                    validator: DistanceValidator {}
                    text: root.sketchSettings.maxHookLengthMm.toFixed(2)
                    onFinishedEditting: (newText) => {
                                            const mm = root.parseDouble(newText);
                                            if (mm === null || mm < 0.0) return;
                                            root.sketchSettings.maxHookLengthMm = mm;
                                        }
                }
            }

            HelpArea {
                id: maxHookLengthHelp
                Layout.fillWidth: true
                text: "Physical distance on the input surface — 2 mm covers a typical Apple Pencil press-down slip. This is converted to world meters at draw time using the sketch's current map scale and zoom, so the cap behaves the same whether you're zoomed in or out."
            }

            RowLayout {
                spacing: 6
                Layout.fillWidth: true
                enabled: root.sketchSettings.filterPenHooks

                InformationButton { showItemOnClick: maxHookFractionHelp }

                QC.Label { text: "Max hook fraction" }

                ClickTextInput {
                    Layout.fillWidth: true
                    validator: DistanceValidator {}
                    text: root.sketchSettings.maxHookFraction.toFixed(2)
                    onFinishedEditting: (newText) => {
                                            const frac = root.parseDouble(newText);
                                            if (frac === null || frac < 0.0 || frac > 1.0) return;
                                            root.sketchSettings.maxHookFraction = frac;
                                        }
                }
            }

            HelpArea {
                id: maxHookFractionHelp
                Layout.fillWidth: true
                text: "Secondary cap expressed as a fraction of the full stroke. The hook arm must stay below this portion of the stroke's length to be trimmable. Protects short deliberate strokes whose apparent 'reversal' is a large chunk of the intended mark."
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignRight
                spacing: 8

                QC.Button {
                    text: "Restore Defaults"
                    onClicked: {
                        root.sketchSettings.filterPenHooks = false;
                        root.sketchSettings.maxHookLengthMm = 2.0;
                        root.sketchSettings.maxHookFraction = 0.15;
                    }
                }
            }
        }
    }
}
