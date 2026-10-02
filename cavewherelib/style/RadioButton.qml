import QtQuick as QQ
import QtQuick.Templates as T
import QtQuick.Controls.impl
import cavewherelib

T.RadioButton {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: Theme.controlVerticalPadding
    spacing: Theme.controlSpacing + 1

    indicator: QQ.Rectangle {
        implicitWidth: Theme.indicatorSize
        implicitHeight: Theme.indicatorSize
        x: control.text ? (control.mirrored ? control.width - width - control.rightPadding : control.leftPadding)
                        : control.leftPadding + (control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2

        radius: width / 2
        opacity: control.enabled ? 1 : Theme.disabledOpacity
        color: control.checked ? Theme.checkFill
             : control.down ? Theme.hoverOverlay : "transparent"
        border.width: control.checked ? 0 : control.hovered ? 2 : 1.5
        border.color: Theme.controlBorder

        QQ.Rectangle {
            anchors.centerIn: parent
            width: Theme.radioDotSize
            height: Theme.radioDotSize
            radius: width / 2
            color: Theme.checkMark
            visible: control.checked
        }

        StyleFocusRing {
            visible: control.visualFocus
        }
    }

    contentItem: CheckLabel {
        leftPadding: control.indicator && !control.mirrored ? control.indicator.width + control.spacing : 0
        rightPadding: control.indicator && control.mirrored ? control.indicator.width + control.spacing : 0
        text: control.text
        font: control.font
        color: control.palette.windowText
        opacity: control.enabled ? 1 : Theme.disabledOpacity
    }
}
