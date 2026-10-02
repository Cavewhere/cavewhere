import QtQuick as QQ
import QtQuick.Templates as T
import cavewherelib

T.BusyIndicator {
    id: control

    // The droplet opening and the expanding close read only on a large ring; a
    // small one fades in and out spinning.
    readonly property bool dropletEnabled: Math.min(width, height) >= Theme.busyDropletMinimumSize

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: 0

    function settle() {
        openAnimation.stop()
        closeAnimation.stop()
        drop.opacity = 0
        firstRipple.opacity = 0
        secondRipple.opacity = 0
        ringLayer.opacity = 1
        ringLayer.scale = 1
    }

    function start() {
        if (dropletEnabled) {
            closeAnimation.stop()
            openAnimation.restart()
        } else {
            settle()
        }
    }

    function finish() {
        if (dropletEnabled) {
            openAnimation.stop()
            drop.opacity = 0
            firstRipple.opacity = 0
            secondRipple.opacity = 0
            closeAnimation.restart()
        } else {
            settle()
        }
    }

    // An indicator created running shows the ring at once; the droplet marks a
    // start that happens while it is on screen.
    onRunningChanged: running ? start() : finish()
    // A layout can shrink the indicator below the droplet size after it starts.
    onDropletEnabledChanged: if (!dropletEnabled) settle()

    contentItem: QQ.Item {
        implicitWidth: Theme.busyIndicatorSize
        implicitHeight: Theme.busyIndicatorSize
        opacity: control.running ? 1 : 0

        QQ.Behavior on opacity {
            id: fadeBehavior

            // The close fades with the ring's expansion when the droplet plays.
            QQ.NumberAnimation {
                duration: fadeBehavior.targetValue > 0 || !control.dropletEnabled
                          ? Theme.busyFadeDuration : Theme.busyRingExitDuration
            }
        }

        QQ.Item {
            id: ringLayer

            anchors.fill: parent

            BusyRing {
                objectName: "busyRing"
                anchors.centerIn: parent
                width: Math.min(parent.width, parent.height)
                height: width
                startColor: Theme.progressStart
                midColor: Theme.progressMid
                endColor: Theme.progressEnd
                leadColor: Theme.progressLead

                // Keeps turning while the close fades it out. A RotationAnimation
                // rather than an animator, whose value reaches QML only when it
                // stops: tests and bindings read the angle as it turns.
                QQ.RotationAnimation on rotation {
                    from: 0
                    to: 360
                    duration: Theme.busySpinDuration
                    easing.type: QQ.Easing.Linear
                    loops: QQ.Animation.Infinite
                    running: control.visible && (control.running || ringLayer.parent.opacity > 0)
                }
            }
        }

        QQ.Rectangle {
            id: firstRipple

            anchors.centerIn: parent
            width: Math.min(parent.width, parent.height)
            height: width
            radius: width / 2
            color: Theme.transparent
            border.width: Theme.busyRippleBorderWidth
            border.color: Theme.progressMid
            opacity: 0
        }

        QQ.Rectangle {
            id: secondRipple

            anchors.centerIn: parent
            width: firstRipple.width
            height: width
            radius: width / 2
            color: Theme.transparent
            border.width: Theme.busyRippleBorderWidth
            border.color: Theme.progressMid
            opacity: 0
        }

        QQ.Rectangle {
            id: drop

            x: (parent.width - width) / 2
            width: Theme.busyDropSize
            height: width
            radius: width / 2
            color: Theme.progressLead
            opacity: 0
        }

        // A drop falls to the center, two ripples spread from where it lands,
        // and the ring grows in behind the second ripple.
        QQ.SequentialAnimation {
            id: openAnimation

            QQ.PropertyAction { target: ringLayer; property: "opacity"; value: 0 }
            QQ.PropertyAction { target: ringLayer; property: "scale"; value: Theme.busyRingEnterScale }
            QQ.PropertyAction { targets: [firstRipple, secondRipple]; property: "opacity"; value: 0 }
            QQ.PropertyAction { target: drop; property: "opacity"; value: 1 }

            QQ.NumberAnimation {
                target: drop
                property: "y"
                from: 0
                to: (drop.parent.height - drop.height) / 2
                duration: Theme.busyDropFallDuration
                easing.type: QQ.Easing.InQuad
            }

            QQ.PropertyAction { target: drop; property: "opacity"; value: 0 }

            QQ.ParallelAnimation {
                QQ.ParallelAnimation {
                    QQ.NumberAnimation {
                        target: firstRipple
                        property: "scale"
                        from: Theme.busyRippleStartScale
                        to: 1
                        duration: Theme.busyRippleDuration
                        easing.type: QQ.Easing.OutQuad
                    }
                    QQ.NumberAnimation {
                        target: firstRipple
                        property: "opacity"
                        from: Theme.busyRippleStartOpacity
                        to: 0
                        duration: Theme.busyRippleDuration
                        easing.type: QQ.Easing.OutQuad
                    }
                }

                QQ.SequentialAnimation {
                    QQ.PauseAnimation { duration: Theme.busyRippleStagger }
                    QQ.ParallelAnimation {
                        QQ.NumberAnimation {
                            target: secondRipple
                            property: "scale"
                            from: Theme.busyRippleStartScale
                            to: 1
                            duration: Theme.busyRippleDuration
                            easing.type: QQ.Easing.OutQuad
                        }
                        QQ.NumberAnimation {
                            target: secondRipple
                            property: "opacity"
                            from: Theme.busyRippleStartOpacity
                            to: 0
                            duration: Theme.busyRippleDuration
                            easing.type: QQ.Easing.OutQuad
                        }
                    }
                }

                QQ.SequentialAnimation {
                    QQ.PauseAnimation { duration: Theme.busyRippleStagger }
                    QQ.ParallelAnimation {
                        QQ.NumberAnimation {
                            target: ringLayer
                            property: "scale"
                            from: Theme.busyRingEnterScale
                            to: 1
                            duration: Theme.busyRingEnterDuration
                        }
                        QQ.NumberAnimation {
                            target: ringLayer
                            property: "opacity"
                            from: 0
                            to: 1
                            duration: Theme.busyRingEnterDuration
                        }
                    }
                }
            }
        }

        // The content's fade carries the opacity; this grows the ring with it.
        QQ.NumberAnimation {
            id: closeAnimation

            target: ringLayer
            property: "scale"
            to: Theme.busyRingExitScale
            duration: Theme.busyRingExitDuration
        }
    }
}
