import QtQuick as QQ
import QtQuick.Templates as T
import cavewherelib

T.ProgressBar {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    contentItem: QQ.Item {
        implicitWidth: Theme.progressBarWidth
        implicitHeight: Theme.progressBarHeight
        clip: true

        // Mint to cyan with a bright leading edge. Determinate: grows from the
        // left. Indeterminate: a fixed-width bar that slides across.
        QQ.Rectangle {
            id: bar
            height: parent.height
            radius: height / 2
            width: control.indeterminate ? parent.width * Theme.progressIndeterminateFraction
                                         : parent.width * control.position
            x: control.indeterminate ? -width + (parent.width + width) * slideAnimation.progress : 0
            gradient: QQ.Gradient {
                orientation: QQ.Gradient.Horizontal
                QQ.GradientStop { position: 0.0; color: Theme.progressStart }
                QQ.GradientStop { position: Theme.progressMidPosition; color: Theme.progressMid }
                QQ.GradientStop { position: Theme.progressEndPosition; color: Theme.progressEnd }
                QQ.GradientStop { position: 1.0; color: Theme.progressLead }
            }
        }

        QQ.NumberAnimation {
            id: slideAnimation
            property real progress: 0
            target: slideAnimation
            property: "progress"
            from: 0
            to: 1
            duration: Theme.progressSlideDuration
            easing.type: QQ.Easing.InOutQuad
            loops: QQ.Animation.Infinite
            running: control.indeterminate && control.visible
        }
    }

    background: QQ.Rectangle {
        implicitWidth: Theme.progressBarWidth
        implicitHeight: Theme.progressBarHeight
        y: (control.height - height) / 2
        height: Theme.progressBarHeight
        radius: height / 2
        color: Theme.track
    }
}
