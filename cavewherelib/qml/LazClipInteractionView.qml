/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

LazClipInteraction {
    id: clipperId

    required property BaseTurnTableInteraction turnTableInteraction

    // Stash whether the user had the pitch locked already, so we can
    // restore that state on deactivate instead of always unlocking.
    property bool _previousPitchLocked: false

    anchors.fill: parent
    visible: false
    enabled: false
    focus: visible

    onActivated: {
        // The clip tool only works at top-down: cwLazClipInteraction
        // refuses to add polygon points otherwise. Snap the camera to
        // plan view and lock pitch so the user can't tilt off-axis via
        // the plan/profile buttons or direct pitch input while drawing.
        //
        // setPitch() early-returns when pitchLocked, so the lock must
        // stay off until the animation finishes — otherwise every
        // animated frame is dropped. pitchSnapAnimation.onFinished
        // re-engages the lock.
        _previousPitchLocked = turnTableInteraction.pitchLocked
        turnTableInteraction.pitchLocked = false
        pitchSnapAnimation.to = 90.0
        pitchSnapAnimation.restart()
    }

    onDeactivated: {
        // Stop a mid-flight snap so it doesn't keep dragging pitch
        // toward 90 after the tool is gone.
        pitchSnapAnimation.stop()
        turnTableInteraction.pitchLocked = _previousPitchLocked
    }

    QQ.NumberAnimation {
        id: pitchSnapAnimation
        target: clipperId.turnTableInteraction
        property: "pitch"
        duration: 200
        easing.type: QQ.Easing.InOutQuad
        onFinished: clipperId.turnTableInteraction.pitchLocked = true
    }

    // acceptedButtons: NoButton lets clicks fall through to the TapHandler.
    QQ.MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        cursorShape: Qt.CrossCursor
        hoverEnabled: true
    }

    QQ.TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: (eventPoint) => clipperId.addPoint(eventPoint.position)
        onDoubleTapped: (eventPoint) => clipperId.closePolygon()
    }

    QQ.HoverHandler {
        id: hoverHandlerId
        onPointChanged: {
            polygonCanvasId.hoverScreenPos = point.position
            polygonCanvasId.snapActive = clipperId.isNearFirstPoint(point.position)
        }
    }

    TurnTableForwardingHandlers {
        turnTableInteraction: clipperId.turnTableInteraction
        // Top-down ortho is a hard requirement for screen→world ray casting
        // in cwLazClipInteraction (see header). Rotating away from top-down
        // would silently break the clip; suppress right-drag rotation here.
        rotationEnabled: false
    }

    LazClipPolygonCanvas {
        id: polygonCanvasId
        anchors.fill: parent
        // QCanvasPainterItem inherits QQuickRhiItem, which composites
        // opaquely by default — without alphaBlending the item's color
        // buffer overwrites the GL terrain behind us. Pair it with a
        // transparent clear color so only the painted polygon shows up.
        alphaBlending: true
        fillColor: "transparent"
        interaction: clipperId
        polygonOutlineColor: Theme.accent
        polygonFillColor: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
        polygonVertexBorderColor: Theme.text
    }

    QQ.Row {
        id: actionsRowId
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 64
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 10
        visible: clipperId.state === LazClipInteraction.Closed
                  || clipperId.state === LazClipInteraction.Processing

        // Crop/Erase are fire-and-forget: failures surface via
        // cwProject::errorModel(), not an in-tool banner.
        IconButton {
            id: cropButtonId
            objectName: "lazClipCropButton"
            iconSource: "qrc:/twbs-icons/icons/crop.svg"
            sourceSize: Qt.size(21, 21)
            text: qsTr("Crop")
            toolTip: qsTr("Crop — keep points inside the polygon")
            enabled: clipperId.canCommit
            onClicked: {
                clipperId.commit(LazClipInteraction.Keep)
                clipperId.deactivate()
            }
        }

        IconButton {
            id: eraseButtonId
            objectName: "lazClipEraseButton"
            iconSource: "qrc:/twbs-icons/icons/eraser.svg"
            sourceSize: Qt.size(21, 21)
            text: qsTr("Erase")
            toolTip: qsTr("Erase — remove points inside the polygon")
            enabled: clipperId.canCommit
            onClicked: {
                clipperId.commit(LazClipInteraction.Remove)
                clipperId.deactivate()
            }
        }

        IconButton {
            id: cancelButtonId
            objectName: "lazClipCancelButton"
            iconSource: "qrc:/twbs-icons/icons/x-lg.svg"
            sourceSize: Qt.size(21, 21)
            text: qsTr("Cancel")
            toolTip: qsTr("Cancel — discard the polygon and exit")
            onClicked: clipperId.deactivate()
        }
    }

    HelpBox {
        id: helpBoxId
        objectName: "lazClipHelpBox"
        anchors.top: parent.top
        anchors.topMargin: 20
        anchors.bottom: undefined
        visible: clipperId.errorMessage === ""
                  && clipperId.state !== LazClipInteraction.Processing

        text: {
            switch (clipperId.state) {
            case LazClipInteraction.Idle:
                return qsTr("Click to start the clip polygon.")
            case LazClipInteraction.Drawing:
                if (clipperId.pointCount < 3) {
                    return qsTr("Click to add more vertices. <b>3 minimum.</b>")
                }
                return qsTr("Click near the first vertex (or double-click) to close.")
            case LazClipInteraction.Closed:
                return qsTr("Choose <b>Crop</b> or <b>Erase</b>, or cancel.")
            }
            return ""
        }
    }

    ErrorHelpBox {
        id: errorBoxId
        objectName: "lazClipErrorBox"
        anchors.top: parent.top
        anchors.topMargin: 20
        anchors.bottom: undefined
        visible: clipperId.errorMessage !== ""
        text: clipperId.errorMessage
    }

    HelpBox {
        id: processingBoxId
        anchors.top: parent.top
        anchors.topMargin: 20
        anchors.bottom: undefined
        visible: clipperId.state === LazClipInteraction.Processing
        text: qsTr("Clipping…")
    }

    // deactivate() emits the deactivated signal that InteractionManager
    // listens on, which clears activeInteraction and restores the default
    // (turn-table) interaction. The C++ onDeactivated slot also runs and
    // calls cancel() to drop any in-progress polygon and error state, so
    // a plain deactivate() here fully exits the tool — no need to call
    // cancel() separately.
    QQ.Keys.onEscapePressed: clipperId.deactivate()
}
