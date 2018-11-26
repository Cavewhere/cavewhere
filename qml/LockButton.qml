import QtQuick 2.0
import QtQuick.Controls 2.0

ToolButton {
    icon.name: "lock"
    icon.source: down ? "qrc:/icons/lock.png" : "qrc:/icons/unlock.png"
}
