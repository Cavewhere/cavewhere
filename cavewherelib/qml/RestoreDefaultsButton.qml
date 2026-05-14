import QtQuick
import QtQuick.Controls as QC
import QtQuick.Layouts

RowLayout {
    id: root

    required property QtObject settings

    Layout.fillWidth: true
    Layout.alignment: Qt.AlignRight
    spacing: 8

    Item { Layout.fillWidth: true }

    QC.Button {
        objectName: "restoreDefaultsButton"
        text: "Restore Defaults"
        enabled: root.settings ? !root.settings.isAtDefaults : false
        onClicked: {
            if (root.settings) {
                root.settings.resetToDefaults()
            }
        }
    }
}
