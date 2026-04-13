import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
import cavewherelib

RowLayout {
    id: sectionHeader

    property alias text: label.text
    property bool showAddButton: false

    signal addClicked()

    spacing: Theme.delegatePadding

    SectionLabel {
        id: label
    }

    QC.RoundButton {
        visible: sectionHeader.showAddButton
        icon.source: "qrc:twbs-icons/icons/plus.svg"
        icon.width: Theme.iconSizeSmall
        icon.height: Theme.iconSizeSmall
        onClicked: sectionHeader.addClicked()
    }
}
