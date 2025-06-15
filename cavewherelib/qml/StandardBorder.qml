/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick

BorderImage {
    source: "qrc:icons/border.png"
    anchors.fill: parent;
    border { left: 5; right: 5; top: 5; bottom: 5 }
    horizontalTileMode: BorderImage.Repeat
    verticalTileMode: BorderImage.Repeat
}
