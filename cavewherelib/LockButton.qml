import QtQuick 2.0
import QtQuick.Controls as QC

QC.ToolButton {
    icon.name: "lock"
    icon.source: down ? "qrc:/icons/lock.png" : "qrc:/icons/unlock.png"
    icon.color: Theme.icon
}
