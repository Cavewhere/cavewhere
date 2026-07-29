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
QQ.Rectangle {
    id: root
    objectName: "externalCenterlineFileErrorBanner"

    // Cavern's text, verbatim. Empty for a file it read without complaint,
    // which is what hides the banner.
    property string errorMessage: ""

    visible: errorMessage.length > 0
    color: Theme.errorBackground
    radius: 5
    implicitHeight: contentLayoutId.implicitHeight + Theme.sectionSpacing * 2

    ColumnLayout {
        id: contentLayoutId
        anchors.fill: parent
        anchors.margins: Theme.sectionSpacing
        spacing: Theme.tightSpacing

        QC.Label {
            objectName: "fileErrorTitle"
            Layout.fillWidth: true
            font.bold: true
            wrapMode: QC.Label.WordWrap
            text: qsTr("This file has errors")
        }

        BodyText {
            objectName: "fileErrorMessage"
            Layout.fillWidth: true
            wrapMode: QC.Label.WordWrap
            font.pixelSize: Theme.fontSizeSmall
            text: root.errorMessage
        }
    }
}
