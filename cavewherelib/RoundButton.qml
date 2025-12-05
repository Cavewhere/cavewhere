import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

QC.RoundButton {
    id: roundButton

    // Default icon tint; can be overridden by callers
    icon.color: Theme.icon

    // Match common sizing
    implicitWidth: implicitHeight
    implicitHeight: 28
}
