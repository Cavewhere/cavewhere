import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

ColumnLayout {
    spacing: 20

    Units { id: unitsId }

    QC.GroupBox {
        title: "Units"
        Layout.fillWidth: true

        ColumnLayout {
            width: parent.width
            spacing: 8

            BodyText {
                Layout.fillWidth: true
                text: "The unit system new projects start in. Each project stores its "
                      + "own unit system, so changing this only affects projects created afterwards."
            }

            RowLayout {
                spacing: 12

                QC.Label {
                    text: "Default:"
                }

                QC.ComboBox {
                    objectName: "unitSystemComboBox"
                    model: [unitsId.unitSystemName(Units.Metric),
                            unitsId.unitSystemName(Units.Imperial)]
                    currentIndex: RootData.settings.unitSettings.unitSystem
                    onActivated: (index) => RootData.settings.unitSettings.unitSystem = index
                }
            }
        }
    }

    QQ.Item { Layout.fillHeight: true }
}
