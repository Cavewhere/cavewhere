/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ
import Cavewhere 1.0

QQ.Row {
    id: caveLengthId

    property alias text: textId.text
    property var unitValue: null
    property alias unitModel: lengthId.unitModel

    spacing: 5

    Text {
        id: textId
    }

    UnitValueInput {
        id: lengthId
        valueReadOnly: true
        unitValue: caveLengthId.unitValue
    }
}
