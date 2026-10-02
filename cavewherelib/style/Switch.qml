import QtQuick as QQ
import QtQuick.Templates as T
import QtQuick.Controls.impl
import cavewherelib

T.Switch {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: Theme.controlVerticalPadding
    spacing: Theme.controlSpacing + 1

    indicator: QQ.Rectangle {
        implicitWidth: Theme.switchWidth
        implicitHeight: Theme.switchHeight
        x: control.text ? (control.mirrored ? control.width - width - control.rightPadding : control.leftPadding)
                        : control.leftPadding + (control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2

        radius: height / 2
        opacity: control.enabled ? 1 : Theme.disabledOpacity
        color: control.checked ? Theme.accent : Theme.track

        QQ.Rectangle {
            readonly property int travel: parent.width - parent.height

            x: Theme.switchThumbInset + control.visualPosition * travel
            y: Theme.switchThumbInset
            width: parent.height - 2 * Theme.switchThumbInset
            height: width
            radius: width / 2
            color: Theme.background
            border.width: control.checked ? 0 : 1
            border.color: Theme.controlBorder

            QQ.Behavior on x {
                enabled: !control.down
                QQ.NumberAnimation { duration: Theme.toggleAnimationDuration }
            }
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
