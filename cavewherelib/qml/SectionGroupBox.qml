import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// A flat tinted section: the style's group box title, on a borderless fill.
QC.GroupBox {
    id: sectionGroupBoxId

    horizontalPadding: Theme.statsPadding
    verticalPadding: Theme.statsPadding

    // The pane's implicit content item is named "GroupBox", which would put an
    // extra link in the objectName chain the tests find page content by.
    QQ.Component.onCompleted: contentItem.objectName = ""

    background: QQ.Rectangle {
        color: Theme.sectionFill
        radius: Theme.panelRadius
        border.width: 0
    }
}
