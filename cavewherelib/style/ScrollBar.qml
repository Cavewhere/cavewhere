import QtQuick as QQ
import QtQuick.Templates as T
import cavewherelib

T.ScrollBar {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: Theme.scrollBarPadding
    visible: control.policy !== T.ScrollBar.AlwaysOff
    minimumSize: {
        const length = control.orientation === Qt.Horizontal ? control.width : control.height
        return length > 0 ? Math.min(1, Theme.scrollBarMinimumLength / length) : 0
    }

    // The handle overlays the content and has no track behind it.
    contentItem: QQ.Rectangle {
        implicitWidth: control.interactive ? Theme.scrollBarThickness : Theme.scrollIndicatorThickness
        implicitHeight: control.interactive ? Theme.scrollBarThickness : Theme.scrollIndicatorThickness
        radius: Math.min(width, height) / 2
        color: Theme.scrollHandle
        opacity: 0.0

        states: QQ.State {
            name: "active"
            when: control.policy === T.ScrollBar.AlwaysOn || (control.active && control.size < 1.0)
            QQ.PropertyChanges {
                control.contentItem.opacity: {
                    if (control.pressed) {
                        return 1.0
                    }
                    return control.hovered ? Theme.scrollHandleHoverOpacity : Theme.scrollHandleOpacity
                }
            }
        }

        transitions: QQ.Transition {
            from: "active"
            QQ.SequentialAnimation {
                QQ.PauseAnimation { duration: Theme.scrollFadeDelay }
                QQ.NumberAnimation {
                    target: control.contentItem
                    duration: Theme.scrollFadeDuration
                    property: "opacity"
                    to: 0.0
                }
            }
        }
    }
}
