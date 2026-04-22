import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

ColumnLayout {
    id: itemId
    property SketchSettings sketchSettings: RootData.settings.sketchSettings

    QC.GroupBox {
        title: "Thumbnails"
        Layout.fillWidth: true

        ColumnLayout {
            RowLayout {
                InformationButton {
                    showItemOnClick: autoUpdateHelpId
                }

                QC.CheckBox {
                    objectName: "autoIconUpdatesCheckBox"
                    text: "Auto update"
                    checked: itemId.sketchSettings.autoIconUpdates
                    onToggled: {
                        itemId.sketchSettings.autoIconUpdates = checked;
                    }
                }
            }

            HelpArea {
                id: autoUpdateHelpId
                Layout.fillWidth: true
                text: "Regenerate sketch thumbnails in the background after you stop drawing. Off by default on mobile to avoid stroke lag and battery drain; thumbnails still refresh when you leave or switch sketches."
            }

            RowLayout {
                enabled: itemId.sketchSettings.autoIconUpdates

                InformationButton {
                    showItemOnClick: idleIntervalHelpId
                }

                QC.Label {
                    text: "Idle delay (s)"
                }

                DoubleSpinBox {
                    objectName: "idleIntervalSpinBox"
                    decimals: 1
                    realFrom: 0.0
                    realTo: 60.0
                    realStepSize: 0.5
                    realValue: itemId.sketchSettings.idleIntervalMs / 1000.0
                    onValueModified: {
                        itemId.sketchSettings.idleIntervalMs = Math.round(realValue * 1000);
                    }
                }
            }

            HelpArea {
                id: idleIntervalHelpId
                Layout.fillWidth: true
                text: "How long to wait after your last edit before rendering a thumbnail. Lower is fresher; higher coalesces edit bursts. Defaults: 3 s desktop, 5 s mobile."
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignRight

                QC.Button {
                    text: "Restore Defaults"
                    onClicked: itemId.sketchSettings.resetToDefaults()
                }
            }
        }
    }
}
