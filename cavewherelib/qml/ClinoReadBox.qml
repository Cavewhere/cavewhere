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
        dataValue: clinoReadBox.dataValue
        removePreview: clinoReadBox.removePreview
    }

    rightClickMenu: removeMenuId.item
}
