import QtQuick as QQ

import cavewherelib

// Test-only overlay for the screenshot harness: draws a highlight ring around
// `target` and dims the rest of the window, so a generated manual screenshot
// draws the reader's eye to one UI element. Mounted as a child of the window's
// content item and rendered before the grab, so it appears in the PNG.
QQ.Item {
    id: overlayId

    property QQ.Item target: null
    property bool dim: true
    property real padding: 6
    property real ringThickness: 3
    property real cornerRadius: 8

    readonly property rect targetRect: target
        ? target.mapToItem(overlayId, 0, 0, target.width, target.height)
        : Qt.rect(0, 0, 0, 0)

    // Clamp the ring inside the window, insetting by the stroke so the full
    // border stays visible even when the target is flush against an edge (the
    // side panel reaches the right and bottom of the window).
    readonly property real _inset: ringThickness
    readonly property real _left: Math.max(_inset, targetRect.x - padding)
    readonly property real _top: Math.max(_inset, targetRect.y - padding)
    readonly property real _right: Math.min(width - _inset, targetRect.x + targetRect.width + padding)
    readonly property real _bottom: Math.min(height - _inset, targetRect.y + targetRect.height + padding)

    readonly property real ringX: _left
    readonly property real ringY: _top
    readonly property real ringW: Math.max(0, _right - _left)
    readonly property real ringH: Math.max(0, _bottom - _top)

    visible: target !== null

    // Dim scrim tiled around the highlighted rect (four non-overlapping bands) so
    // the target stays at full brightness while everything else recedes.
    QQ.Item {
        anchors.fill: parent
        visible: overlayId.dim && overlayId.target !== null

        QQ.Rectangle {
            color: Theme.shadow
            x: 0
            y: 0
            width: parent.width
            height: Math.max(0, overlayId.ringY)
        }
        QQ.Rectangle {
            color: Theme.shadow
            x: 0
            y: overlayId.ringY + overlayId.ringH
            width: parent.width
            height: Math.max(0, parent.height - (overlayId.ringY + overlayId.ringH))
        }
        QQ.Rectangle {
            color: Theme.shadow
            x: 0
            y: overlayId.ringY
            width: Math.max(0, overlayId.ringX)
            height: overlayId.ringH
        }
        QQ.Rectangle {
            color: Theme.shadow
            x: overlayId.ringX + overlayId.ringW
            y: overlayId.ringY
            width: Math.max(0, parent.width - (overlayId.ringX + overlayId.ringW))
            height: overlayId.ringH
        }
    }

    QQ.Rectangle {
        x: overlayId.ringX
        y: overlayId.ringY
        width: overlayId.ringW
        height: overlayId.ringH
        radius: overlayId.cornerRadius
        color: Theme.transparent
        border.color: Theme.danger
        border.width: overlayId.ringThickness
    }
}
