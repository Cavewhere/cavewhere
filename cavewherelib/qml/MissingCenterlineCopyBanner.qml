/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

// "The attached file is gone from the project" — question 1 of
// plans/EXTERNAL_FILE_LIVE_LINK_RETIREMENT.html §7.
//
// The in-project copy is the only file the project reads, so its absence
// silences everything else about the attachment: no stations, no scan, no
// cavern complaint naming it. Picking the file again is the whole fix, which
// is why this is the one banner that offers Replace… itself.
NoticeBanner {
    id: root
    objectName: "missingCenterlineCopyBanner"

    // Project-relative path of the file that should be there, from
    // ExternalCenterlineManager.missingCopyPath(). Empty hides the banner.
    property string missingPath: ""

    signal replaceRequested()

    visible: missingPath.length > 0
    color: Theme.errorBackground
    title: qsTr("This survey's file is missing")
    titleObjectName: "missingCopyTitle"

    BodyText {
        objectName: "missingCopyDetail"
        Layout.fillWidth: true
        wrapMode: QC.Label.WordWrap
        font.pixelSize: Theme.fontSizeSmall
        text: qsTr("%1 is gone from the project folder, so this survey can't be read. Pick the file again to put it back.")
              .arg(root.missingPath)
    }

    QC.Button {
        objectName: "missingCopyReplaceButton"
        Layout.topMargin: Theme.tightSpacing
        text: qsTr("Replace…")
        onClicked: root.replaceRequested()
    }
}
