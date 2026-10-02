import QtQuick as QQ
import QtQuick.Templates as T
import QtQuick.Controls.impl
import cavewherelib

T.Slider {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitHandleWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitHandleHeight + topPadding + bottomPadding)

    padding: (Theme.sliderThickness - Theme.sliderHandleSize) / 2

    handle: QQ.Rectangle {
        x: control.leftPadding + (control.horizontal ? control.visualPosition * (control.availableWidth - width)
                                                     : (control.availableWidth - width) / 2)
        y: control.topPadding + (control.horizontal ? (control.availableHeight - height) / 2
                                                    : control.visualPosition * (control.availableHeight - height))
        implicitWidth: Theme.sliderHandleSize
        implicitHeight: Theme.sliderHandleSize
        radius: width / 2
        opacity: control.enabled ? 1 : Theme.disabledOpacity
        color: Theme.background
        border.width: control.pressed ? 3 : 2
        border.color: Theme.accent

        StyleFocusRing {
            visible: control.visualFocus
        }
    }

    background: QQ.Rectangle {
        x: control.leftPadding + (control.horizontal ? 0 : (control.availableWidth - width) / 2)
        y: control.topPadding + (control.horizontal ? (control.availableHeight - height) / 2 : 0)
        implicitWidth: control.horizontal ? Theme.sliderLength : Theme.sliderTrackHeight
        implicitHeight: control.horizontal ? Theme.sliderTrackHeight : Theme.sliderLength
        width: control.horizontal ? control.availableWidth : implicitWidth
        height: control.horizontal ? implicitHeight : control.availableHeight
        radius: Theme.sliderTrackHeight / 2
        opacity: control.enabled ? 1 : Theme.disabledOpacity
        color: Theme.track
        scale: control.horizontal && control.mirrored ? -1 : 1

        QQ.Rectangle {
            y: control.horizontal ? 0 : control.visualPosition * parent.height
            width: control.horizontal ? control.position * parent.width : Theme.sliderTrackHeight
            height: control.horizontal ? Theme.sliderTrackHeight : control.position * parent.height
            radius: Theme.sliderTrackHeight / 2
            color: Theme.accent
        }
    }
}
