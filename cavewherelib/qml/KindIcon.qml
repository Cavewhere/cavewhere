/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

// The glyph naming what a survey row is: a trip, or for a native node a cave,
// a Folder where caves go, or a Section (a Folder inside a cave). A node's
// choice follows from where it sits, through takesCaves and isSection, so
// relabeling a cave turns its Sections into Folders without anything here
// reading the kind. A sourced node names itself with its Kind chip instead and
// draws no glyph; every trip draws one, sourced or native.
Icon {
    id: kindIconId

    property SurveyNode node: null
    property Trip trip: null

    source: {
        if (kindIconId.trip !== null) {
            return Theme.kindIconTrip;
        }
        if (kindIconId.node === null) {
            return "";
        }
        if (kindIconId.node.takesCaves) {
            return Theme.kindIconFolder;
        }
        if (kindIconId.node.isSection) {
            return Theme.kindIconSection;
        }
        return Theme.kindIconCave;
    }
    visible: kindIconId.trip !== null
             || (kindIconId.node !== null && !kindIconId.node.isSourced)
    colorizationColor: Theme.textSubtle
}
