/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC

// A menu entry that a QC.Menu carries only while \a active is true.
//
// A MenuItem that hides itself keeps the place the menu laid it out in, so
// the entries below it answer for the wrong verb. Qt's own answer is to add
// and remove the item instead, which is what this does: declare one of these
// inside the menu it belongs to and name that menu.
QQ.Instantiator {
    id: conditionalItemId

    //The menu the item joins and leaves.
    required property QC.Menu menu

    //Where in the menu the item goes; -1 puts it last.
    property int insertIndex: -1

    //The item's own label and name, since the item itself is made here.
    property string text
    property string itemObjectName

    signal triggered()

    delegate: QC.MenuItem {
        objectName: conditionalItemId.itemObjectName
        text: conditionalItemId.text

        onTriggered: conditionalItemId.triggered()
    }

    onObjectAdded: (index, object) => {
        const item = object as QC.MenuItem;
        if(conditionalItemId.insertIndex >= 0) {
            conditionalItemId.menu.insertItem(conditionalItemId.insertIndex, item);
        } else {
            conditionalItemId.menu.addItem(item);
        }
    }

    onObjectRemoved: (index, object) => conditionalItemId.menu.removeItem(object as QC.MenuItem)
}
