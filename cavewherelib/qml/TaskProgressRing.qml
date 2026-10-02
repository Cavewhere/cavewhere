/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// The app's busy mark, wherever it appears: the sidebar footer at Wide and
// Medium, and the top bar chip at Narrow. One component so the three surfaces
// cannot drift into three different marks.
//
// It spins whenever nothing can say how far along it is — which today is most
// jobs, since only point-cloud loads, clips, picking rebuilds and Compass
// imports report step counts. Drawing an arc for the rest would mean drawing one
// that has to jump.
//
// progress and count default to the shared ActiveTasks state and stay
// overridable, the same shape SideBarUpdateFooter uses, so a test can drive the
// ring directly without inventing jobs.
QQ.Item {
    id: ringId

    property real progress: ActiveTasks.progress
    property int count: ActiveTasks.count

    // The count only goes inside the ring where nothing else carries it — at
    // Medium the footer's "Running 3 Tasks" label is hidden, at Wide it is not.
    property bool showCount: false

    // What the arc is really drawn at. The aggregate legitimately runs backward
    // — jobs leave the model when they finish, so two at 0.9 and 0.1 read 0.50
    // and then 0.10 the moment the first one lands — and an arc that jumps
    // backward reads as a glitch, so it slides to the new number instead. The
    // model goes on reporting the truth; only the drawing is smoothed.
    //
    // Losing the number is not a drop to zero, so the indeterminate sentinel is
    // taken whole at both ends. Driven by hand rather than by a Behavior because
    // the rule depends on where the arc already was, which is exactly what a
    // Behavior's enabled cannot see at the moment it is read.
    property real easedProgress: -1

    // One turn of the indeterminate spin, and how long the arc takes to travel
    // to a new number. Both are this component's own timing rather than Theme
    // tokens: nothing else reads them, and a shared file is the wrong place for
    // a constant with one consumer.
    readonly property int spinDuration: 1100
    readonly property int easeDuration: 250

    implicitWidth: Theme.iconSizeSmall
    implicitHeight: Theme.iconSizeSmall

    onProgressChanged: {
        easeAnimationId.stop()

        if (ringId.easedProgress < 0 || ringId.progress < 0) {
            ringId.easedProgress = ringId.progress
            return
        }

        easeAnimationId.from = ringId.easedProgress
        easeAnimationId.to = ringId.progress
        easeAnimationId.start()
    }

    QQ.Component.onCompleted: ringId.easedProgress = ringId.progress

    QQ.NumberAnimation {
        id: easeAnimationId
        target: ringId
        property: "easedProgress"
        duration: ringId.easeDuration
    }

    ProgressRing {
        id: canvasId
        objectName: "progressRingCanvas"
        anchors.fill: parent

        progress: ringId.easedProgress
        arcColor: Theme.accent
        trackColor: Theme.progressRingTrack

        // Stops on its own once a job reports progress, since the ring is then
        // drawing a real number and the spin would sweep it away. Zero counts as
        // nothing to draw rather than as a number: there is no arc at 0%, and a
        // still ring reads as work that has stalled.
        QQ.NumberAnimation on spinAngle {
            running: ringId.progress <= 0
            loops: QQ.Animation.Infinite
            from: 0
            to: 360
            duration: ringId.spinDuration
        }
    }

    QC.Label {
        objectName: "progressRingCount"
        anchors.centerIn: parent

        visible: ringId.showCount && ringId.count > 0
        text: ringId.count
        color: canvasId.arcColor
        font.pixelSize: Theme.progressRingCountFontSize
        font.bold: true
    }
}
