import QtQuick 2.0
import QtQuick.Controls as QC

QC.ToolButton {
    icon.name: "lock"
    icon.source: (down || checked) ? "qrc:/twbs-icons/icons/lock-fill.svg" : "qrc:/twbs-icons/icons/unlock.svg"
    icon.color: Theme.icon
    icon.width: 16
    icon.height: 16
}
