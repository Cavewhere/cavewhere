/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/
pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Controls as QC
import cavewherelib

QQ.Rectangle {
    id: button

    property alias text: textLabel.text;
    property alias image: icon.source;
    property bool troggled: false;
    property int buttonIndex;
    property int layout: Qt.Vertical
    property bool compactMode: false
    property alias imageSize: icon.sourceSize
    property alias layoutDirection: columnLayoutId.layoutDirection

    anchors.left: parent.left;
    anchors.right: parent.right
    height: columnLayoutId.height + 10
    color: Theme.transparent

    //Called when troggle is true
    signal buttonIsTroggled()

    // The current page's button takes the selected-row tint; an idle button
    // recedes until the pointer reaches it.
    QQ.Rectangle {
        id: buttonFaceId
        objectName: "buttonFace"
        anchors.fill: parent
        // Lines the face up with the tool rail card below the buttons.
        anchors.margins: Theme.toolRailPanelInset
        radius: Theme.panelRadius
        border.width: 1
        color: button.troggled ? Theme.highlight
                               : hoverHandler.hovered ? Theme.hoverOverlay : Theme.transparent
        border.color: button.troggled ? Theme.accentMuted
                                      : hoverHandler.hovered ? Theme.border : Theme.transparent
    }

    GridLayout {
        id: columnLayoutId
        anchors.centerIn: parent
        opacity: button.troggled || hoverHandler.hovered ? 1.0 : Theme.sidebarIdleOpacity

        columns: layout == Qt.Vertical ? 1 : 2
        // rows: columnLayout ? 2 : 1

        QQ.Image {
            id: icon
            objectName: "icon"
            smooth: true

            // height: Qt.Vertical ? 32 : 10
            // width: Qt.V

            sourceSize: button.compactMode
                       ? Qt.size(Theme.iconSizeSmall, Theme.iconSizeSmall)
                       : layout == Qt.Vertical
                         ? Qt.size(Theme.iconSizeMedium, Theme.iconSizeMedium)
                         : Qt.size(textLabel.height, textLabel.height)

            // visible: false

            layer.enabled: true
            layer.effect: QQ.ShaderEffect {
                property QQ.color pixelColor: textLabel.color
                fragmentShader: "qrc:/shaders/toColor.frag.qsb"
            }
        }

        QC.Label {
            id: textLabel
            objectName: "textLabel"
            color: Theme.sidebar.text
            text: "text"
            smooth: true
            font.bold: true
            font.pixelSize: button.compactMode ? Theme.fontSizeSmall : Theme.fontSizeMedium
            Layout.alignment: Qt.AlignHCenter
        }
    }

    QQ.TapHandler {
        gesturePolicy: QQ.TapHandler.ReleaseWithinBounds
        onSingleTapped: button.buttonIsTroggled();
    }

    QQ.HoverHandler {
        id: hoverHandler
    }
}
