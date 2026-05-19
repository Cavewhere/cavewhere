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

    anchors.fill: parent
    visible: false
    enabled: false
    focus: visible

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

        QC.RoundButton {
            id: cropButtonId
            objectName: "lazClipCropButton"
            icon.source: "qrc:/twbs-icons/icons/crop.svg"
            enabled: clipperId.canCommit
            QC.ToolTip.visible: hovered
            QC.ToolTip.text: qsTr("Crop — keep points inside the polygon")
            onClicked: clipperId.commit(LazClipInteraction.Keep)
        }

        QC.RoundButton {
            id: eraseButtonId
            objectName: "lazClipEraseButton"
            icon.source: "qrc:/twbs-icons/icons/eraser.svg"
            enabled: clipperId.canCommit
            QC.ToolTip.visible: hovered
            QC.ToolTip.text: qsTr("Erase — remove points inside the polygon")
            onClicked: clipperId.commit(LazClipInteraction.Remove)
        }

        QC.RoundButton {
            id: cancelButtonId
            objectName: "lazClipCancelButton"
            icon.source: "qrc:/twbs-icons/icons/x-lg.svg"
            // Cancel stays available during Processing as a UI affordance,
            // but the C++ commit() ignores it then — the in-flight clip
            // resolves on its own. See cwLazClipInteraction::cancel().
            QC.ToolTip.visible: hovered
            QC.ToolTip.text: qsTr("Cancel — discard the polygon")
            onClicked: clipperId.cancel()
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

    QQ.Keys.onEscapePressed: clipperId.cancel()
}
