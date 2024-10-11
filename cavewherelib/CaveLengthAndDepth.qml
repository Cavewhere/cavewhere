/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ
import cavewherelib

QQ.Column {
    property Cave currentCave: null

    CaveLengthComponent {
        id: caveLengthId
        text: "Length:"
        unitValue: currentCave !== null ? currentCave.length : null
        unitModel: UnitDefaults.lengthModel
    }

    CaveLengthComponent {
        id: caveDepth
        text: "Depth:"
        unitValue: currentCave !== null ? currentCave.depth : null
        unitModel: UnitDefaults.depthModel
    }

}
