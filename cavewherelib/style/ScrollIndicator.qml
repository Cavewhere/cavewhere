import QtQuick as QQ
import QtQuick.Templates as T
import cavewherelib

T.ScrollIndicator {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: Theme.scrollBarPadding

    contentItem: QQ.Rectangle {
        implicitWidth: Theme.scrollIndicatorThickness
        implicitHeight: Theme.scrollIndicatorThickness
        radius: Math.min(width, height) / 2
        color: Theme.scrollHandle
        visible: control.size < 1.0
        opacity: 0.0

        states: QQ.State {
            name: "active"
            when: control.active
            QQ.PropertyChanges { control.contentItem.opacity: Theme.scrollHandleOpacity }
        }

        transitions: [
            QQ.Transition {
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
        ]
    }
}
