import QtQuick as QQ
import QtQuick.Effects
import cavewherelib

// Shared background for Menu, the ComboBox list, Popup, Dialog, and ToolTip: a
// bordered surface with a soft drop shadow.
//
// The popup that uses this sets all four insets to -Theme.popupShadowMargin, so
// this item is larger than the popup by that margin on every side. The surface
// is drawn inset by the same margin, which leaves room for the shadow inside
// the item. A popup shown in its own window (Popup.Window) sizes that window
// from the background, so the shadow stays visible there too.
QQ.Item {
    id: panel

    property QQ.color color: Theme.popupSurface
    property QQ.color borderColor: Theme.popupBorder
    property real radius: Theme.controlRadius
    readonly property int margin: Theme.popupShadowMargin

    RectangularShadow {
        anchors.fill: surface
        radius: panel.radius
        blur: Theme.popupShadowBlur
        offset.y: Theme.popupShadowOffset
        color: Theme.popupShadow
    }

    QQ.Rectangle {
        id: surface
        anchors.fill: parent
        anchors.margins: panel.margin
        radius: panel.radius
        color: panel.color
        border.width: 1
        border.color: panel.borderColor
    }
}
