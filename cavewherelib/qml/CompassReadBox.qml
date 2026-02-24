/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import cavewherelib

ReadingBox {
    id: compassReadBox
    dataValidator: CompassValidator { }

    ShotMenu {
        id: removeMenuId
        model: compassReadBox.model
        dataValue: compassReadBox.dataValue
        removePreview: compassReadBox.removePreview
    }

    rightClickMenuLoader: removeMenuId
}
