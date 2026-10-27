/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

// Cavern's own complaint about one attachment, from the scan's per-file
// harvest (plans/EXTERNAL_FILE_SCAN_STATION_HARVEST_PLAN.html §3.7).
//
// This is the only place a broken file is named: the region solve reads every
// attachment as one run and fails with one log, so it can say the region is
// broken but not which file broke it. The harvest reads this file alone, right
// after the attach and on every edit of it.
NoticeBanner {
    id: root
    objectName: "externalCenterlineFileErrorBanner"

    // Cavern's text, verbatim. Empty for a file it read without complaint,
    // which is what hides the banner.
    property string errorMessage: ""

    // Cavern reports every complaint in a file, not the first, and a badly
    // broken one runs to dozens of lines. Past this the log scrolls instead of
    // growing, so it can't push the panel's Replace button — the way out of
    // the state the log is describing — off the bottom.
    readonly property real maximumMessageHeight: 120

    visible: errorMessage.length > 0
    color: Theme.errorBackground
    title: qsTr("This file has errors")
    titleObjectName: "fileErrorTitle"

    QQ.Flickable {
        id: messageAreaId
        objectName: "fileErrorMessageArea"

        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(messageId.implicitHeight,
                                         root.maximumMessageHeight)

        contentWidth: width
        contentHeight: messageId.implicitHeight
        clip: true
        boundsBehavior: QQ.Flickable.StopAtBounds

        QC.ScrollBar.vertical: QC.ScrollBar {
            policy: messageAreaId.contentHeight > messageAreaId.height
                    ? QC.ScrollBar.AlwaysOn
                    : QC.ScrollBar.AlwaysOff
        }

        BodyText {
            id: messageId
            objectName: "fileErrorMessage"

            width: messageAreaId.width
            wrapMode: QC.Label.WordWrap
            font.pixelSize: Theme.fontSizeSmall
            text: root.errorMessage
        }
    }
}
