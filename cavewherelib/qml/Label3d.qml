import QtQuick
import QtQuick.Controls as QC
import cavewherelib
/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// The root item is referenced into the 3D scene by cwQuickItemSubscene via
// refFromEffectItem(), which excludes the referenced item's OWN opacity and
// transform from the rendered subtree (Qt expects the "effect" to apply them).
// So the fade animates the CHILD label's opacity, which IS part of the rendered
// subtree and therefore shows in the billboard's 3D draw. The root's opacity is
// bound to the child's purely so cwLabel3dView can drive on-demand re-renders by
// connecting to this (referenced) item's opacityChanged — the root's opacity is
// never rendered, so this binding has no visual effect of its own.
Item {
    id: root

    property alias text: labelText.text

    implicitWidth: labelText.implicitWidth
    implicitHeight: labelText.implicitHeight
    width: implicitWidth
    height: implicitHeight

    opacity: labelText.opacity

    QC.Label {
        id: labelText

        anchors.fill: parent
        color: Theme.textInverse
        style: Text.Outline
        styleColor: Theme.text
        font.pixelSize: Theme.fontSizeUI

        opacity: 0.0

        function fadeInIfVisible() {
            if(visible) {
                opacity = 0.0
                fadeIn.restart()
            }
        }

        Component.onCompleted: fadeInIfVisible()
        onVisibleChanged: fadeInIfVisible()

        NumberAnimation {
            id: fadeIn
            target: labelText
            property: "opacity"
            from: 0.0
            to: 1.0
            duration: 1000
            easing.type: Easing.OutQuad
        }
    }
}
