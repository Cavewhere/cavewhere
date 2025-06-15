/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

QQ.Column {
    id: itemId

    property Cave currentCave: null

    CaveLengthComponent {
        id: caveLengthId
        text: "Length:"
        unitValue: itemId.currentCave !== null ? itemId.currentCave.length : null
        unitModel: UnitDefaults.lengthModel
    }

    CaveLengthComponent {
        id: caveDepth
        text: "Depth:"
        unitValue: itemId.currentCave !== null ? itemId.currentCave.depth : null
        unitModel: UnitDefaults.depthModel
    }

}
