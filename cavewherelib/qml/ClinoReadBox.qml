/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import cavewherelib

ReadingBox {
    id: clinoReadBox
    dataValidator: ClinoValidator { }

    ShotMenu {
        id: removeMenuId
        model: clinoReadBox.model
        dataValue: clinoReadBox.dataValue
        listViewIndex: clinoReadBox.listViewIndex
        removePreview: clinoReadBox.removePreview
    }

    rightClickMenuLoader: removeMenuId
}
