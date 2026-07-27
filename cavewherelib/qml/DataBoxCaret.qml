/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

// The focus-activated caret a survey-table cell puts in its top-right corner to
// offer cell-level actions — excluding a distance, fixing a station. A cell
// declares one with its menu and an active condition, so the affordance itself
// (corner, margin, resting opacity, focus policy) has one definition.
//
// Deferred twice on purpose: the button exists only while its cell is active,
// and ContextMenuButton defers the menu again until the caret is clicked.
QQ.Loader {
    id: caretId

    // The cell's menu. A Component, so its items aren't built until the caret is
    // clicked — every cell would otherwise pay for a menu nobody opens.
    required property QQ.Component menu

    // Names the button, not this loader: the loader has no visual presence, and
    // it's the button that tests and accessibility need to find.
    property string buttonObjectName: ""

    readonly property int cornerMargin: 3
    readonly property real restingOpacity: 0.75

    anchors.right: parent.right
    anchors.top: parent.top
    anchors.margins: caretId.cornerMargin

    sourceComponent: ContextMenuButton {
        objectName: caretId.buttonObjectName

        focusPolicy: Qt.NoFocus

        opacity: state === "" ? caretId.restingOpacity : 1.0

        menu: caretId.menu
    }
}
