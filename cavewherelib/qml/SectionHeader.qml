import QtQml
import QtQuick as QQ
import QtQuick.Layouts
import cavewherelib

RowLayout {
    id: sectionHeader

    property alias text: label.text
    property Component addControl: null

    spacing: Theme.delegatePadding

    SectionLabel {
        id: label
    }

    QQ.Loader {
        active: sectionHeader.addControl !== null
        visible: active
        sourceComponent: sectionHeader.addControl
    }
}
