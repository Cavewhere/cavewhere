/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

// The glyph naming what a native survey node is: a cave, a Folder where caves
// go, or a Section (a Folder inside a cave). The choice follows from where the
// node sits, through takesCaves and isSection, so relabeling a cave turns its
// Sections into Folders without anything here reading the kind. A sourced
// node names itself with its Kind chip instead and draws no glyph.
Icon {
    id: kindIconId

    property SurveyNode node: null

    source: {
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
    visible: kindIconId.node !== null && !kindIconId.node.isSourced
    colorizationColor: Theme.textSubtle
}
