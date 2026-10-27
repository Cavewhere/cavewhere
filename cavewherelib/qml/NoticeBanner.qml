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

// The shape every in-panel notice shares: a rounded Theme-colored card with a
// bold title and whatever the notice wants to say under it.
//
// The caller supplies the color, the title and the body; the card's corner,
// margins, spacing and height come from here, so all the notices in a panel
// keep agreeing with each other.
QQ.Rectangle {
    id: root

    // Corner shared by every notice card.
    readonly property real cornerRadius: 5

    property string title: ""

    // Name the title label answers to in findChild(), so each notice keeps its
    // own handle on it.
    property string titleObjectName: ""

    default property alias content: contentLayoutId.data

    radius: cornerRadius
    implicitHeight: bannerLayoutId.implicitHeight + Theme.sectionSpacing * 2

    ColumnLayout {
        id: bannerLayoutId
        anchors.fill: parent
        anchors.margins: Theme.sectionSpacing
        spacing: Theme.tightSpacing

        QC.Label {
            objectName: root.titleObjectName
            Layout.fillWidth: true
            font.bold: true
            wrapMode: QC.Label.WordWrap
            text: root.title
        }

        ColumnLayout {
            id: contentLayoutId
            Layout.fillWidth: true
            spacing: Theme.tightSpacing
        }
    }
}
