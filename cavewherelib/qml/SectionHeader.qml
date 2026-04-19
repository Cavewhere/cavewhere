import QtQml
import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
import cavewherelib

RowLayout {
    id: sectionHeader

    property alias text: label.text
    property bool showAddButton: false
    property Component addControl: null

    signal addClicked()

    spacing: Theme.delegatePadding

    SectionLabel {
        id: label
    }

    QQ.Loader {
        active: sectionHeader.addControl !== null
        visible: active
        sourceComponent: sectionHeader.addControl
    }

    QC.RoundButton {
        visible: sectionHeader.showAddButton && sectionHeader.addControl === null
        icon.source: "qrc:twbs-icons/icons/plus.svg"
        icon.width: Theme.iconSizeSmall
        icon.height: Theme.iconSizeSmall
        onClicked: sectionHeader.addClicked()
    }
}
