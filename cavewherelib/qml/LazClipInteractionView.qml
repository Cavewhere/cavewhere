/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Layouts
import cavewherelib

LazClipInteraction {
    id: clipperId

    required property BaseTurnTableInteraction turnTableInteraction

    // Remember the user's pitch-lock state so we can restore it on
    // deactivate instead of always unlocking.
    property bool _previousPitchLocked: false

    anchors.fill: parent
    visible: false
    enabled: false
    focus: visible
    // While the tool is active, sit above the always-on map overlays
    // (LeadView, LinePlotLabelView) that are declared after this view in
    // GLTerrainRenderer. Those overlays have full-fill tap-away handlers; left
    // below them, they grab taps meant for this tool's Crop/Erase/Cancel
    // buttons. z beats sibling declaration order, so this reclaims input.
    z: visible ? 1 : 0

    onActivated: {
        // Freeze the camera at whatever angle the user is currently at —
        // the clip polygon is projected from screen to the z=0 plane on
        // every click, so rotating mid-draw would silently move already-
        // placed vertices relative to the ground. Lock pitch (and the
        // forwarder below blocks right-drag rotate) for the duration.
        _previousPitchLocked = turnTableInteraction.pitchLocked
        turnTableInteraction.pitchLocked = true
    }

    onDeactivated: {
        turnTableInteraction.pitchLocked = _previousPitchLocked
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
        // Polygon vertices are projected onto z=0 at the camera angle in
        // effect when each vertex is placed — rotating mid-draw would
        // re-project the existing vertices and silently move them. Block
        // right-drag rotate (pitchLocked above blocks the plan/profile
        // buttons) until the tool deactivates.
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

    // Floating background for the Crop/Erase/Cancel toolbar. IconButton
    // renders a transparent background until hovered/selected, so without
    // this surface the icons disappear against same-colored terrain.
    ShadowRectangle {
        id: actionsContainerId
        anchors {
            bottom: parent.bottom
            bottomMargin: 64
            horizontalCenter: parent.horizontalCenter
        }
        width: actionsRowId.implicitWidth + Theme.floatingToolbarPadding
        height: actionsRowId.implicitHeight + Theme.floatingToolbarPadding
        color: Theme.surface
        radius: 5
        visible: clipperId.state === LazClipInteraction.Closed
                  || clipperId.state === LazClipInteraction.Processing

        RowLayout {
            id: actionsRowId
            anchors.centerIn: parent
            spacing: 10

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
