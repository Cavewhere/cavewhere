import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// A flat tinted section: the style's group box title, on a borderless fill.
QC.GroupBox {
    id: sectionGroupBoxId

    horizontalPadding: Theme.statsPadding
    verticalPadding: Theme.statsPadding

    background: QQ.Rectangle {
        color: Theme.sectionFill
        radius: Theme.panelRadius
        border.width: 0
    }
}
