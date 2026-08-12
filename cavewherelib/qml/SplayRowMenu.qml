/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// The menu one splay row pops, from its ⋯ or from a right click. Moving arms
// the splay and waits for a station to be clicked; removing happens at once,
// since the cluster it came out of is right there to see and a bad shot out of
// a download is cheap to lose.
//
// A cluster of thirty splays is thirty rows the view builds and recycles as it
// scrolls, and a QC.Menu is a popup with a list inside it — far more than the
// row shows. So the menu waits behind this Loader until something asks for it,
// the way StationMenu waits behind its own.
QQ.Loader {
    id: splayRowMenuLoader
    objectName: "splayRowMenu"

    required property SurveyEditorModel model
    required property cwSurveyEditorRowIndex rowIndex

    //The menu once it has been asked for, and null until then
    readonly property QC.Menu menu: splayRowMenuLoader.item as QC.Menu

    active: false

    //Builds the menu if this is the first time it's been asked for, then pops
    //it under the pointer
    function popup() {
        splayRowMenuLoader.active = true
        splayRowMenuLoader.item.popup()
    }

    sourceComponent: QQ.Component {
        QC.Menu {
            objectName: "splayRowMenuRoot"

            QC.MenuItem {
                objectName: "moveSplayMenuItem"
                text: "Move to…"
                onTriggered: splayRowMenuLoader.model.startSplayMove(splayRowMenuLoader.rowIndex, false)
            }

            QC.MenuItem {
                objectName: "removeSplayMenuItem"
                text: "Remove splay"
                onTriggered: splayRowMenuLoader.model.removeSplayAt(splayRowMenuLoader.rowIndex)
            }
        }
    }
}
